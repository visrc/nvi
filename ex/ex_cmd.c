/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_cmd.c,v 5.10 1992/04/19 08:54:08 bostic Exp $ (Berkeley) $Date: 1992/04/19 08:54:08 $";
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
/* START_OPTION_DEF */
EXCMDLIST cmds[] = {
#define	C_BANG		0
	"!",		ex_bang,	E_ADDR2|E_EXRCOK|E_NL,
	    "s",	"[line [,line]] ! command",
#define	C_HASH		1
	"#",		ex_number,	E_ADDR2|E_NL|E_SETLAST,
	    "c1",	"[line [,line]] # [count] [l]",
#define	C_SUBAGAIN	2
	"&",		ex_subagain,	E_ADDR2,
	    "s",	"[line [,line]] & [options] [count] [flags]",
#define	C_STAR		3
	"*",		ex_at,		0,
	    "b",	"@ [buffer]",
#define	C_SHIFTL	4
	"<",		ex_shiftl,	E_ADDR2,
	    "c1",	"[line [,line]] < [count] [flags]",
#define	C_EQUAL		5
	"=",		ex_equal,	E_ADDR1,
	    "1",	"[line] = [flags]",
#define	C_SHIFTR	6
	">",		ex_shiftr,	E_ADDR2,
	    "c1",	"[line [,line]] > [count] [flags]",
#define	C_AT		7
	"@",		ex_at,		0,
	    "b",	"@ [buffer]",
#define	C_APPEND	8
	"append",	ex_append,	E_ADDR1|E_ZERO,
	    "!",	"[line] a[ppend][!]",
#define	C_ABBR		9
	"abbreviate", 	ex_abbr,	E_EXRCOK,
	    "s",	"ab[brev] word replace",
#define	C_ARGS		10
	"args",		ex_args,	E_EXRCOK,
	    "",		"ar[gs]",
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
	    "!f1o",	"cd[!] [directory]",
#define	C_CHDIR		14
	"chdir",	ex_cd,		E_EXRCOK,
	    "!f1o",	"chd[ir][!] [directory]",
#define	C_COPY		15
	"copy",		ex_copy,	E_ADDR2,
	    "l1",	"[line [,line]] co[py] line [flags]",
#undef	E_PERM
#define	C_DELETE	16
	"delete",	ex_delete,	E_ADDR2,
	    "bc1",	"[line [,line]] d[elete] [buffer] [count] [flags]",
#undef	E_PERM
#ifdef DEBUG
#define	E_PERM		0
#else
#define	E_PERM		E_NOPERM
#endif
#define	C_DEBUG		17
	"debug",	ex_debug,	E_ADDR1|E_PERM,
	    "!",	"[line] debug[!]",
#undef	E_PERM
#ifdef NO_DIGRAPH
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
#define	C_DIGRAPH	18
	"digraph",	ex_digraph,	E_EXRCOK|E_PERM,
	    "",		"digraph XXX",
#define	C_EDIT		19
	"edit",		ex_edit,	0,
	    "!+f1o",	"e[dit][!] [+cmd] [file]",
#undef	E_PERM
#ifdef NO_ERRLIST
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
#define	C_ERRLIST	20
	"errlist",	ex_errlist,	E_PERM,
	    "",		"errlist XXX",
#define	C_EX		21
	"ex",		ex_edit,	0,
	    "!+f1o",	"ex[!] [+cmd] [file]",
#define	C_FILE		22
	"file",		ex_file,	0,
	    "f10",	"f[ile] [name]",
#define	C_GLOBAL	23
	"global",	ex_global,	E_ADDR2_OR_0,
	    "s",	"[line [,line]] g[lobal] /pattern/ [commands]",
#define	C_INSERT	24
	"insert",	ex_append,	E_ADDR1,
	    "!",	"[line] i[nsert][!]",
#define	C_JOIN		25
	"join",		ex_join,	E_ADDR2,
	    "!c1",	"[line [,line]] j[oin][!] [count] [flags]",
#define	C_K		26
	"k",		ex_mark,	E_ADDR1,
	    "w1r",	"[line] k key",
#define	C_LIST		27
	"list",		ex_list,	E_ADDR2|E_NL|E_SETLAST,
	    "c1",	"[line [,line]] l[ist] [count] [#]",
#define	C_MOVE		28
	"move",		ex_move,	E_ADDR2,
	    "l",	"[line [,line]] m[ove] line",
#define	C_MARK		29
	"mark",		ex_mark,	E_ADDR1,
	    "w1r",	"[line] ma[rk] key",
#undef	E_PERM
#ifdef NO_ERRLIST
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
#define	C_MAKE		30
	"make",		ex_make,	E_PERM,
	    "",		"make XXX",
#define	C_MAP		31
	"map",		ex_map,		E_EXRCOK,
	    "s",	"map[!] [key replace]",
#undef	E_PERM
#ifdef NO_MKEXRC
#define	E_PERM		E_NOPERM
#else
#define	E_PERM		0
#endif
#define	C_MKEXRC	32
	"mkexrc",	ex_mkexrc,	E_PERM,
	    "!f1r",	"mkexrc[!] file",
#define	C_NEXT		33
	"next",		ex_next,	0,
	    "fN",	"n[ext][!] [file ...]",
#define	C_NUMBER	34
	"number",	ex_number,	E_ADDR2|E_NL|E_SETLAST,
	    "c1",	"[line [,line]] nu[mber] [count] [l]",
#define	C_PRINT		35
	"print",	ex_print,	E_ADDR2|E_NL|E_SETLAST,
	    "c1",	"[line [,line]] p[rint] [count] [#l]",
#define	C_PREVIOUS	36
	"previous",	ex_prev,	0,
	    "!fN",	"prev[ious][!] [file ...]",
#define	C_PUT		37
	"put",		ex_put,		E_ADDR1|E_ZERO,
	    "b",	"[line] pu[t] [buffer]",
#define	C_QUIT		38
	"quit",		ex_quit,	0,
	    "!",	"q[uit][!]",
#define	C_READ		39
	"read",		ex_read,	E_ADDR1|E_ZERO,
	    "s",	"[line] r[ead] [!cmd | [file]]",
#define	C_REWIND	40
	"rewind",	ex_rew,		0,
	    "!",	"rew[ind][!]",
#define	C_SUBSTITUTE	41
	"substitute",	ex_substitute,	E_ADDR2,
	    "s",
	    "[line [,line]] s[ubstitute] [/pat/repl/[options] [count] [flags]]",
#define	C_SET		42
	"set",		ex_set,		E_EXRCOK,
	    "wN",
	    "se[t] [option[=[value]]...] [nooption ...] [option? ...] [all]",
#define	C_SHELL		43
	"shell",	ex_shell,	E_NL,
	    "", 	"sh[ell]",
#define	C_SOURCE	44
	"source",	ex_source,	E_EXRCOK,
	    "f1r", 	"so[urce] file",
#define	C_T		45
	"t",		ex_move,	E_ADDR2,
	    "l1", 	"[line [,line]] t line [flags]",
#define	C_TAG		46
	"tag",		ex_tag,		0,
	    "!w1o", 	"ta[g][!] [string]",
#define	C_UNDO		47
	"undo",		ex_undo,	0,
	    "", 	"u[ndo]",
#define	C_UNABBREVIATE	48
	"unabbreviate",	ex_unabbr,	E_EXRCOK,
	    "w1r", 	"una[bbrev] word",
#define	C_UNMAP		49
	"unmap",	ex_unmap,	E_EXRCOK,
	    "!w1r", 	"unm[ap][!] key",
#define	C_VGLOBAL	50
	"vglobal",	ex_global,	E_ADDR2_OR_0,
	    "s", 	"[line [,line]] v[global] /pattern/ [commands]",
#define	C_VERSION	51
	"version",	ex_version,	E_EXRCOK,
	    "", 	"version",
#define	C_VISUAL	52
	"visual",	ex_visual,	E_ADDR2,
	    "2c1", 	"[line] vi[sual] [type] [count] [flags]",
#define	C_WRITE		53
	"write",	ex_write,	E_ADDR2_OR_0,
	    "s",	"[line [,line]] w[rite] [!cmd | [>>] [file]]",
#define	C_WQ		54
	"wq",		ex_wq,		E_ADDR2_OR_0|E_NL,
	    "!>f1o",	"[line [,line]] wq[!] [>>] [file]",
#define	C_XIT		55
	"xit",		ex_xit,		E_ADDR2_OR_0|E_NL,
	    "!f1o",	"[line [,line]] x[it][!] [file]",
#define	C_YANK		56
	"yank",		ex_yank,	E_ADDR2,
	    "bc",	"[line [,line]] ya[nk] [buffer] [count]",
	{NULL},
};
#define	O_OPTIONCOUNT	57
/* END_OPTION_DEF */
