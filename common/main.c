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
static char sccsid[] = "$Id: main.c,v 5.5 1992/01/16 13:26:35 bostic Exp $ (Berkeley) $Date: 1992/01/16 13:26:35 $";
#endif /* not lint */

#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "vi.h"
#include "pathnames.h"
#include "extern.h"

#ifdef DEBUG
FILE *tracefp;
#endif

static jmp_buf jmpenv;

#ifndef NO_DIGRAPH
static init_digraphs();
#endif

static void obsolete __P((char *[]));
static void onhup __P((int));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, i;
	char *cmd, *err, *str, *tag;


	/* Set mode based on the program name. */
	mode = MODE_VI;
#ifndef NO_EXTENSIONS
	if (!strcmp(*argv, "edit"))
		*o_inputmode = TRUE;
	else
#endif
	if (!strcmp(*argv, "view"))
		*o_readonly = TRUE;
	else if (!strcmp(argv, "ex"))
		mode = MODE_EX;

	obsolete(argv);
	cmd = err = str = tag = NULL;
	while ((ch = getopt(argc, argv, "c:eimRrT:t:v")) != EOF)
		switch(ch) {
		case 'c':		/* Run the command. */
			cmd = optarg;
			break;
		case 'e':		/* Ex mode. */
			mode = MODE_EX;
			break;
#ifndef NO_EXTENSIONS
		case 'i':		/* Input mode. */
			*o_inputmode = TRUE;
			break;
#endif
#ifndef NO_ERRLIST
		case 'm':		/* Error list. */
			err = optarg;
			break;
#endif
		case 'R':		/* Readonly. */
			*o_readonly = TRUE;
			break;
		case 'r':		/* Recover. */
			msg("use the `elvrec` program to recover lost files");
			endmsgs();
			refresh();
			endwin();
			exit(0);
#ifdef DEBUG
		case 'T':		/* Trace. */
			if ((tracefp = fopen(optarg, "w")) == NULL) {
				msg("%s: %s", optarg, strerror(errno));
				endmsgs();
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
		set_file(argc, argv);
	else {
		static char def[] = "", *defav[] = { def, NULL };
		set_file(1, defav);
	}

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
	initopts();

	/*
	 * Map the arrow keys.  The KU, KD, KL,and KR variables correspond to
	 * the :ku=: (etc.) termcap capabilities.  The variables are defined
	 * as part of the curses package.
	 */
	if (has_KU)
		mapkey(has_KU, "k",	WHEN_VICMD|WHEN_INMV, "<Up>");
	if (has_KD)
		mapkey(has_KD, "j",	WHEN_VICMD|WHEN_INMV, "<Down>");
	if (has_KL)
		mapkey(has_KL, "h",	WHEN_VICMD|WHEN_INMV, "<Left>");
	if (has_KR)
		mapkey(has_KR, "l",	WHEN_VICMD|WHEN_INMV, "<Right>");
	if (has_HM)
		mapkey(has_HM, "^",	WHEN_VICMD|WHEN_INMV, "<Home>");
	if (has_EN)
		mapkey(has_EN, "$",	WHEN_VICMD|WHEN_INMV, "<End>");
	if (has_PU)
		mapkey(has_PU, "\002",	WHEN_VICMD|WHEN_INMV, "<PageUp>");
	if (has_PD)
		mapkey(has_PD, "\006",	WHEN_VICMD|WHEN_INMV, "<PageDn>");
	if (has_KI)
		mapkey(has_KI, "i",	WHEN_VICMD|WHEN_INMV, "<Insert>");
	if (ERASEKEY != '\177')
		mapkey("\177", "x",	WHEN_VICMD|WHEN_INMV, "<Del>");

#ifndef NO_DIGRAPH
	init_digraphs();
#endif

	/* Read the .exrc files and EXINIT environment variable. */
	doexrc(_PATH_SYSEXRC);
#ifdef HMEXRC
	str = getenv("HOME");
	if (str && *str)
	{
		strcpy(tmpblk.c, str);
		str = tmpblk.c + strlen(tmpblk.c);
		if (str[-1] != '/')
			*str++ = '/';
		strcpy(str, HMEXRC);
		doexrc(tmpblk.c);
	}
#endif
	if (*o_exrc)
		doexrc(_NAME_EXRC);
	if ((str = getenv("EXINIT")) != NULL)
		exstring(str, strlen(str));

	/* search for a tag (or an error) now, if desired */
	blkinit();
	if (tag)
		cmd_tag(MARK_FIRST, MARK_FIRST, CMD_TAG, 0, tag);
#ifndef NO_ERRLIST
	else if (err)
		cmd_errlist(MARK_FIRST, MARK_FIRST, CMD_ERRLIST, 0, err);
#endif

	/* If no tag/err, or tag failed, start with first file. */
	if (tmpfd < 0) {
		next_file(MARK_UNSET, MARK_UNSET, 0, FALSE, NULL);

		/* pretend to do something, just to force a recoverable
		 * version of the file out to disk
		 */
		ChangeText
		{
		}
		clrflag(file, MODIFIED);
	}

	/* now we do the immediate ex command that we noticed before */
	if (cmd)
		doexcmd(cmd);

	/* repeatedly call ex() or vi() (depending on the mode) until the
	 * mode is set to MODE_QUIT
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
#ifndef	NO_CURSORSHAPE
	if (has_CQ)
		do_CQ();
#endif
	endmsgs();
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


#ifndef NO_DIGRAPH

/* This stuff us used to build the default digraphs table. */
static char	digtable[][4] =
{
# ifdef CS_IBMPC
	"C,\200",	"u\"\1",	"e'\2",		"a^\3",
	"a\"\4",	"a`\5",		"a@\6",		"c,\7",
	"e^\10",	"e\"\211",	"e`\12",	"i\"\13",
	"i^\14",	"i`\15",	"A\"\16",	"A@\17",
	"E'\20",	"ae\21",	"AE\22",	"o^\23",
	"o\"\24",	"o`\25",	"u^\26",	"u`\27",
	"y\"\30",	"O\"\31",	"U\"\32",	"a'\240",
	"i'!",		"o'\"",		"u'#",		"n~$",
	"N~%",		"a-&",		"o-'",		"~?(",
	"~!-",		"\"<.",		"\">/",
#  ifdef CS_SPECIAL
	"2/+",		"4/,",		"^+;",		"^q<",
	"^c=",		"^r>",		"^t?",		"pp]",
	"^^^",		"oo_",		"*a`",		"*ba",
	"*pc",		"*Sd",		"*se",		"*uf",
	"*tg",		"*Ph",		"*Ti",		"*Oj",
	"*dk",		"*Hl",		"*hm",		"*En",
	"*No",		"eqp",		"pmq",		"ger",
	"les",		"*It",		"*iu",		"*/v",
	"*=w",		"sq{",		"^n|",		"^2}",
	"^3~",		"^_\377",
#  endif /* CS_SPECIAL */
# endif /* CS_IBMPC */
# ifdef CS_LATIN1
	"~!!",		"a-*",		"\">+",		"o-:",
	"\"<>",		"~??",

	"A`@",		"A'A",		"A^B",		"A~C",
	"A\"D",		"A@E",		"AEF",		"C,G",
	"E`H",		"E'I",		"E^J",		"E\"K",
	"I`L",		"I'M",		"I^N",		"I\"O",
	"-DP",		"N~Q",		"O`R",		"O'S",
	"O^T",		"O~U",		"O\"V",		"O/X",
	"U`Y",		"U'Z",		"U^[",		"U\"\\",
	"Y'_",

	"a``",		"a'a",		"a^b",		"a~c",
	"a\"d",		"a@e",		"aef",		"c,g",
	"e`h",		"e'i",		"e^j",		"e\"k",
	"i`l",		"i'm",		"i^n",		"i\"o",
	"-dp",		"n~q",		"o`r",		"o's",
	"o^t",		"o~u",		"o\"v",		"o/x",
	"u`y",		"u'z",		"u^{",		"u\"|",
	"y'~",
# endif /* CS_LATIN1 */
	""
};

static init_digraphs()
{
	int	i;

	for (i = 0; *digtable[i]; i++)
	{
		do_digraph(FALSE, digtable[i]);
	}
	do_digraph(FALSE, (char *)0);
}
#endif /* NO_DIGRAPH */

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
	    "usage: vi [-eimRrv] [-c command] [-m file] [-t tag]\n");
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
