/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ip_main.c,v 8.13 2000/06/28 20:20:37 skimo Exp $ (Berkeley) $Date: 2000/06/28 20:20:37 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/uio.h>
#include <pthread.h>

#include "../common/common.h"
#include "../ipc/ip.h"
#include "extern.h"

GS *__global_list;				/* GLOBAL: List of screens. */

static void	   ip_func_std __P((GS *));
static IP_PRIVATE *ip_init __P((WIN *wp, int i_fd, int o_fd, int argc, char *argv[]));
static void	   perr __P((char *, char *));
static void run_editor __P((void * vp));

/*
 * ip_main --
 *      This is the main loop for the vi-as-library editor.
 *
 * PUBLIC: int ip_main __P((int, char *[], GS *, char *));
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	IP_PRIVATE *ipp;
	int rval;
	char *ip_arg;
	char **p_av, **t_av;
	GS *gp;
	WIN *wp;
	int i_fd, o_fd, main_ifd, main_ofd;
	char *ep;

	/* Create and initialize the global structure. */
	__global_list = gp = gs_init(argv[0]);

	/*
	 * Strip out any arguments that vi isn't going to understand.  There's
	 * no way to portably call getopt twice, so arguments parsed here must
	 * be removed from the argument list.
	 */
	ip_arg = NULL;
	for (p_av = t_av = argv;;) {
		if (*t_av == NULL) {
			*p_av = NULL;
			break;
		}
		if (!strcmp(*t_av, "--")) {
			while ((*p_av++ = *t_av++) != NULL);
			break;
		}
		if (!memcmp(*t_av, "-I", sizeof("-I") - 1)) {
			if (t_av[0][2] != '\0') {
				ip_arg = t_av[0] + 2;
				++t_av;
				--argc;
				continue;
			}
			else if (t_av[1] != NULL) {
				ip_arg = t_av[1];
				t_av += 2;
				argc -= 2;
				continue;
			}
		}
		*p_av++ = *t_av++;
	}

	/*
	 * Crack ip_arg -- it's of the form #.#, where the first number is the
	 * file descriptor from the screen, the second is the file descriptor
	 * to the screen.
	 */
	if (!isdigit(ip_arg[0]))
		goto usage;
	i_fd = strtol(ip_arg, &ep, 10);
	if (ep[0] != '.' || !isdigit(ep[1]))
		goto usage;
	o_fd = strtol(++ep, &ep, 10);
	if (ep[0] != '\0') {
usage:		ip_usage();
		return (NULL);
	}

		/* Create new window */
		wp = gs_new_win(gp);

	/* Create and partially initialize the IP structure. */
	if ((ipp = ip_init(wp, i_fd, o_fd, argc, argv)) == NULL)
		return (1);

	run_editor((void *)wp);

	/* Free the global and IP private areas. */
#if defined(DEBUG) || defined(PURIFY) || defined(LIBRARY)
	free(gp);
#endif
	exit (rval);
}

static void 
run_editor(void * vp)
{
	GS *gp;
	IP_PRIVATE *ipp;
	WIN *wp;
	EVENT ev;
	int rval;
	IP_BUF ipb;

	wp = (WIN *) vp;
	gp = wp->gp;
	ipp = wp->ip_private;

	/* Add the terminal type to the global structure. */
	if ((OG_D_STR(gp, GO_TERM) =
	    OG_STR(gp, GO_TERM) = strdup("ip_curses")) == NULL)
		perr(gp->progname, NULL);

	/*
	 * Figure out how big the screen is -- read events until we get
	 * the rows and columns.
	 */
	for (;;) {
		if (ip_wevent(wp, NULL, &ev, 0, 0))
			return;
		if (ev.e_event == E_WRESIZE)
			break;
		if (ev.e_event == E_EOF || ev.e_event == E_ERR ||
		    ev.e_event == E_SIGHUP || ev.e_event == E_SIGTERM)
			return;
		if (ev.e_event == E_IPCOMMAND && ev.e_ipcom == VI_QUIT)
			return;
	}

	/* Run ex/vi. */
	rval = editor(wp, ipp->argc, ipp->argv);

	/* Clean up the screen. */
	(void)ip_quit(wp);

	/* Send the quit message. */
	ipb.code = SI_QUIT;
	(void)vi_send(ipp->o_fd, NULL, &ipb);

	/* Give the screen a couple of seconds to deal with it. */
	sleep(2);

#if defined(DEBUG) || defined(PURIFY) || defined(LIBRARY)
	free(ipp);
#endif
}

/*
 * ip_init --
 *	Create and partially initialize the GS structure.
 */
static IP_PRIVATE *
ip_init(WIN *wp, int i_fd, int o_fd, int argc, char *argv[])
{
	IP_PRIVATE *ipp;

	/* Allocate the IP private structure. */
	CALLOC_NOMSG(NULL, ipp, IP_PRIVATE *, 1, sizeof(IP_PRIVATE));
	if (ipp == NULL)
		perr(wp->gp->progname,  NULL);
	wp->ip_private = ipp;

	ipp->i_fd = i_fd;
	ipp->o_fd = o_fd;
 
 	ipp->argc = argc;
 	ipp->argv = argv;
 
	/* Initialize the list of ip functions. */
	ip_func_std(wp->gp);

	return (ipp);
}

/*
 * ip_func_std --
 *	Initialize the standard ip functions.
 */
static void
ip_func_std(gp)
	GS *gp;
{
	gp->scr_addstr = ip_addstr;
	gp->scr_attr = ip_attr;
	gp->scr_baud = ip_baud;
	gp->scr_bell = ip_bell;
	gp->scr_busy = ip_busy;
	gp->scr_clrtoeol = ip_clrtoeol;
	gp->scr_cursor = ip_cursor;
	gp->scr_deleteln = ip_deleteln;
	gp->scr_discard = ip_discard;
	gp->scr_event = ip_event;
	gp->scr_ex_adjust = ip_ex_adjust;
	gp->scr_fmap = ip_fmap;
	gp->scr_insertln = ip_insertln;
	gp->scr_keyval = ip_keyval;
	gp->scr_move = ip_move;
	gp->scr_msg = NULL;
	gp->scr_optchange = ip_optchange;
	gp->scr_refresh = ip_refresh;
	gp->scr_rename = ip_rename;
	gp->scr_reply = ip_reply;
	gp->scr_screen = ip_screen;
	gp->scr_split = ip_split;
	gp->scr_suspend = ip_suspend;
	gp->scr_usage = ip_usage;
}

/*
 * perr --
 *	Print system error.
 */
static void
perr(name, msg)
	char *name, *msg;
{
	(void)fprintf(stderr, "%s:", name);
	if (msg != NULL)
		(void)fprintf(stderr, "%s:", msg);
	(void)fprintf(stderr, "%s\n", strerror(errno));
	exit(1);
}
