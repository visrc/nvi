/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
char copyright[] =
"%Z% Copyright (c) 1991 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "$Id: main.c,v 5.12 1992/04/14 09:24:38 bostic Exp $ (Berkeley) $Date: 1992/04/14 09:24:38 $";
#endif /* not lint */

#include <sys/param.h>
#include <signal.h>
#include <setjmp.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "pathnames.h"
#include "seq.h"
#include "extern.h"

#ifdef DEBUG
FILE *tracefp;
#endif

static jmp_buf jmpenv;
int reading_exrc;

static void obsolete __P((char *[]));
static void onhup __P((int));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	CMDARG cmd;
	int ch, i;
	char *excmdarg, *err, *p, *tag, path[MAXPATHLEN];

	/* Set mode based on the program name. */
	if ((p = rindex(*argv, '/')) == NULL)
		p = *argv;
	else
		++p;
	if (!strcmp(p, "ex"))
		mode = MODE_EX;
	else if (!strcmp(p, "view")) {
		SET(O_READONLY)
		mode = MODE_VI;
	} else
		mode = MODE_VI;

	obsolete(argv);
	excmdarg = err = tag = NULL;
	while ((ch = getopt(argc, argv, "c:emRrT:t:v")) != EOF)
		switch(ch) {
		case 'c':		/* Run the command. */
			excmdarg = optarg;
			break;
		case 'e':		/* Ex mode. */
			mode = MODE_EX;
			break;
#ifndef NO_ERRLIST
		case 'm':		/* Error list. */
			err = optarg;
			break;
#endif
		case 'R':		/* Readonly. */
			SET(O_READONLY);
			break;
		case 'r':		/* Recover. */
			(void)fprintf(stderr,
			    "%s: recover option not currently implemented.\n");
			exit(1);
#ifdef DEBUG
		case 'T':		/* Trace. */
			if ((tracefp = fopen(optarg, "w")) == NULL) {
				msg("%s: %s", optarg, strerror(errno));
				endmsg();
			}
			(void)fprintf(tracefp, "trace: open %s\n", optarg);
			break;
#endif
		case 't':		/* Tag. */
			tag = optarg;
			break;
		case 'v':		/* Vi mode. */
			mode = MODE_VI;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* The remaining arguments are file names. */
	if (argc)
		file_set(argc, argv, 0);

	/* Temporarily ignore interrupts. */
	(void)signal(SIGINT, SIG_IGN);

	/* Start curses. */
	initscr();
	cbreak();
	noecho();
	scrollok(stdscr, TRUE);

	/* Catch HUP, TSTP */
	(void)signal(SIGHUP, onhup);
	(void)signal(SIGTSTP, onstop);

	/*
	 * Initialize the options -- must be done after initscr(), so that
	 * we can alter LINES and COLS if necessary.
	 */
	opts_init();

	/* Initialize the key sequence list. */
	seq_init();

	/* Initialize special keys, must be after key sequence init. */
	map_init();

#ifndef NO_DIGRAPH
	digraph_init();
#endif

	/*
	 * Source the system, ~user and local .exrc files.
	 * XXX
	 * Check the correct order for these.
	 */
	reading_exrc = 1;
	(void)exfile(_PATH_SYSEXRC, 0);
	if ((p = getenv("HOME")) != NULL && *p) {
		(void)snprintf(path, sizeof(path), "%s/.exrc", p);
		(void)exfile(path, 0);
	}
	if (ISSET(O_EXRC))
		(void)exfile(_NAME_EXRC, 0);
	reading_exrc = 0;

	/* Source the EXINIT environment variable. */
	if ((p = getenv("EXINIT")) != NULL)
		if ((p = strdup(p)) == NULL)
			msg("Error: %s", strerror(errno));
		else {
			(void)exstring(p, strlen(p));
			free(p);
		}

	/* Search for a tag (or an error) now, if desired. */
	blkinit();
	if (tag) {
		SETCMDARG(cmd, C_TAG, 2, MARK_FIRST, MARK_FIRST, 0, tag);
		ex_tag(&cmd);
	}
#ifndef NO_ERRLIST
	else if (err) {
		SETCMDARG(cmd, C_ERRLIST, 2, MARK_FIRST, MARK_FIRST, 0, err);
		ex_errlist(&cmd);
	}
#endif

	/* If no tag/err, or tag failed, start with first file. */
	if (tmpfd < 0) {
		if (file_cnt()) {
			SETCMDARG(cmd,
			    C_NEXT, 0, MARK_UNSET, MARK_UNSET, 0, NULL);
			ex_next(&cmd);
		} else
			tmpstart("");

		/* pretend to do something, just to force a recoverable
		 * version of the file out to disk
		 */
		ChangeText
		{
		}
		clrflag(file, MODIFIED);
	}

	/* Now we do the immediate ex command that we noticed before. */
	if (excmdarg)
		(void)excmd(excmdarg);

	/*
	 * Repeatedly call ex() or vi() (depending on the mode) until the
	 * mode is set to MODE_QUIT.
	 */
	while (mode != MODE_QUIT) {
		if (setjmp(jmpenv))
			/* Maybe we just aborted a change? */
			abortdo();
		signal(SIGINT, (void(*)()) trapint);

		switch (mode) {
		  case MODE_VI:
			vi();
			break;

		  case MODE_EX:
			ex();
			break;
#ifdef DEBUG
		  default:
			msg("mode = %d?", mode);
			mode = MODE_QUIT;
#endif
		}
	}

	/* free up the cut buffers */
	cutend();

	/* end curses */
	endmsg();
	move(LINES - 1, 0);
	clrtoeol();
	refresh();
	endwin();

	exit(0);
	/* NOTREACHED */
}


/*ARGSUSED*/
void
trapint(signo)
	int signo;
{
	resume_curses(FALSE);
	abortdo();
	signal(signo, trapint);
	doingglobal = FALSE;
	longjmp(jmpenv, 1);
}

static void
obsolete(argv)
	char *argv[];
{
	static char *eofarg = "-c$";

	/*
	 * Translate old style arguments into something getopt will like.
	 * Change "+/command" into "-ccommand".
	 * Change "+" into "-c$".
	 */
	while (*++argv)
		if (argv[0][0] == '+')
			if (argv[0][1] == '\0')
				argv[0] = eofarg;
			else if (argv[0][1] == '/') {
				argv[0][0] = '-';
				argv[0][1] = 'c';
			}
			
}

static void
usage()
{
	(void)fprintf(stderr,
	    "usage: vi [-emRrv] [-c command] [-m file] [-t tag]\n");
	exit(1);
}

/*
 * This function handles deadly signals.  It restores sanity to the terminal
 * preserves the current temp file, and deletes any old temp files.
 */
static void
onhup(signo)
	int signo;
{
	/* Restore the terminal's sanity. */
	endwin();

	/* If we had a temp file going, then preserve it. */
	if (tmpnum > 0 && tmpfd >= 0) {
		(void)close(tmpfd);
		(void)sprintf(tmpblk.c,
		    "%s \"%s\" %s", _PATH_PRESERVE, "vi died", tmpname);
		(void)system(tmpblk.c);
	}

	/* Delete any old temp files. */
	cutend();

	/* Exit with the proper exit status. */
	(void)signal(signo, SIG_DFL);
	(void)kill(0, signo);
	/* NOTREACHED */
}
