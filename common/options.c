/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: options.c,v 5.18 1992/06/07 13:50:18 bostic Exp $ (Berkeley) $Date: 1992/06/07 13:50:18 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <curses.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <paths.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "screen.h"
#include "term.h"
#include "extern.h"

static int opts_abbcmp __P((const void *, const void *));
static int opts_cmp __P((const void *, const void *));
static int opts_print __P((struct _option *));

/*
 * First array slot is the current value, second and third are the low and
 * and high ends of the range.
 *
 * XXX
 * Some of the limiting values are clearly randomly chosen, and have no
 * meaning.  How make O_REPORT just shut up?
 */
static long o_columns[3] = {80, 32, 255};
static long o_keytime[3] = {2, 0, 50};
static long o_lines[3] = {25, 2, 66};
static long o_report[3] = {5, 1, 127};
static long o_scroll[3] = {12, 1, 127};
static long o_shiftwidth[3] = {8, 1, 255};
static long o_sidescroll[3] = {16, 1, 64};
static long o_tabstop[3] = {8, 1, 40};
static long o_taglength[3] = {0, 0, 30};
static long o_window[3] = {24, 1, 24};
static long o_wrapmargin[3] = {0, 0, 255};

OPTIONS opts[] = {
/* O_AUTOINDENT */
	"autoindent",	NULL,		OPT_0BOOL,
/* O_AUTOPRINT */
	"autoprint",	NULL,		OPT_1BOOL,
/* O_AUTOTAB */
	"autotab",	NULL,		OPT_1BOOL,
/* O_AUTOWRITE */
	"autowrite",	NULL,		OPT_0BOOL,
/* O_BEAUTIFY */
	"beautify",	NULL,		OPT_0BOOL,
/* O_CC */
	"cc",		"cc -c",	OPT_STR,
/* O_COLUMNS */
	"columns",	&o_columns,	OPT_NOSAVE|OPT_NUM|OPT_REDRAW|OPT_SIZE,
/* O_DIGRAPH */
	"digraph",	NULL,		OPT_0BOOL,
/* O_DIRECTORY */
	"directory",	_PATH_TMP,	OPT_NOSAVE|OPT_STR,
/* O_EDCOMPATIBLE */
	"edcompatible",	NULL,		OPT_0BOOL,
/* O_EQUALPRG */
	"equalprg",	"fmt",		OPT_STR,
/* O_ERRORBELLS */
	"errorbells",	NULL,		OPT_1BOOL,
/* O_EXRC */
	"exrc",		NULL,		OPT_0BOOL,
/* O_EXREFRESH */
	"exrefresh",	NULL,		OPT_1BOOL,
/* O_FLASH */
	"flash",	NULL,		OPT_1BOOL,
/* O_IGNORECASE */
	"ignorecase",	NULL,		OPT_0BOOL,
/* O_KEYTIME */
	"keytime",	&o_keytime,	OPT_NUM,
/* O_LINES */
	"lines",	&o_lines,	OPT_NOSAVE|OPT_NUM|OPT_REDRAW|OPT_SIZE,
/* O_LIST */
	"list",		NULL,		OPT_0BOOL|OPT_REDRAW,
/* O_MAGIC */
	"magic",	NULL,		OPT_1BOOL,
/* O_MAKE */
	"make",		"make",		OPT_STR,
/* O_MESG */
	"mesg",		NULL,		OPT_1BOOL,
/* O_NUMBER */
	"number",	NULL,		OPT_0BOOL|OPT_REDRAW,
/* O_PARAGRAPHS */
	"paragraphs",	"PPppIPLPQP",	OPT_STR,
/* O_PROMPT */
	"prompt",	NULL,		OPT_1BOOL,
/* O_READONLY */
	"readonly",	NULL,		OPT_0BOOL,
/* O_REPORT */
	"report",	&o_report,	OPT_NUM,
/* O_RULER */
	"ruler",	NULL,		OPT_0BOOL,
/* O_SCROLL */
	"scroll",	&o_scroll,	OPT_NUM,
/* O_SECTIONS */
	"sections",	"NHSHSSSEse",	OPT_STR,
/* O_SHELL */
	"shell",	_PATH_BSHELL,	OPT_STR,
/* O_SHIFTWIDTH */
	"shiftwidth",	&o_shiftwidth,	OPT_NUM,
/* O_SHOWMATCH */
	"showmatch",	NULL,		OPT_0BOOL,
/* O_SHOWMODE */
	"showmode",	NULL,		OPT_0BOOL,
/* O_SIDESCROLL */
	"sidescroll",	&o_sidescroll,	OPT_NUM,
/* O_SYNC */
	"sync",		NULL,		OPT_0BOOL,
/* O_TABSTOP */
	"tabstop",	&o_tabstop,	OPT_NUM|OPT_REDRAW,
/* O_TAGLENGTH */
	"taglength",	&o_taglength,	OPT_NUM,
/* O_TERM */
	"term",		"unknown",	OPT_NOSAVE|OPT_STR,
/* O_TERSE */
	"terse",	NULL,		OPT_0BOOL,
/* O_TIMEOUT */
	"timeout",	NULL,		OPT_0BOOL,
/* O_VBELL */
	"vbell",	NULL,		OPT_0BOOL,
/* O_VERBOSE */
	"verbose",	NULL,		OPT_0BOOL,
/* O_WARN */
	"warn",		NULL,		OPT_1BOOL,
/* O_WINDOW */
	"window",	&o_window,	OPT_NUM|OPT_REDRAW,
/* O_WRAPMARGIN */
	"wrapmargin",	&o_wrapmargin,	OPT_NUM,
/* O_WRAPSCAN */
	"wrapscan",	NULL,		OPT_1BOOL,
/* O_WRITEANY */
	"writeany",	NULL,		OPT_0BOOL,
	NULL,
};

typedef struct abbrev {
        char *name;
        int offset;
} ABBREV;

static ABBREV abbrev[] = {
	"ai",	O_AUTOINDENT,
	"ap",	O_AUTOPRINT,
	"at",	O_AUTOTAB,
	"aw",	O_AUTOWRITE,
	"bf",	O_BEAUTIFY,
	"cc",	O_CC,
	"co",	O_COLUMNS,
	"dig",	O_DIGRAPH,
	"dir",	O_DIRECTORY,
	"eb",	O_ERRORBELLS,
	"ed",	O_EDCOMPATIBLE,
	"ep",	O_EQUALPRG,
	"er",	O_EXREFRESH,
	"fl",	O_VBELL,
	"ic",	O_IGNORECASE,
	"kt",	O_KEYTIME,
	"li",	O_LIST,
	"ls",	O_LINES,
	"ma",	O_MAGIC,
	"me",	O_MESG,
	"mk",	O_MAKE,
	"nu",	O_NUMBER,
	"pa",	O_PARAGRAPHS,
	"pr",	O_PROMPT,
	"re",	O_REPORT,
	"ro",	O_READONLY,
	"ru",	O_RULER,
	"sc",	O_SCROLL,
	"se",	O_SECTIONS,
	"sh",	O_SHELL,
	"sm",	O_SHOWMATCH,
	"ss",	O_SIDESCROLL,
	"sw",	O_SHIFTWIDTH,
	"sy",	O_SYNC,
	"te",	O_TERM,
	"tl",	O_TAGLENGTH,
	"to",	O_KEYTIME,
	"tr",	O_TERSE,
	"ts",	O_TABSTOP,
	"vb",	O_VBELL,
	"ve",	O_VERBOSE,
	"wa",	O_WARN,
	"wi",	O_WINDOW,
	"wm",	O_WRAPMARGIN,
	"wr",	O_WRITEANY,
	"ws",	O_WRAPSCAN,
	NULL,
};

/*
 * opts_init --
 *	Initialize some of the options.  Since the user isn't really "setting"
 *	these variables, we don't set their OPT_SET bits.
 */
int
opts_init()
{
	struct  winsize win;
	u_short row, col;
	char *s;

	/*
	 * Get the screen rows and columns.  The idea is to duplicate what
	 * curses will do to figure out the rows and columns.  If the values
	 * are wrong, it's not a big deal -- as soon as the user sets them
	 * explicitly the environment will be set and curses will use the new
	 * values.
	 *
	 * Try TIOCGWINSZ.
	 */
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &win) != -1 &&
	    win.ws_row != 0 && win.ws_col != 0) {
		row = win.ws_row;
		col = win.ws_col;
	}

	/* POSIX 1003.2 requires the environment to override. */
	if ((s = getenv("ROWS")) != NULL)
		row = strtol(s, NULL, 10);
	if ((s = getenv("COLUMNS")) != NULL)
		col = strtol(s, NULL, 10);

	/* Make sure we got values that we can live with. */
	if (row < 2 || col < 40) {
		msg("Screen rows %d cols %d: too small.\n", row, col);
		return (1);
	}

	LVAL(O_LINES) = row;
	LVAL(O_SCROLL) = row / 2 - 1;
	LVAL(O_WINDOW) = row - 1;
	LVAL(O_COLUMNS) = col;

	if (s = getenv("SHELL")) {
		PVAL(O_SHELL) = strdup(s);
		FSET(O_SHELL, OPT_ALLOCATED);
	}

	/* Disable the vbell option if we don't know how to do a vbell. */
	if (!VB) {
		FSET(O_FLASH, OPT_NOSET);
		FSET(O_VBELL, OPT_NOSET);
	}
	return (0);
}

/*
 * opts_set --
 *	Change the values of one or more options.
 */
void
opts_set(argv)
	char **argv;
{
	register char *p;
	ABBREV atmp, *ap;
	OPTIONS otmp, *op;
	long value;
	int all, ch, needredraw, off, resize;
	char *ep, *equals, *name, sbuf[50];
	
	for (all = needredraw = resize = 0; *argv; ++argv) {
		/*
		 * The historic vi dumped the options for each occurrence of
		 * "all" in the set list.  Stupid.
		 */
		if (!strcmp(*argv, "all")) {
			all = 1;
			continue;
		}
			
		/* Find equals sign or end of set, skipping backquoted chars. */
		for (p = name = *argv, equals = NULL; ch = *p; ++p)
			switch(ch) {
			case '=':
				equals = p;
				break;
			case '\\':
				/* Historic vi just used the backslash. */
				if (p[1] == '\0')
					break;
				++p;
				break;
			}

		off = 0;
		op = NULL;
		if (equals)
			*equals++ = '\0';

		/* Check list of abbreviations. */
		atmp.name = name;
		if ((ap = bsearch(&atmp, abbrev,
		    sizeof(abbrev) / sizeof(ABBREV) - 1,
		    sizeof(ABBREV), opts_abbcmp)) != NULL) {
			op = opts + ap->offset;
			goto found;
		}

		/* Check list of options. */
		otmp.name = name;
		if ((op = bsearch(&otmp, opts,
		    sizeof(opts) / sizeof(OPTIONS) - 1,
		    sizeof(OPTIONS), opts_cmp)) != NULL)
			goto found;

		/* Try the name without any leading "no". */
		if (name[0] == 'n' && name[1] == 'o') {
			off = 1;
			name += 2;
		} else
			goto found;

		/* Check list of abbreviations. */
		atmp.name = name;
		if ((ap = bsearch(&atmp, abbrev,
		    sizeof(abbrev) / sizeof(ABBREV) - 1,
		    sizeof(ABBREV), opts_abbcmp)) != NULL) {
			op = opts + ap->offset;
			goto found;
		}

		/* Check list of options. */
		otmp.name = name;
		op = bsearch(&otmp, opts,
		    sizeof(opts) / sizeof(OPTIONS) - 1,
		    sizeof(OPTIONS), opts_cmp);

found:		if (op == NULL || off && !ISFSETP(op, OPT_0BOOL|OPT_1BOOL)) {
			msg("no option %s: 'set all' gives all option values",
			    name);
			continue;
		}

		/* Set name, value. */
		if (ISFSETP(op, OPT_NOSET)) {
			msg("%s: may not be set", name);
			continue;
		}

		switch (op->flags & OPT_TYPE) {
		case OPT_0BOOL:
		case OPT_1BOOL:
			if (equals)
				msg("set: option [no]%s is a boolean", name);
			else {
				FUNSETP(op, OPT_0BOOL | OPT_1BOOL);
				FSETP(op,
				    (off ? OPT_0BOOL : OPT_1BOOL) | OPT_SET);
				resize |= ISFSETP(op, OPT_SIZE);
				needredraw |= ISFSETP(op, OPT_REDRAW);
			}
			break;
		case OPT_NUM:
			if (!equals) {
				msg("set: option [no]%s requires a value",
				    name);
				break;
			}
			value = strtol(equals, &ep, 10);
			if (*ep && !isspace(*ep)) {
				msg("set %s: illegal number %s", name, equals);
				break;
			}
			if (value < MINLVALP(op)) {
				msg("set %s: min value is %ld",
				    name, MINLVALP(op));
				break;
			}
			if (value > MAXLVALP(op)) {
				msg("set %s: max value is %ld",
				    name, MAXLVALP(op));
				break;
			}
			LVALP(op) = value;
			FSETP(op, OPT_SET);
			resize |= ISFSETP(op, OPT_SIZE);
			needredraw |= ISFSETP(op, OPT_REDRAW);
			break;
		case OPT_STR:
			if (!equals) {
				msg("set: option [no]%s requires a value",
				    name);
				break;
			}
			if (ISFSETP(op, OPT_ALLOCATED))
				free(PVALP(op));
			PVALP(op) = strdup(equals);
			FSETP(op, OPT_ALLOCATED | OPT_SET);
			resize |= ISFSETP(op, OPT_SIZE);
			needredraw |= ISFSETP(op, OPT_REDRAW);
			break;
		}
	}

	if (all)
		opts_dump(1);

	/*
	 * Rows and columns must be put in the environment so they override
	 * curses default actions.
	 */
	if (resize) {
		scr_end();
		(void)snprintf(sbuf, sizeof(sbuf), "%ld", LVAL(O_LINES));
		(void)setenv("ROWS", sbuf, 1);
		(void)snprintf(sbuf, sizeof(sbuf), "%ld", LVAL(O_COLUMNS));
		(void)setenv("COLUMNS", sbuf, 1);
		scr_init();
	} else if (needredraw)
		scr_ref();
}

/*
 * opt_dump --
 *	List the current values of selected options.
 */
void
opts_dump(all)
	int all;
{
	OPTIONS *op;
	int base, b_num, chcnt, cnt, col, colwidth, curlen, endcol, s_num;
	int numcols, numrows, row, termwidth;
	int b_op[O_OPTIONCOUNT], s_op[O_OPTIONCOUNT];
	char nbuf[20];

	/*
	 * Options are output in two groups -- those that fit at least two to
	 * a line and those that don't.  We do output on tab boundaries for no
	 * particular reason.   First get the set of options to list, keeping
	 * track of the length of each.  No error checking, because we know
	 * that O_TERM was set so at least one option has the OPT_SET bit on.
	 * Termwidth is the tab stop before half of the line in the first loop,
	 * and the full line length later on.
	 */
	colwidth = -1;
	termwidth = (COLS - 1) / 2 & ~(TAB - 1);
	for (b_num = s_num = 0, op = opts; op->name; ++op) {
		if (!all && !ISFSETP(op, OPT_SET))
			continue;
		cnt = op - opts;
		curlen = strlen(op->name);
		switch (op->flags & OPT_TYPE) {
		case OPT_0BOOL:
			curlen += 2;
			break;
		case OPT_1BOOL:
			break;
		case OPT_NUM:
			curlen +=
			    snprintf(nbuf, sizeof(nbuf), "%ld", LVAL(cnt));
			break;
		case OPT_STR:
			curlen += strlen(PVAL(cnt)) + 3;
			break;
		}
		if (curlen < termwidth) {
			if (colwidth < curlen)
				colwidth = curlen;
			s_op[s_num++] = cnt;
		} else
			b_op[b_num++] = cnt;
	}

	colwidth = (colwidth + TAB) & ~(TAB - 1);
	termwidth = COLS - 1;
	numcols = termwidth / colwidth;
	if (s_num > numcols) {
		numrows = s_num / numcols;
		if (s_num % numcols)
			++numrows;
	} else
		numrows = 1;

	EX_PRSTART(1);
	for (row = 0; row < numrows;) {
		endcol = colwidth;
		for (base = row, chcnt = col = 0; col < numcols; ++col) {
			chcnt += opts_print(&opts[s_op[base]]);
			if ((base += numrows) >= s_num)
				break;
			while ((cnt = (chcnt + TAB & ~(TAB - 1))) <= endcol) {
				(void)putchar('\t');
				chcnt = cnt;
			}
			endcol += colwidth;
		}
		if (++row < numrows)
			EX_PRNEWLINE;
	}

	for (row = 0; row < b_num;) {
		(void)opts_print(&opts[b_op[row]]);
		++row;
		if (numrows || row < b_num)
			EX_PRNEWLINE;
	}
	EX_PRTRAIL;
}

/*
 * opts_save --
 *	Write the current configuration to a file.
 */
void
opts_save(fp)
	FILE *fp;
{
	OPTIONS *op;

	for (op = opts; op->name; ++op) {
		if (ISFSETP(op, OPT_NOSAVE))
			continue;
		switch (op->flags & OPT_TYPE) {
		case OPT_0BOOL:
			(void)fprintf(fp, "set no%s\n", op->name);
			break;
		case OPT_1BOOL:
			(void)fprintf(fp, "set %s\n", op->name);
			break;
		case OPT_NUM:
			(void)fprintf(fp,
			    "set %s=%-3d\n", op->name, LVALP(op));
			break;
		case OPT_STR:
			(void)fprintf(fp,
			    "set %s=\"%s\"\n", op->name, PVALP(op));
			break;
		}
	}
}

/*
 * opt_print --
 *	Print out an option.
 */
static int
opts_print(op)
	OPTIONS *op;
{
	int curlen;
	char nbuf[20];

	curlen = 0;
	switch (op->flags & OPT_TYPE) {
	case OPT_0BOOL:
		curlen += 2;
		(void)putchar('n');
		(void)putchar('o');
		/* FALLTHROUGH */
	case OPT_1BOOL:
		curlen += printf("%s", op->name);
		break;
	case OPT_NUM:
		curlen += printf("%s", op->name);
		curlen += 1;
		(void)putchar('=');
		curlen += printf("%ld", LVALP(op));
		break;
	case OPT_STR:
		curlen += printf("%s", op->name);
		curlen += 1;
		(void)putchar('=');
		curlen += 1;
		(void)putchar('"');
		curlen += printf("%s", PVALP(op));
		curlen += 1;
		(void)putchar('"');
		break;
	}
	return (curlen);
}

opts_abbcmp(a, b)
        const void *a, *b;
{
        return(strcmp(((ABBREV *)a)->name, ((ABBREV *)b)->name));
}

opts_cmp(a, b)
        const void *a, *b;
{
        return(strcmp(((OPTIONS *)a)->name, ((OPTIONS *)b)->name));
}
