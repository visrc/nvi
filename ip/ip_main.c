/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ip_main.c,v 8.1 1996/09/20 19:35:58 bostic Exp $ (Berkeley) $Date: 1996/09/20 19:35:58 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#include "../common/common.h"
#include "ip.h"

static void	   ip_func_std __P((GS *));
static IP_PRIVATE *ip_init __P((GS *, char *));
static void	   nomem __P((char *));

/*
 * main --
 *      This is the main loop for the vi-as-library editor.
 */
int
ip_main(gp, argc, argv, ip_arg)
	GS *gp;
	int argc;
	char *argv[], *ip_arg;
{
	EVENT ev;
	IP_PRIVATE *ipp;
	IP_BUF ipb;
	int rval;

	/* Create and partially initialize the IP structure. */
	if ((ipp = ip_init(gp, ip_arg)) == NULL)
		return (1);

	/*
	 * Figure out how big the screen is -- read events until we get
	 * the rows and columns.
	 */
	do {
		if (ip_event(NULL, &ev, 0, 0))
			return (1);
	} while (ev.e_event != IPO_EOF && ev.e_event != IPO_ERR &&
	    ev.e_event != IPO_QUIT && ev.e_event != IPO_RESIZE &&
	    ev.e_event != IPO_SIGHUP && ev.e_event != IPO_SIGTERM);
	if (ev.e_event != IPO_RESIZE)
		return (1);

	/* Add the rows and columns to the global structure. */
	OG_VAL(gp, GO_LINES) = OG_D_VAL(gp, GO_LINES) = ev.e_lno;
	OG_VAL(gp, GO_COLUMNS) = OG_D_VAL(gp, GO_COLUMNS) = ev.e_cno;

	/* Run ex/vi. */
	rval = editor(gp, argc, argv);

	/* Clean up the screen. */
	(void)ip_quit(gp);

	/* Free the global and IP private areas. */
#if defined(DEBUG) || defined(PURIFY) || defined(LIBRARY)
	free(ipp);
	free(gp);
#endif

	return (rval);
}

/*
 * ip_init --
 *	Create and partially initialize the GS structure.
 */
static IP_PRIVATE *
ip_init(gp, ip_arg)
	GS *gp;
	char *ip_arg;
{
	IP_PRIVATE *ipp;
	char *ep;

	/* Allocate the IP private structure. */
	CALLOC_NOMSG(NULL, ipp, IP_PRIVATE *, 1, sizeof(IP_PRIVATE));
	if (ipp == NULL)
		nomem(gp->progname);
	gp->ip_private = ipp;

	/*
	 * Crack ip_arg -- it's of the form #.#, where the first number is the
	 * file descriptor from the screen, the second is the file descriptor
	 * to the screen.
	 */
	if (!isdigit(ip_arg[0]))
		goto usage;
	ipp->i_fd = strtol(ip_arg, &ep, 10);
	if (ep[0] != '.' || !isdigit(ep[1]))
		goto usage;
	ipp->o_fd = strtol(ep, &ep, 10);
	if (ep[0] != '\0') {
usage:		ip_usage();
		return (NULL);
	}

	/* Initialize the list of ip functions. */
	ip_func_std(gp);

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
	gp->scr_screen = ip_screen;
	gp->scr_suspend = ip_suspend;
	gp->scr_usage = ip_usage;
}

/*
 * nomem --
 *	No memory error.
 */
static void
nomem(name)
	char *name;
{
	(void)fprintf(stderr, "%s: %s\n", name, strerror(errno));
	exit(1);
}
