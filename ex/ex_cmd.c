/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_cmd.c,v 5.14 1992/05/23 09:34:44 bostic Exp $ (Berkeley) $Date: 1992/05/23 09:34:44 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

/*
 * This array maps ex command names to command functions.
 *
 * The order in which command names are listed below is important --
 * ambiguous abbreviations are resolved to be the first possible match,
 * e.g. "r" means "read", not "rewind", because "read" is listed before
 * "rewind").
 *
 * The syntax of the ex commands is unbelievably irregular.  Stupid is
 * another description that leaps to mind.  In any case, it's a special
 * case from one end to the other.  Each command has a "syntax script"
 * associated with it, which describes the items that are possible.  The
 * script syntax is as follows:
 *	!		-- ! flag
 *	+		-- +cmd
 *	1		-- flags: [+-]*[pl#][+-]*
 *	2		-- flags: [-.+^]
 *	>		-- >> string
 *	b		-- buffer
 *	c		-- count
 *	f[N]|#[or]	-- file (a number or N, optional or required)
 *	l		-- line
 *	w[N]|#[or]	-- word (a number or N, optional or required)
 *	s		-- string (parse locally)
 */
EXCMDLIST cmds[] = {
/* C_BANG */
	"!",		ex_bang,	E_ADDR2_NONE|E_EXRCOK,
	    "s",	"[line [,line]] ! command",
/* C_HASH */
	"#",		ex_number,	E_ADDR2|E_SETLAST,
	    "c1",	"[line [,line]] # [count] [l]",
/* C_SUBAGAIN */
	"&",		ex_subagain,	E_ADDR2,
	    "s",	"[line [,line]] & [options] [count] [flags]",
/* C_STAR */
	"*",		ex_at,		0,
	    "b",	"@ [buffer]",
/* C_SHIFTL */
	"<",		ex_shiftl,	E_ADDR2,
	    "c1",	"[line [,line]] < [count] [flags]",
/* C_EQUAL */
	"=",		ex_equal,	E_ADDR1,
	    "1",	"[line] = [flags]",
/* C_SHIFTR */
	">",		ex_shiftr,	E_ADDR2,
	    "c1",	"[line [,line]] > [count] [flags]",
/* C_AT */
	"@",		ex_at,		0,
	    "b",	"@ [buffer]",
/* C_APPEND */
	"append",	ex_append,	E_ADDR1|E_ZERO,
	    "!",	"[line] a[ppend][!]",
/* C_ABBR */
	"abbreviate", 	ex_abbr,	E_EXRCOK,
	    "s",	"ab[brev] word replace",
/* C_ARGS */
	"args",		ex_args,	E_EXRCOK,
	    "",		"ar[gs]",
/* C_CHANGE */
	"change",	ex_change,	E_ADDR2,
	    "!c",	"[line [,line]] c[hange][!] [count]",
#ifdef	NO_ERRLIST
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
/* C_CC */
	"cc",		ex_cc,		E_PERM,
	    "s",	"cc [argument ...]",
/* C_CD */
	"cd",		ex_cd,		E_EXRCOK,
	    "!f1o",	"cd[!] [directory]",
/* C_CHDIR */
	"chdir",	ex_cd,		E_EXRCOK,
	    "!f1o",	"chd[ir][!] [directory]",
/* C_COPY */
	"copy",		ex_copy,	E_ADDR2,
	    "l1",	"[line [,line]] co[py] line [flags]",
/* C_DELETE */
	"delete",	ex_delete,	E_ADDR2,
	    "bc1",	"[line [,line]] d[elete] [buffer] [count] [flags]",
#ifdef NO_DIGRAPH
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
/* C_DIGRAPH */
	"digraph",	ex_digraph,	E_EXRCOK|E_PERM,
	    "",		"digraph XXX",
/* C_EDIT */
	"edit",		ex_edit,	0,
	    "!+f1o",	"e[dit][!] [+cmd] [file]",
#ifdef NO_ERRLIST
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
/* C_ERRLIST */
	"errlist",	ex_errlist,	E_PERM,
	    "f1o",	"errlist [file]",
/* C_EX */
	"ex",		ex_edit,	0,
	    "!+f1o",	"ex[!] [+cmd] [file]",
/* C_FILE */
	"file",		ex_file,	0,
	    "f10",	"f[ile] [name]",
/* C_GLOBAL */
	"global",	ex_global,	E_ADDR2_ALL,
	    "s",	"[line [,line]] g[lobal] /pattern/ [commands]",
/* C_INSERT */
	"insert",	ex_append,	E_ADDR1,
	    "!",	"[line] i[nsert][!]",
/* C_JOIN */
	"join",		ex_join,	E_ADDR2,
	    "!c1",	"[line [,line]] j[oin][!] [count] [flags]",
/* C_K */
	"k",		ex_mark,	E_ADDR1,
	    "w1r",	"[line] k key",
/* C_LIST */
	"list",		ex_list,	E_ADDR2|E_SETLAST,
	    "c1",	"[line [,line]] l[ist] [count] [#]",
/* C_MOVE */
	"move",		ex_move,	E_ADDR2,
	    "l",	"[line [,line]] m[ove] line",
/* C_MARK */
	"mark",		ex_mark,	E_ADDR1,
	    "w1r",	"[line] ma[rk] key",
#ifdef NO_ERRLIST
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
/* C_MAKE */
	"make",		ex_make,	E_PERM,
	    "s",	"make [argument ...]",
/* C_MAP */
	"map",		ex_map,		E_EXRCOK,
	    "s",	"map[!] [key replace]",
#ifdef NO_MKEXRC
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
/* C_MKEXRC */
	"mkexrc",	ex_mkexrc,	E_PERM,
	    "!f1r",	"mkexrc[!] file",
/* C_NEXT */
	"next",		ex_next,	0,
	    "fN",	"n[ext][!] [file ...]",
/* C_NUMBER */
	"number",	ex_number,	E_ADDR2|E_SETLAST,
	    "c1",	"[line [,line]] nu[mber] [count] [l]",
/* C_PRINT */
	"print",	ex_print,	E_ADDR2|E_SETLAST,
	    "c1",	"[line [,line]] p[rint] [count] [#l]",
/* C_PREVIOUS */
	"previous",	ex_prev,	0,
	    "!fN",	"prev[ious][!] [file ...]",
/* C_PUT */
	"put",		ex_put,		E_ADDR1|E_ZERO,
	    "b",	"[line] pu[t] [buffer]",
/* C_QUIT */
	"quit",		ex_quit,	0,
	    "!",	"q[uit][!]",
/* C_READ */
	"read",		ex_read,	E_ADDR1|E_ZERO,
	    "s",	"[line] r[ead] [!cmd | [file]]",
/* C_REWIND */
	"rewind",	ex_rew,		0,
	    "!",	"rew[ind][!]",
/* C_SUBSTITUTE */
	"substitute",	ex_substitute,	E_ADDR2,
	    "s",
	    "[line [,line]] s[ubstitute] [/pat/repl/[options] [count] [flags]]",
/* C_SET */
	"set",		ex_set,		E_EXRCOK,
	    "wN",
	    "se[t] [option[=[value]]...] [nooption ...] [option? ...] [all]",
/* C_SHELL */
	"shell",	ex_shell,	0,
	    "", 	"sh[ell]",
/* C_SOURCE */
	"source",	ex_source,	E_EXRCOK,
	    "f1r", 	"so[urce] file",
/* C_T */
	"t",		ex_move,	E_ADDR2,
	    "l1", 	"[line [,line]] t line [flags]",
/* C_TAG */
	"tag",		ex_tag,		0,
	    "!w1o", 	"ta[g][!] [string]",
/* C_UNDO */
	"undo",		ex_undo,	0,
	    "", 	"u[ndo]",
/* C_UNABBREVIATE */
	"unabbreviate",	ex_unabbr,	E_EXRCOK,
	    "w1r", 	"una[bbrev] word",
/* C_UNMAP */
	"unmap",	ex_unmap,	E_EXRCOK,
	    "!w1r", 	"unm[ap][!] key",
/* C_VGLOBAL */
	"vglobal",	ex_global,	E_ADDR2_ALL,
	    "s", 	"[line [,line]] v[global] /pattern/ [commands]",
/* C_VERSION */
	"version",	ex_version,	E_EXRCOK,
	    "", 	"version",
/* C_VISUAL */
	"visual",	ex_visual,	E_ADDR2,
	    "2c1", 	"[line] vi[sual] [type] [count] [flags]",
/* C_WRITE */
	"write",	ex_write,	E_ADDR2_ALL,
	    "s",	"[line [,line]] w[rite] [!cmd | [>>] [file]]",
/* C_WQ */
	"wq",		ex_wq,		E_ADDR2_ALL,
	    "!>f1o",	"[line [,line]] wq[!] [>>] [file]",
/* C_XIT */
	"xit",		ex_xit,		E_ADDR2_ALL,
	    "!f1o",	"[line [,line]] x[it][!] [file]",
/* C_YANK */
	"yank",		ex_yank,	E_ADDR2,
	    "bc",	"[line [,line]] ya[nk] [buffer] [count]",
	{NULL},
};
