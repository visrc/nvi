/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: options.c,v 5.52 1993/03/25 15:00:20 bostic Exp $ (Berkeley) $Date: 1993/03/25 15:00:20 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "pathnames.h"
#include "screen.h"
#include "term.h"

static int	 opts_abbcmp __P((const void *, const void *));
static int	 opts_cmp __P((const void *, const void *));
static OPTIONS	*opts_prefix __P((char *));
static int	 opts_print __P((SCR *, OPTIONS *));

static long	s_columns	= 80;
static long	s_keytime	=  2;
static long	s_lines		= 24;
static long	s_report	=  5;
static long	s_scroll	= 12;
static long	s_shiftwidth	=  8;
static long	s_sidescroll	= 16;
static long	s_tabstop	=  8;
static long	s_wrapmargin	=  0;

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
	    OPT_NOSAVE|OPT_NUM|OPT_REDRAW,
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
	"errorbells",	NULL,		NULL,		OPT_0BOOL,
/* O_EXRC */
	"exrc",		NULL,		NULL,		OPT_0BOOL,
/* O_EXREFRESH */
	"exrefresh",	NULL,		NULL,		OPT_1BOOL,
/* O_EXTENDED */
	"extended",	NULL,		NULL,		OPT_0BOOL,
/* O_FLASH */
	"flash",	NULL,		f_flash,	OPT_1BOOL,
/* O_IGNORECASE */
	"ignorecase",	NULL,		NULL,		OPT_0BOOL,
/* O_KEYTIME */
	"keytime",	&s_keytime,	f_keytime,	OPT_NUM,
/* O_LEFTRIGHT */
	"leftright",	NULL,		f_leftright,	OPT_0BOOL,
/* O_LINES */
	"lines",	&s_lines,	f_lines,
	    OPT_NOSAVE|OPT_NUM|OPT_REDRAW,
/* O_LIST */
	"list",		NULL,		f_list,		OPT_0BOOL|OPT_REDRAW,
/* O_MAGIC */
	"magic",	NULL,		NULL,		OPT_1BOOL,
/* O_MAKE */
	"make",		"make",		NULL,		OPT_STR,
/* O_MESG */
	"mesg",		NULL,		f_mesg,		OPT_1BOOL,
/* O_MODELINES */
	"modelines",	NULL,		f_modelines,	OPT_0BOOL,
/* O_NUMBER */
	"number",	NULL,		NULL,		OPT_0BOOL|OPT_REDRAW,
/* O_NUNDO */
	"nundo",	NULL,		NULL,		OPT_0BOOL,
/* O_OPTIMIZE */
	"optimize",	NULL,		NULL,		OPT_1BOOL,
/* O_PARAGRAPHS */
	"paragraphs",
	    "IPLPPPQPP LIpplpipbp",	NULL,		OPT_STR,
/* O_PROMPT */
	"prompt",	NULL,		NULL,		OPT_1BOOL,
/* O_READONLY */
	"readonly",	NULL,		NULL,		OPT_0BOOL,
/* O_REDRAW */
	"redraw",	NULL,		NULL,		OPT_0BOOL,
/* O_REMAP */
	"remap",	NULL,		NULL,		OPT_1BOOL,
/* O_REPORT */
	"report",	&s_report,	NULL,		OPT_NUM,
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
/* O_SYNCCMD */
	"sync",		NULL,		NULL,		OPT_0BOOL,
/* O_TABSTOP */
	"tabstop",	&s_tabstop,	f_tabstop,	OPT_NUM|OPT_REDRAW,
/* O_TAGS */
	"tags",		NULL,		f_tags,		OPT_STR,
/* O_TERM */
	"term",		"unknown",	f_term,		OPT_NOSAVE|OPT_STR,
/* O_TERSE */
	"terse",	NULL,		NULL,		OPT_0BOOL,
/* O_TIMEOUT */
	"timeout",	NULL,		NULL,		OPT_0BOOL,
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
} OABBREV;

static OABBREV abbrev[] = {
	"ai",		O_AUTOINDENT,
	"ap",		O_AUTOPRINT,
	"at",		O_AUTOTAB,
	"aw",		O_AUTOWRITE,
	"bf",		O_BEAUTIFY,
	"cc",		O_CC,
	"co",		O_COLUMNS,
	"dig",		O_DIGRAPH,
	"dir",		O_DIRECTORY,
	"eb",		O_ERRORBELLS,
	"ed",		O_EDCOMPATIBLE,
	"ep",		O_EQUALPRG,
	"er",		O_EXREFRESH,
	"fl",		O_FLASH,
	"ic",		O_IGNORECASE,
	"kt",		O_KEYTIME,
	"li",		O_LIST,
	"ls",		O_LINES,
	"ma",		O_MAGIC,
	"me",		O_MESG,
	"mk",		O_MAKE,
	"modeline",	O_MODELINES,
	"nu",		O_NUMBER,
	"opt",		O_OPTIMIZE,
	"pa",		O_PARAGRAPHS,
	"pr",		O_PROMPT,
	"re",		O_REPORT,
	"ro",		O_READONLY,
	"ru",		O_RULER,
	"sc",		O_SCROLL,
	"se",		O_SECTIONS,
	"sh",		O_SHELL,
	"sm",		O_SHOWMATCH,
	"ss",		O_SIDESCROLL,
	"sw",		O_SHIFTWIDTH,
	"sy",		O_SYNCCMD,
	"te",		O_TERM,
	"to",		O_KEYTIME,
	"tr",		O_TERSE,
	"ts",		O_TABSTOP,
	"ve",		O_VERBOSE,
	"wa",		O_WARN,
	"wm",		O_WRAPMARGIN,
	"wr",		O_WRITEANY,
	"ws",		O_WRAPSCAN,
	NULL,
};

/*
 * opts_init --
 *	Initialize some of the options.  Since the user isn't really
 *	"setting" these variables, we don't set their OPT_SET bits.
 */
int
opts_init(sp)
	SCR *sp;
{
	char *s, *argv[2], b1[1024];

	(void)set_window_size(sp, 0);

	argv[0] = b1;
	argv[1] = NULL;
							/* O_SCROLL */
	(void)snprintf(b1, sizeof(b1), "sc=%ld", LVAL(O_LINES));
	(void)opts_set(sp, argv);
	FUNSET(O_SCROLL, OPT_SET);

	if (s = getenv("SHELL")) {			/* O_SHELL */
		(void)snprintf(b1, sizeof(b1), "shell=%s", s);
		(void)opts_set(sp, argv);
	}
	FUNSET(O_SHELL, OPT_SET);
							/* O_TAGS */
	(void)snprintf(b1, sizeof(b1), "tags=%s", _PATH_TAGS);
	(void)opts_set(sp, argv);
	FUNSET(O_TAGS, OPT_SET);

	(void)f_flash(NULL, NULL, NULL);		/* O_FLASH */
	FUNSET(O_FLASH, OPT_SET);
	return (0);
}

/*
 * opts_set --
 *	Change the values of one or more options.
 */
int
opts_set(sp, argv)
	SCR *sp;
	char **argv;
{
	register char *p;
	OABBREV atmp, *ap;
	OPTIONS otmp, *op;
	long value;
	int all, ch, off, rval;
	char *endp, *equals, *name;
	
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
		    sizeof(abbrev) / sizeof(OABBREV) - 1,
		    sizeof(OABBREV), opts_abbcmp)) != NULL) {
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
			goto prefix;

		/* Check list of abbreviations. */
		atmp.name = name;
		if ((ap = bsearch(&atmp, abbrev,
		    sizeof(abbrev) / sizeof(OABBREV) - 1,
		    sizeof(OABBREV), opts_abbcmp)) != NULL) {
			op = opts + ap->offset;
			goto found;
		}

		/* Check list of options. */
		otmp.name = name;
		if ((op = bsearch(&otmp, opts,
		    sizeof(opts) / sizeof(OPTIONS) - 1,
		    sizeof(OPTIONS), opts_cmp)) != NULL)
			goto found;

		/* Check for prefix match. */
prefix:		op = opts_prefix(name);

found:		if (op == NULL || off && !ISFSETP(op, OPT_0BOOL|OPT_1BOOL)) {
			msgq(sp, M_ERROR,
			    "no option %s: 'set all' gives all option values",
			    name);
			continue;
		}

		/* Set name, value. */
		if (ISFSETP(op, OPT_NOSET)) {
			msgq(sp, M_ERROR, "%s: may not be set", name);
			continue;
		}

		switch (op->flags & OPT_TYPE) {
		case OPT_0BOOL:
		case OPT_1BOOL:
			if (equals) {
				msgq(sp, M_ERROR,
				    "set: option [no]%s is a boolean", name);
				break;
			}
			FUNSETP(op, OPT_0BOOL | OPT_1BOOL);
			FSETP(op, (off ? OPT_0BOOL : OPT_1BOOL) | OPT_SET);
			if (op->func && op->func(sp, &off, NULL)) {
				rval = 1;
				break;
			}
			goto draw;
		case OPT_NUM:
			if (!equals) {
				msgq(sp, M_ERROR,
				    "set: option %s requires a value",
				    name);
				break;
			}
			value = strtol(equals, &endp, 10);
			if (*endp && !isspace(*endp)) {
				msgq(sp, M_ERROR,
				    "set %s: illegal number %s", name, equals);
				break;
			}
			if (op->func && op->func(sp, &value, equals)) {
				rval = 1;
				break;
			}
			LVALP(op) = value;
			FSETP(op, OPT_SET);
			goto draw;
		case OPT_STR:
			if (!equals) {
				msgq(sp, M_ERROR,
				    "set: option %s requires a value",
				    name);
				break;
			}
			if (op->func && op->func(sp, &value, equals)) {
				rval = 1;
				break;
			}
			if (ISFSETP(op, OPT_ALLOCATED))
				free(op->value);
			if ((op->value = strdup(equals)) == NULL) {
				msgq(sp, M_ERROR,
				    "Error: %s", strerror(errno));
				rval = 1;
				break;
			}
			FSETP(op, OPT_ALLOCATED | OPT_SET);
draw:			if (ISFSETP(op, OPT_REDRAW))
				F_SET(sp, S_REDRAW);
			break;
		default:
			abort();
		}
	}
	if (all)
		opts_dump(sp, 1);
	return (rval);
}

/*
 * opt_dump --
 *	List the current values of selected options.
 */
void
opts_dump(sp, all)
	SCR *sp;
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
	termwidth = (sp->cols - 1) / 2 & ~(tablen - 1);
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
	termwidth = sp->cols - 1;
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
			chcnt += opts_print(sp, &opts[s_op[base]]);
			if ((base += numrows) >= s_num)
				break;
			while ((cnt =
			    (chcnt + tablen & ~(tablen - 1))) <= endcol) {
				(void)putc('\t', sp->stdfp);
				chcnt = cnt;
			}
			endcol += colwidth;
		}
		if (++row < numrows || b_num)
			(void)putc('\n', sp->stdfp);
	}

	for (row = 0; row < b_num;) {
		(void)opts_print(sp, &opts[b_op[row]]);
		if (++row < b_num)
			(void)putc('\n', sp->stdfp);
	}
	(void)putc('\n', sp->stdfp);
}

/*
 * opts_save --
 *	Write the current configuration to a file.
 */
int
opts_save(sp, fp)
	SCR *sp;
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
			    "set %s=\"%s\"\n", op->name, op->value);
			break;
		}
	}
	return (ferror(fp));
}

/*
 * opts_print --
 *	Print out an option.
 */
static int
opts_print(sp, op)
	SCR *sp;
	OPTIONS *op;
{
	int curlen;

	curlen = 0;
	switch (op->flags & OPT_TYPE) {
	case OPT_0BOOL:
		curlen += 2;
		(void)putc('n', sp->stdfp);
		(void)putc('o', sp->stdfp);
		/* FALLTHROUGH */
	case OPT_1BOOL:
		curlen += fprintf(sp->stdfp, "%s", op->name);
		break;
	case OPT_NUM:
		curlen += fprintf(sp->stdfp, "%s", op->name);
		curlen += 1;
		(void)putc('=', sp->stdfp);
		curlen += fprintf(sp->stdfp, "%ld", LVALP(op));
		break;
	case OPT_STR:
		curlen += fprintf(sp->stdfp, "%s", op->name);
		curlen += 1;
		(void)putc('=', sp->stdfp);
		curlen += 1;
		(void)putc('"', sp->stdfp);
		curlen += fprintf(sp->stdfp, "%s", op->value);
		curlen += 1;
		(void)putc('"', sp->stdfp);
		break;
	}
	return (curlen);
}

/*
 * opts_prefix --
 *	Check to see if the name is the prefix of one (only one) option.
 *	If so, return that option.
 */
static OPTIONS *
opts_prefix(name)
	char *name;
{
	OPTIONS *op, *save_op;
	size_t len;

	save_op = NULL;
	len = strlen(name);
	for (op = opts; op->name != NULL; ++op)
		if (!memcmp(op->name, name, len)) {
			if (save_op != NULL)
				return (NULL);
			save_op = op;
		}
	return (save_op);
}

static int
opts_abbcmp(a, b)
        const void *a, *b;
{
        return(strcmp(((OABBREV *)a)->name, ((OABBREV *)b)->name));
}

static int
opts_cmp(a, b)
        const void *a, *b;
{
        return(strcmp(((OPTIONS *)a)->name, ((OPTIONS *)b)->name));
}
