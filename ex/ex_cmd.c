/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_cmd.c,v 5.2 1992/04/05 09:47:14 bostic Exp $ (Berkeley) $Date: 1992/04/05 09:47:14 $";
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
 * The syntax script works as follows:
 *	!		-- ! flag
 *	+		-- +cmd
 *	1		-- p, l, # flags
 *	2		-- -, ., +, ^ flags
 *	>		-- >> string
 *	b		-- buffer
 *	c		-- count
 *	f[N]|#[or]	-- file (a number or N, optional or required)
 *	l		-- line
 *	w[N]|#[or]	-- word (a number or N, optional or required)
 *	s		-- string (parse locally)
 */
/* START_OPTION_DEF */
CMDLIST cmds[] = {
#define	C_BANG		0
	"!",		ex_bang,	E_ADDR2|E_EXRCOK|E_NL,
	    "w1r",	"[line [,line]] ! command",
#define	C_SUBAGAIN	1
	"&",		ex_subagain,	E_ADDR2,
	    "s",	"[line [,line]] & [options] [count] [flags]",
#define	C_STAR		2
	"*",		ex_at,		0,
	    "b",	"@ [buffer]",
#define	C_SHIFTL	3
	"<",		ex_shiftl,	E_ADDR2,
	    "c1",	"[line [,line]] < [count] [flags]",
#define	C_EQUAL		4
	"=",		ex_equal,	E_ADDR1,
	    "1",	"[line] = [flags]",
#define	C_SHIFTR	5
	">",		ex_shiftr,	E_ADDR2,
	    "c1",	"[line [,line]] > [count] [flags]",
#define	C_AT		6
	"@",		ex_at,		0,
	    "b",	"@ [buffer]",
#define	C_APPEND	7
	"append",	ex_append,	E_ADDR1|E_ZERO,
	    "!",	"[line] a[ppend][!]",
#define	C_ABBR		8
	"abbreviate", 	ex_abbr,	E_EXRCOK,
	    "w2r",	"ab[brev] word replace",
#define	C_ARGS		9
	"args",		ex_args,	E_EXRCOK,
	    "",		"ar[gs]",
#undef	E_PERM
#ifdef DEBUG
#define	E_PERM		0
#else
#define	E_PERM		E_NOPERM
#endif
#define	C_BUG		10
	"bug",		ex_debug,	E_NL|E_PERM,
	    "",		"bug",
#define	C_CHANGE	11
	"change",	ex_change,	E_ADDR2,
	    "!c",	"[line [,line]] c[hange][!] [count]",
#undef	E_PERM
#ifdef	NO_ERRLIST
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
#define	C_CC		12
	"cc",		ex_cc,		E_PERM,
	    "",		"cc XXX",
#define	C_CD		13
	"cd",		ex_cd,		E_EXRCOK,
	    "f1o",	"cd[!] [directory]",
#define	C_CHDIR		14
	"chdir",	ex_cd,		E_EXRCOK,
	    "f1o",	"chd[ir][!] [directory]",
#define	C_COPY		15
	"copy",		ex_copy,	E_ADDR2,
	    "l1",	"[line [,line]] co[py] line [flags]",
#undef	E_PERM
#ifdef NO_COLOR
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
#define	C_COLOR		16
	"color",	ex_color,	E_EXRCOK|E_PERM,
	    "",		"color XXX",
#define	C_DELETE	17
	"delete",	ex_delete,	E_ADDR2,
	    "bc1",	"[line [,line]] d[elete] [buffer] [count] [flags]",
#undef	E_PERM
#ifdef DEBUG
#define	E_PERM		0
#else
#define	E_PERM		E_NOPERM
#endif
#define	C_DEBUG		18
	"debug",	ex_debug,	E_NL|E_PERM,
	    "",		"debug XXX",
#undef	E_PERM
#ifdef NO_DIGRAPH
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
#define	C_DIGRAPH	19
	"digraph",	ex_digraph,	E_EXRCOK|E_PERM,
	    "",		"digraph XXX",
#define	C_EDIT		20
	"edit",		ex_edit,	0,
	    "+f10",	"e[dit][!] [+cmd] [file]",
#undef	E_PERM
#ifdef NO_ERRLIST
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
#define	C_ERRLIST	21
	"errlist",	ex_errlist,	E_PERM,
	    "",		"errlist XXX",
#define	C_EX		22
	"ex",		ex_edit,	0,
	    "+f10",	"ex[!] [+cmd] [file]",
#define	C_FILE		23
	"file",		ex_file,	0,
	    "f10",	"f[ile] [name]",
#define	C_GLOBAL	24
	"global",	ex_global,	E_ADDR2,
	    "s",	"[line [,line]] g[lobal] /pattern/ [commands]",
#define	C_INSERT	25
	"insert",	ex_append,	E_ADDR1,
	    "!",	"[line] i[nsert][!]",
#define	C_JOIN		26
	"join",		ex_join,	E_ADDR2,
	    "!c1",	"[line [,line]] j[oin][!] [count] [flags]",
#define	C_K		27
	"k",		ex_mark,	E_ADDR1,
	    "w1r",	"[line] k key",
#define	C_LIST		28
	"list",		ex_list,	E_ADDR2|E_NL,
	    "c1",	"[line [,line]] l[ist] [count] [flags]",
#define	C_MOVE		29
	"move",		ex_move,	E_ADDR2,
	    "l",	"[line [,line]] m[ove] line",
#define	C_MARK		30
	"mark",		ex_mark,	E_ADDR1,
	    "w1r",	"[line] ma[rk] key",
#undef	E_PERM
#ifdef NO_ERRLIST
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
#define	C_MAKE		31
	"make",		ex_make,	E_PERM,
	    "",		"make XXX",
#define	C_MAP		32
	"map",		ex_map,		E_EXRCOK,
	    "s",	"map[!] [key replace]",
#undef	E_PERM
#ifdef NO_MKEXRC
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
#define	C_MKEXRC	33
	"mkexrc",	ex_mkexrc,	E_PERM,
	    "!f1r",	"mkexrc[!] file",
#define	C_NEXT		34
	"next",		ex_next,	0,
	    "fN",	"n[ext][!] [file ...]",
#define	C_NUMBER	35
	"number",	ex_number,	E_ADDR2|E_NL,
	    "c1",	"[line [,line]] nu[mber] [count] [flags]",
#define	C_PRINT		36
	"print",	ex_print,	E_ADDR2|E_NL,
	    "c1",	"[line [,line]] p[rint] [count] [flags]",
#define	C_PREVIOUS	37
	"previous",	ex_prev,	0,
	    "!fN",	"prev[ious][!] [file ...]",
#define	C_PUT		38
	"put",		ex_put,		E_ADDR1|E_ZERO,
	    "b",	"[line] pu[t] [buffer]",
#define	C_QUIT		39
	"quit",		ex_quit,	0,
	    "!",	"q[uit][!]",
#define	C_READ		40
	"read",		ex_read,	E_ADDR1|E_ZERO,
	    "!f1o",	"[line] r[ead][!] [file]",
#define	C_REWIND	41
	"rewind",	ex_rew,		0,
	    "!",	"rew[ind][!]",
#define	C_SUBSTITUTE	42
	"substitute",	ex_substitute,	E_ADDR2,
	    "s",
	    "[line [,line]] s[ubstitute] [/pat/repl/[options] [count] [flags]]",
#define	C_SET		43
	"set",		ex_set,		E_EXRCOK,
	    "wN",
	    "se[t] [option[=[value]]...] [nooption ...] [option? ...] [all]",
#define	C_SHELL		44
	"shell",	ex_shell,	E_NL,
	    "", 	"sh[ell]",
#define	C_SOURCE	45
	"source",	ex_source,	E_EXRCOK,
	    "f1r", 	"so[urce] file",
#define	C_T		46
	"t",		ex_move,	E_ADDR2,
	    "l1", 	"[line [,line]] t line [flags]",
#define	C_TAG		47
	"tag",		ex_tag,		0,
	    "!w1o", 	"ta[g][!] [string]",
#define	C_UNDO		48
	"undo",		ex_undo,	0,
	    "", 	"u[ndo]",
#define	C_UNABBREVIATE	49
	"unabbreviate",	ex_abbr,	E_EXRCOK,
	    "w1r", 	"una[bbrev] word",
#define	C_UNMAP		50
	"unmap",	ex_unmap,	E_EXRCOK,
	    "!w1r", 	"unm[ap][!] key",
#define	C_VGLOBAL	51
	"vglobal",	ex_global,	E_ADDR2,
	    "s", 	"[line [,line]] v[global] /pattern/ [commands]",
#define	C_VALIDATE	52
#undef	E_PERM
#ifdef DEBUG
#define	E_PERM		0
#else
#define	E_PERM		E_NOPERM
#endif
	"validate",	ex_validate,	E_NL|E_PERM,
	    "", 	"validate XXX",
#define	C_VERSION	53
	"version",	ex_version,	E_EXRCOK,
	    "", 	"version",
#define	C_VISUAL	54
	"visual",	ex_visual,	E_ADDR1,
	    "2c1", 	"[line] vi[sual] [type] [count] [flags]",
#define	C_WRITE		55
	"write",	ex_write,	E_ADDR2,
	    "!>f1o",	"[line [,line]] w[rite][!] [>>] [file]",
#define	C_WQ		56
	"wq",		ex_wq,		E_ADDR2|E_NL,
	    "!>f1o",	"[line [,line]] wq[!] [>>] [file]",
#define	C_XIT		57
	"xit",		ex_xit,		E_ADDR2|E_NL,
	    "!f1o",	"[line [,line]] x[it][!] [file]",
#define	C_YANK		58
	"yank",		ex_yank,	E_ADDR2,
	    "bc",	"[line [,line]] ya[nk] [buffer] [count]",
	{NULL},
};
#define	O_OPTIONCOUNT	59
/* END_OPTION_DEF */
