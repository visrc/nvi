/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char copyright[] =
"%Z% Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n\
%Z% Copyright (c) 1994, 1995\n\
	Keith Bostic.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "$Id: main.c,v 10.8 1995/06/23 19:14:08 bostic Exp $ (Berkeley) $Date: 1995/06/23 19:14:08 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "../ex/tag.h"

static void	 v_estr __P((char *, int, char *));
static int	 v_obsolete __P((char *, char *[]));

/*
 * v_init --
 *	Main editor initialization routine.
 *
 * PUBLIC: init_t v_init __P((GS *, int, char *[], recno_t, size_t));
 */
init_t
v_init(gp, argc, argv, rows, cols)
	GS *gp;
	int argc;
	char *argv[];
	recno_t rows;
	size_t cols;
{
	extern int optind;
	extern char *optarg;
	FREF *frp;
	SCR *sp;
	init_t rval;
	u_int flags;
	int ch, flagchk, lflag, readonly, silent;
	char *tag_f, *wsizearg;
	char path[256];

	/*
	 * Common global structure initialization.
	 *
	 * !!!
	 * Signals not on, no need to block them for queue manipulation.
	 */
	CIRCLEQ_INIT(&gp->dq);
	CIRCLEQ_INIT(&gp->hq);
	LIST_INIT(&gp->ecq);
	LIST_INSERT_HEAD(&gp->ecq, &gp->excmd, q);
	LIST_INIT(&gp->msgq);
	gp->noprint = DEFAULT_NOPRINT;

	/* Structures shared by screens so stored in the GS structure. */
	CIRCLEQ_INIT(&gp->frefq);
	CIRCLEQ_INIT(&gp->dcb_store.textq);
	LIST_INIT(&gp->cutq);
	LIST_INIT(&gp->seqq);

	/* Set initial screen type and mode based on the program name. */
	readonly = 0;
	if (!strcmp(gp->progname, "ex") || !strcmp(gp->progname, "nex"))
		LF_INIT(S_EX);
	else {
		/* Nview, view, xview are readonly. */
		if (!strcmp(gp->progname, "nview") ||
		    !strcmp(gp->progname, "view") ||
		    !strcmp(gp->progname, "xview"))
			readonly = 1;
		
		/* Vi is the default. */
		LF_INIT(S_VI);
	}

	/* Convert old-style arguments into new-style ones. */
	if (v_obsolete(gp->progname, argv))
		return (INIT_ERR);

	/* Parse the arguments. */
	flagchk = '\0';
	tag_f = wsizearg = NULL;
	lflag = silent = 0;

	/* Set the file snapshot flag. */
	F_SET(gp, G_SNAPSHOT);

#ifdef DEBUG
	while ((ch = getopt(argc, argv, "c:DeFlRrsT:t:vw:")) != EOF)
#else
	while ((ch = getopt(argc, argv, "c:eFlRrst:vw:")) != EOF)
#endif
		switch (ch) {
		case 'c':		/* Run the command. */
			/*
			 * XXX
			 * We should support multiple -c options.
			 */
			gp->icommand = optarg;
			break;
#ifdef DEBUG
		case 'D':
			(void)printf("%lu waiting...\n", (u_long)getpid());
			(void)read(STDIN_FILENO, &ch, 1);
			break;
#endif
		case 'e':		/* Ex mode. */
			LF_CLR(S_VI);
			LF_SET(S_EX);
			break;
		case 'F':		/* No snapshot. */
			F_CLR(gp, G_SNAPSHOT);
			break;
		case 'l':		/* Set lisp, showmatch options. */
			lflag = 1;
			break;
		case 'R':		/* Readonly. */
			readonly = 1;
			break;
		case 'r':		/* Recover. */
			if (flagchk == 't') {
				v_estr(gp->progname, 0,
				    "only one of -r and -t may be specified.");
				return (INIT_ERR);
			}
			flagchk = 'r';
			break;
		case 's':
			silent = 1;
			break;
#ifdef DEBUG
		case 'T':		/* Trace. */
			if ((gp->tracefp = fopen(optarg, "w")) == NULL) {
				v_estr(gp->progname, errno, optarg);
				goto err;
			}
			(void)fprintf(gp->tracefp,
			    "\n===\ntrace: open %s\n", optarg);
			break;
#endif
		case 't':		/* Tag. */
			if (flagchk == 'r') {
				v_estr(gp->progname, 0,
				    "only one of -r and -t may be specified.");
				return (INIT_ERR);
			}
			if (flagchk == 't') {
				v_estr(gp->progname, 0,
				    "only one tag file may be specified.");
				return (INIT_ERR);
			}
			flagchk = 't';
			tag_f = optarg;
			break;
		case 'v':		/* Vi mode. */
			LF_CLR(S_EX);
			LF_SET(S_VI);
			break;
		case 'w':
			wsizearg = optarg;
			break;
		case '?':
		default:
			return (INIT_USAGE);
		}
	argc -= optind;
	argv += optind;

	/*
	 * -s option is only meaningful to ex.
	 *
	 * If not reading from a terminal, it's like -s was specified.
	 */
	if (silent && !LF_ISSET(S_EX)) {
		v_estr(gp->progname, 0, "-s option is only applicable to ex.");
		goto err;
	}
	if (LF_ISSET(S_EX) && !F_ISSET(gp, G_STDIN_TTY))
		silent = 1;

	/*
	 * Build and initialize the first/current screen.  This is a bit
	 * tricky.  If an error is returned, we may or may not have a
	 * screen structure.  If we have a screen structure, put it on a
	 * display queue so that the error messages get displayed.
	 *
	 * !!!
	 * Signals not yet turned on, don't block them for queue manipulation.
	 *
	 * !!!
	 * Everything we do until we go interactive is done in ex mode.
	 */
	if (screen_init(gp, NULL, &sp)) {
		if (sp != NULL)
			CIRCLEQ_INSERT_HEAD(&gp->dq, sp, q);
		goto err;
	}
	F_SET(sp, S_EX);
	CIRCLEQ_INSERT_HEAD(&gp->dq, sp, q);

	if (v_key_init(sp))		/* Special key initialization. */
		goto err;

	{ int oargs[4], *oargp = oargs;
	if (readonly)			/* Command-line options. */
		*oargp++ = O_READONLY;
	if (lflag) {
		*oargp++ = O_LISP;
		*oargp++ = O_SHOWMATCH;
	}
	*oargp = -1;			/* Options initialization. */
	if (opts_init(sp, oargs, rows, cols))
		goto err;
	}
	if (wsizearg != NULL) {
		ARGS *av[2], a, b;
		(void)snprintf(path, sizeof(path), "window=%s", wsizearg);
		a.bp = (CHAR_T *)path;
		a.len = strlen(path);
		b.bp = NULL;
		b.len = 0;
		av[0] = &a;
		av[1] = &b;
		(void)opts_set(sp, av, 0, NULL);
	}
	if (silent) {			/* Ex batch mode option values. */
		O_CLR(sp, O_AUTOPRINT);
		O_CLR(sp, O_PROMPT);
		O_CLR(sp, O_VERBOSE);
		O_CLR(sp, O_WARN);
		F_SET(sp, S_EX_SILENT);
	}

	if (!silent) {			/* Read EXINIT, exrc files. */
		if (ex_exrc(sp))
			goto err;
		if (F_ISSET(sp, S_EXIT | S_EXIT_FORCE))
			goto done;
	}
	F_CLR(sp, S_EX | S_VI);
	F_SET(sp, LF_ISSET(S_EX | S_VI));

	/*
	 * List recovery files if -r specified without file arguments.
	 * Note, options must be initialized and startup information
	 * read before doing this.
	 */
	if (flagchk == 'r' && argv[0] == NULL) {
		if (rcv_list(sp))
			goto err;
		F_SET(sp, S_EXIT);
		goto done;
	}

	/*
	 * !!!
	 * Initialize the default ^D, ^U scrolling value here, after the
	 * user has had every opportunity to set the window option.
	 *
	 * It's historic practice that changing the value of the window
	 * option did not alter the default scrolling value, only giving
	 * a count to ^D/^U did that.
	 */
	sp->defscroll = (O_VAL(sp, O_WINDOW) + 1) / 2;

	/* Open a tag file if specified. */
	if (tag_f != NULL && ex_tagfirst(sp, tag_f))
		goto err;

	/*
	 * Append any remaining arguments as file names.  Files are recovery
	 * files if -r specified.  If the tag option or ex startup commands
	 * loaded a file, then any file arguments are going to come after it.
	 */
	if (*argv != NULL) {
		if (sp->frp != NULL) {
			MALLOC_NOMSG(sp,
			    *--argv, char *, strlen(sp->frp->name) + 1);
			if (*argv == NULL) {
				v_estr(gp->progname, errno, NULL);
				goto err;
			}
			(void)strcpy(*argv, sp->frp->name);
		}
		sp->argv = sp->cargv = argv;
		F_SET(sp, S_ARGNOFREE);
		if (flagchk == 'r')
			F_SET(sp, S_ARGRECOVER);
	}

	/*
	 * If the ex startup commands and or/the tag option haven't already
	 * created a file, create one.  If no command-line files were given,
	 * use a temporary file.
	 */
	if (sp->frp == NULL) {
		if (sp->argv == NULL) {
			if ((frp = file_add(sp, NULL)) == NULL)
				goto err;
		} else  {
			if ((frp = file_add(sp, (CHAR_T *)sp->argv[0])) == NULL)
				goto err;
			if (F_ISSET(sp, S_ARGRECOVER))
				F_SET(frp, FR_RECOVER);
		}
		if (file_init(sp, frp, NULL, 0))
			goto err;
	}

	/* Startup information may have exited. */
	if (F_ISSET(sp, S_EXIT | S_EXIT_FORCE)) {
done:		rval = INIT_DONE;
		if (0)
err:			rval = INIT_ERR;
		v_end(gp);
		return (rval);
	}
	return (INIT_OK);
}

/*
 * v_end --
 *	End the program, discarding screens and the global area.
 *
 * PUBLIC: void v_end __P((GS *));
 */
void
v_end(gp)
	GS *gp;
{
	MSG *mp;
	SCR *sp;
	char *tty;

	/* If there are any remaining screens, kill them off. */
	while ((sp = gp->dq.cqh_first) != (void *)&gp->dq) {
		sp->refcnt = 1;
		(void)screen_end(sp);
	}
	while ((sp = gp->hq.cqh_first) != (void *)&gp->hq) {
		sp->refcnt = 1;
		(void)screen_end(sp);
	}

#if defined(DEBUG) || defined(PURIFY) || !defined(STANDALONE)
	{ FREF *frp;
		/* Free FREF's. */
		while ((frp = gp->frefq.cqh_first) != (FREF *)&gp->frefq) {
			CIRCLEQ_REMOVE(&gp->frefq, frp, q);
			if (frp->name != NULL)
				free(frp->name);
			if (frp->tname != NULL)
				free(frp->tname);
			free(frp);
		}
	}

	/* Free key input queue. */
	if (gp->i_event != NULL)
		free(gp->i_event);

	/* Free cut buffers. */
	cut_close(gp);

	/* Free map sequences. */
	seq_close(gp);

	/* Default buffer storage. */
	(void)text_lfree(&gp->dcb_store.textq);

	/* Close message catalogs. */
	msg_close(gp);
#endif

	/* Ring the bell if scheduled. */
	if (F_ISSET(gp, G_BELLSCHED))
		(void)fprintf(stderr, "\07");		/* \a */

	/*
	 * Flush any remaining messages.  If a message is here, it's
	 * almost certainly the message about the event that killed us.
	 * Prepend a program name.
	 */
	while ((mp = gp->msgq.lh_first) != NULL) {
		(void)fprintf(stderr,
		    "%s: %.*s.\n", gp->progname, (int)mp->len, mp->buf);
		LIST_REMOVE(mp, q);
#if defined(DEBUG) || defined(PURIFY) || !defined(STANDALONE)
		free(mp->buf);
		free(mp);
#endif
	}

	/* Reset anything that needs resetting. */
	if (gp->flags & G_SETMODE)			/* O_MESG */
		if ((tty = ttyname(STDERR_FILENO)) == NULL)
			v_estr(gp->progname, errno, "ttyname");
		else if (chmod(tty, gp->origmode) < 0)
			v_estr(gp->progname, errno, tty);

#if defined(DEBUG) || defined(PURIFY) || !defined(STANDALONE)
	/* Free any temporary space. */
	if (gp->tmp_bp != NULL)
		free(gp->tmp_bp);

#ifdef DEBUG
	/* Close debugging file descriptor. */
	if (gp->tracefp != NULL)
		(void)fclose(gp->tracefp);
#endif
	free(gp);
#endif
}

/*
 * v_obsolete --
 *	Convert historic arguments into something getopt(3) will like.
 */
static int
v_obsolete(name, argv)
	char *name, *argv[];
{
	size_t len;
	char *p;

	/*
	 * Translate old style arguments into something getopt will like.
	 * Make sure it's not text space memory, because ex modifies the
	 * strings.
	 *	Change "+" into "-c$".
	 *	Change "+<anything else>" into "-c<anything else>".
	 *	Change "-" into "-s"
	 *	The c, T, t and w options take arguments so they can't be
	 *	    special arguments.
	 */
	while (*++argv)
		if (argv[0][0] == '+') {
			if (argv[0][1] == '\0') {
				MALLOC_NOMSG(NULL, argv[0], char *, 4);
				if (argv[0] == NULL)
					goto nomem;
				(void)strcpy(argv[0], "-c$");
			} else  {
				p = argv[0];
				len = strlen(argv[0]);
				MALLOC_NOMSG(NULL, argv[0], char *, len + 2);
				if (argv[0] == NULL)
					goto nomem;
				argv[0][0] = '-';
				argv[0][1] = 'c';
				(void)strcpy(argv[0] + 2, p + 1);
			}
		} else if (argv[0][0] == '-')
			if (argv[0][1] == '\0') {
				MALLOC_NOMSG(NULL, argv[0], char *, 3);
				if (argv[0] == NULL) {
nomem:					v_estr(name, errno, NULL);
					return (1);
				}
				(void)strcpy(argv[0], "-s");
			} else
				if ((argv[0][1] == 'c' || argv[0][1] == 'T' ||
				    argv[0][1] == 't' || argv[0][1] == 'w') &&
				    argv[0][2] == '\0')
					++argv;
	return (0);
}

static void
v_estr(name, eno, msg)
	char *name, *msg;
	int eno;
{
	(void)fprintf(stderr, "%s", name);
	if (msg != NULL)
		(void)fprintf(stderr, ": %s", msg);
	if (eno)
		(void)fprintf(stderr, ": %s", strerror(errno));
	(void)fprintf(stderr, "\n");
}
