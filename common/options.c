/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: options.c,v 5.30 1992/12/20 15:54:14 bostic Exp $ (Berkeley) $Date: 1992/12/20 15:54:14 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "screen.h"
#include "term.h"

static int	f_columns __P((void *));
static int	f_keytime __P((void *));
static int	f_lines __P((void *));
static int	f_ruler __P((void *));
static int	f_shiftwidth __P((void *));
static int	f_sidescroll __P((void *));
static int	f_tabstop __P((void *));
static int	f_wrapmargin __P((void *));

static long	s_columns;
static long	s_keytime;
static long	s_lines;
static long	s_scroll;
static long	s_shiftwidth;
static long	s_sidescroll;
static long	s_tabstop;
static long	s_wrapmargin;

static int opts_abbcmp __P((const void *, const void *));
static int opts_cmp __P((const void *, const void *));
static int opts_print __P((struct _option *));
static int opts_print __P((struct _option *));

OPTIONS opts[] = {
/* O_AUTOINDENT */
	"autoindent",	NULL,		NULL,		OPT_0BOOL,
/* O_AUTOPRINT */
	"autoprint",	NULL,		NULL,		OPT_1BOOL,
/* O_AUTOTAB */
	"autotab",	NULL,		NULL,		OPT_1BOOL,
/* O_AUTOWRITE */
	"autowrite",	NULL,		NULL,		OPT_0BOOL,
/* O_BEAUTIFY */
	"beautify",	NULL,		NULL,		OPT_0BOOL,
/* O_CC */
	"cc",		"cc -c",	NULL,		OPT_STR,
/* O_COLUMNS */
	"columns",	&s_columns,	f_columns,
	    OPT_NOSAVE|OPT_NUM|OPT_REDRAW|OPT_SIZE,
/* O_COMMENT */
	"comment",	NULL,		NULL,		OPT_0BOOL,
/* O_DIGRAPH */
	"digraph",	NULL,		NULL,		OPT_0BOOL,
/* O_DIRECTORY */
	"directory",	_PATH_TMP,	NULL,		OPT_NOSAVE|OPT_STR,
/* O_EDCOMPATIBLE */
	"edcompatible",	NULL,		NULL,		OPT_0BOOL,
/* O_EQUALPRG */
	"equalprg",	"fmt",		NULL,		OPT_STR,
/* O_ERRORBELLS */
	"errorbells",	NULL,		NULL,		OPT_1BOOL,
/* O_EXRC */
	"exrc",		NULL,		NULL,		OPT_0BOOL,
/* O_EXREFRESH */
	"exrefresh",	NULL,		NULL,		OPT_1BOOL,
/* O_EXTENDED */
	"extended",	NULL,		NULL,		OPT_0BOOL,
/* O_FLASH */
	"flash",	NULL,		NULL,		OPT_1BOOL,
/* O_IGNORECASE */
	"ignorecase",	NULL,		NULL,		OPT_0BOOL,
/* O_KEYTIME */
	"keytime",	&s_keytime,	f_keytime,	OPT_NUM,
/* O_LINES */
	"lines",	&s_lines,	f_lines,
	    OPT_NOSAVE|OPT_NUM|OPT_REDRAW|OPT_SIZE,
/* O_LIST */
	"list",		NULL,		NULL,		OPT_0BOOL|OPT_REDRAW,
/* O_MAGIC */
	"magic",	NULL,		NULL,		OPT_1BOOL,
/* O_MAKE */
	"make",		"make",		NULL,		OPT_STR,
/* O_MESG */
	"mesg",		NULL,		NULL,		OPT_1BOOL,
/* O_NUMBER */
	"number",	NULL,		NULL,		OPT_0BOOL|OPT_REDRAW,
/* O_NUNDO */
	"nundo",	NULL,		NULL,		OPT_0BOOL,
/* O_PARAGRAPHS */
	"paragraphs",
	    "IPLPPPQPP LIpplpipbp",	NULL,		OPT_STR,
/* O_PROMPT */
	"prompt",	NULL,		NULL,		OPT_1BOOL,
/* O_READONLY */
	"readonly",	NULL,		NULL,		OPT_0BOOL,
/* O_REPORT */
	"report",	NULL,		NULL,		OPT_NUM,
/* O_RULER */
	"ruler",	NULL,		f_ruler,	OPT_0BOOL,
/* O_SCROLL */
	"scroll",	&s_scroll,	NULL,		OPT_NUM,
/* O_SECTIONS */
	"sections",	"NHSHH HUnhsh",	NULL,		OPT_STR,
/* O_SHELL */
	"shell",	_PATH_BSHELL,	NULL,		OPT_STR,
/* O_SHIFTWIDTH */
	"shiftwidth",	&s_shiftwidth,	f_shiftwidth,	OPT_NUM,
/* O_SHOWMATCH */
	"showmatch",	NULL,		NULL,		OPT_0BOOL,
/* O_SHOWMODE */
	"showmode",	NULL,		NULL,		OPT_0BOOL,
/* O_SIDESCROLL */
	"sidescroll",	&s_sidescroll,	f_sidescroll,	OPT_NUM,
/* O_SYNC */
	"sync",		NULL,		NULL,		OPT_0BOOL,
/* O_TABSTOP */
	"tabstop",	&s_tabstop,	f_tabstop,	OPT_NUM|OPT_REDRAW,
/* O_TERM */
	"term",		"unknown",	NULL,		OPT_NOSAVE|OPT_STR,
/* O_TERSE */
	"terse",	NULL,		NULL,		OPT_0BOOL,
/* O_TIMEOUT */
	"timeout",	NULL,		NULL,		OPT_0BOOL,
/* O_VBELL */
	"vbell",	NULL,		NULL,		OPT_0BOOL,
/* O_VERBOSE */
	"verbose",	NULL,		NULL,		OPT_0BOOL,
/* O_WARN */
	"warn",		NULL,		NULL,		OPT_1BOOL,
/* O_WRAPMARGIN */
	"wrapmargin",	&s_wrapmargin,	f_wrapmargin,	OPT_NUM,
/* O_WRAPSCAN */
	"wrapscan",	NULL,		NULL,		OPT_1BOOL,
/* O_WRITEANY */
	"writeany",	NULL,		NULL,		OPT_0BOOL,
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
	"to",	O_KEYTIME,
	"tr",	O_TERSE,
	"ts",	O_TABSTOP,
	"vb",	O_VBELL,
	"ve",	O_VERBOSE,
	"wa",	O_WARN,
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
	struct winsize win;
	size_t row, col;
	char *s, *argv[2], obuf[100];

	argv[0] = obuf;
	argv[1] = NULL;

	row = 80;
	col = 24;

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

	(void)snprintf(obuf, sizeof(obuf), "ls=%u", row);
	if (opts_set(argv))
		return (1);
	(void)snprintf(obuf, sizeof(obuf), "co=%u", col);
	if (opts_set(argv))
		return (1);
	(void)snprintf(obuf, sizeof(obuf), "sc=%u", row / 2 - 1);
	if (opts_set(argv))
		return (1);

	if (s = getenv("SHELL")) {
		(void)snprintf(obuf, sizeof(obuf), "shell=%s", s);
		if (opts_set(argv))
			return (1);
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
int
opts_set(argv)
	char **argv;
{
	register char *p;
	ABBREV atmp, *ap;
	OPTIONS otmp, *op;
	long value;
	int all, ch, off, rval;
	char *ep, *equals, *name;
	
	for (all = rval = 0; *argv; ++argv) {
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
			if (equals) {
				msg("set: option [no]%s is a boolean", name);
				break;
			}
			FUNSETP(op, OPT_0BOOL | OPT_1BOOL);
			FSETP(op, (off ? OPT_0BOOL : OPT_1BOOL) | OPT_SET);
			if (op->func && op->func(&value)) {
				rval = 1;
				break;
			}
			goto draw;
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
			if (op->func && op->func(&value)) {
				rval = 1;
				break;
			}
			LVALP(op) = value;
			FSETP(op, OPT_SET);
			goto draw;
		case OPT_STR:
			if (!equals) {
				msg("set: option [no]%s requires a value",
				    name);
				break;
			}
			if (op->func && op->func(&value)) {
				rval = 1;
				break;
			}
			if (ISFSETP(op, OPT_ALLOCATED))
				free(PVALP(op));
			PVALP(op) = strdup(equals);
			FSETP(op, OPT_ALLOCATED | OPT_SET);
draw:			if (curf != NULL &&
			    ISFSETP(op, OPT_REDRAW | OPT_SIZE)) {
				if (ISFSETP(op, OPT_REDRAW))
					FF_SET(curf, F_REDRAW);
				if (ISFSETP(op, OPT_SIZE))
					FF_SET(curf, F_RESIZE);
			}
			break;
		default:
			abort();
		}
	}
	if (all)
		opts_dump(1);
	return (rval);
}

static int
f_columns(valp)
	void *valp;
{
	u_long val;

	val = *(u_long *)valp;

#define	MINCOLUMNS	10
	if (val < MINCOLUMNS) {
		msg("Screen columns too small, less than %d.\n", MINCOLUMNS);
		return (1);
	}
	if (val < LVAL(O_SHIFTWIDTH)) {
		msg("Screen columns too small, less than shiftwidth.\n");
		return (1);
	}
	if (val < LVAL(O_SIDESCROLL)) {
		msg("Screen columns too small, less than sidescroll.\n");
		return (1);
	}
	if (val < LVAL(O_TABSTOP)) {
		msg("Screen columns too small, less than tabstop.\n");
		return (1);
	}
	if (val < LVAL(O_WRAPMARGIN)) {
		msg("Screen columns too small, less than wrapmargin.\n");
		return (1);
	}
	return (0);
}

static int
f_keytime(valp)
	void *valp;
{
	u_long val;

	val = *(u_long *)valp;

#define	MAXKEYTIME	20
	if (val > MAXKEYTIME) {
		msg("Keytime too large, more than %d.\n", MAXKEYTIME);
		return (1);
	}
	return (0);
}

static int
f_lines(valp)
	void *valp;
{
	u_long val;

	val = *(u_long *)valp;

#define	MINLINES	4
	if (val < MINLINES) {
		msg("Screen lines too small, less than %d.\n", MINLINES);
		return (1);
	}
	return (0);
}

static int
f_ruler(valp)
	void *valp;
{
	if (curf != NULL)
		scr_modeline(curf, 0);
	return (0);
}

static int
f_shiftwidth(valp)
	void *valp;
{
	u_long val;

	val = *(u_long *)valp;

	if (val > LVAL(O_COLUMNS)) {
		msg("Shiftwidth can't be larger than screen size.\n");
		return (1);
	}
	return (0);
}

static int
f_sidescroll(valp)
	void *valp;
{
	u_long val;

	val = *(u_long *)valp;

	if (val > LVAL(O_COLUMNS)) {
		msg("Sidescroll can't be larger than screen size.\n");
		return (1);
	}
	return (0);
}

static int
f_tabstop(valp)
	void *valp;
{
	u_long val;

	val = *(u_long *)valp;

#define	MAXTABSTOP	20
	if (val > MAXTABSTOP) {
		msg("Tab stops can't be larger than %d.\n", MAXTABSTOP);
		return (1);
	}
	if (val > LVAL(O_COLUMNS)) {
		msg("Tab stops can't be larger than screen size.\n",
		    MAXTABSTOP);
		return (1);
	}
	return (0);
}

static int
f_wrapmargin(valp)
	void *valp;
{
	u_long val;

	val = *(u_long *)valp;

	if (val > LVAL(O_COLUMNS)) {
		msg("Wrapmargin value can't be larger than screen size.\n");
		return (1);
	}
	return (0);
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
	int numcols, numrows, row, tablen, termwidth;
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
	tablen = LVAL(O_TABSTOP);
	termwidth = (curf->cols - 1) / 2 & ~(tablen - 1);
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
			(void)snprintf(nbuf, sizeof(nbuf), "%ld", LVAL(cnt));
			curlen += strlen(nbuf);
			break;
		case OPT_STR:
			curlen += strlen((char *)PVAL(cnt)) + 3;
			break;
		}
		if (curlen < termwidth) {
			if (colwidth < curlen)
				colwidth = curlen;
			s_op[s_num++] = cnt;
		} else
			b_op[b_num++] = cnt;
	}

	colwidth = (colwidth + tablen) & ~(tablen - 1);
	termwidth = curf->cols - 1;
	numcols = termwidth / colwidth;
	if (s_num > numcols) {
		numrows = s_num / numcols;
		if (s_num % numcols)
			++numrows;
	} else
		numrows = 1;

	for (row = 0; row < numrows;) {
		endcol = colwidth;
		for (base = row, chcnt = col = 0; col < numcols; ++col) {
			chcnt += opts_print(&opts[s_op[base]]);
			if ((base += numrows) >= s_num)
				break;
			while ((cnt =
			    (chcnt + tablen & ~(tablen - 1))) <= endcol) {
				(void)putc('\t', curf->stdfp);
				chcnt = cnt;
			}
			endcol += colwidth;
		}
		if (++row < numrows || b_num)
			(void)putc('\n', curf->stdfp);
	}

	for (row = 0; row < b_num;) {
		(void)opts_print(&opts[b_op[row]]);
		if (++row < b_num)
			(void)putc('\n', curf->stdfp);
	}
	(void)putc('\n', curf->stdfp);
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
 * opts_print --
 *	Print out an option.
 */
static int
opts_print(op)
	OPTIONS *op;
{
	int curlen;

	curlen = 0;
	switch (op->flags & OPT_TYPE) {
	case OPT_0BOOL:
		curlen += 2;
		(void)putc('n', curf->stdfp);
		(void)putc('o', curf->stdfp);
		/* FALLTHROUGH */
	case OPT_1BOOL:
		curlen += fprintf(curf->stdfp, "%s", op->name);
		break;
	case OPT_NUM:
		curlen += fprintf(curf->stdfp, "%s", op->name);
		curlen += 1;
		(void)putc('=', curf->stdfp);
		curlen += fprintf(curf->stdfp, "%ld", LVALP(op));
		break;
	case OPT_STR:
		curlen += fprintf(curf->stdfp, "%s", op->name);
		curlen += 1;
		(void)putc('=', curf->stdfp);
		curlen += 1;
		(void)putc('"', curf->stdfp);
		curlen += fprintf(curf->stdfp, "%s", PVALP(op));
		curlen += 1;
		(void)putc('"', curf->stdfp);
		break;
	}
	return (curlen);
}

int
opts_abbcmp(a, b)
        const void *a, *b;
{
        return(strcmp(((ABBREV *)a)->name, ((ABBREV *)b)->name));
}

int
opts_cmp(a, b)
        const void *a, *b;
{
        return(strcmp(((OPTIONS *)a)->name, ((OPTIONS *)b)->name));
}
