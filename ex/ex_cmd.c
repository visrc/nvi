/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_cmd.c,v 8.11 1993/09/08 18:06:59 bostic Exp $ (Berkeley) $Date: 1993/09/08 18:06:59 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

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
 *	f[N#][or]	-- file (a number or N, optional or required)
 *	l		-- line
 *	s		-- string
 *	W		-- word string
 *	w[N#][or]	-- word (a number or N, optional or required)
 */
EXCMDLIST const cmds[] = {
/* C_BANG */
	{"!",		ex_bang,	E_ADDR2_NONE|E_NORC,
	    "s",	"[line [,line]] ! command"},
/* C_HASH */
	{"#",		ex_number,	E_ADDR2|E_F_PRCLEAR|E_NORC|E_SETLAST,
	    "c1",	"[line [,line]] # [count] [l]"},
/* C_SUBAGAIN */
	{"&",		ex_subagain,	E_ADDR2|E_NORC,
	    "s",	"[line [,line]] & [options] [count] [flags]"},
/* C_STAR */
	{"*",		ex_at,		0,
	    "b",	"@ [buffer]"},
/* C_SHIFTL */
	{"<",		ex_shiftl,	E_ADDR2|E_NORC,
	    "c1",	"[line [,line]] <[<...] [count] [flags]"},
/* C_EQUAL */
	{"=",		ex_equal,	E_ADDR1|E_NORC,
	    "1",	"[line] = [flags]"},
/* C_SHIFTR */
	{">",		ex_shiftr,	E_ADDR2|E_NORC,
	    "c1",	"[line [,line]] >[>...] [count] [flags]"},
/* C_AT */
	{"@",		ex_at,		0,
	    "b",	"@ [buffer]"},
/* C_APPEND */
	{"append",	ex_append,	E_ADDR1|E_NORC|E_ZERO|E_ZERODEF,
	    "!",	"[line] a[ppend][!]"},
/* C_ABBR */
	{"abbreviate", 	ex_abbr,	E_NOGLOBAL,
	    "W",	"ab[brev] word replace"},
/* C_ARGS */
	{"args",	ex_args,	E_NOGLOBAL|E_NORC,
	    "",		"ar[gs]"},
/* C_BDISPLAY */
	{"bdisplay",	ex_bdisplay,	E_NOGLOBAL|E_NORC,
	    "",		"[b]display"},
/* C_CHANGE */
	{"change",	ex_change,	E_ADDR2|E_NORC|E_ZERODEF,
	    "!c",	"[line [,line]] c[hange][!] [count]"},
/* C_CC */
	{"cc",		ex_cc,		E_NOGLOBAL|E_NORC,
	    "s",	"cc [argument ...]"},
/* C_CD */
	{"cd",		ex_cd,		E_NOGLOBAL,
	    "!f1o",	"cd[!] [directory]"},
/* C_CHDIR */
	{"chdir",	ex_cd,		E_NOGLOBAL,
	    "!f1o",	"chd[ir][!] [directory]"},
/* C_COPY */
	{"copy",	ex_copy,	E_ADDR2|E_NORC,
	    "l1",	"[line [,line]] co[py] line [flags]"},
/* C_DELETE */
	{"delete",	ex_delete,	E_ADDR2|E_NORC,
	    "bc1",	"[line [,line]] d[elete] [buffer] [count] [flags]"},
/* C_DIGRAPH */
	{"digraph",	ex_digraph,	E_NOGLOBAL|E_NOPERM|E_NORC,
	    "",		"digraph XXX"},
/* C_EDIT */
	{"edit",	ex_edit,	E_NOGLOBAL|E_NORC,
	    "!+f1o",	"e[dit][!] [+cmd] [file]"},
/* C_ERRLIST */
	{"errlist",	ex_errlist,	E_NOGLOBAL|E_NORC,
	    "f1o",	"errlist [file]"},
/* C_EX */
	{"ex",		ex_edit,	E_NOGLOBAL|E_NORC,
	    "!+f1o",	"ex[!] [+cmd] [file]"},
/* C_EXUSAGE */
	{"exusage",	ex_usage,	E_NOGLOBAL|E_NORC,
	    "w1r",	"[exu]sage command"},
/* C_FILE */
	{"file",	ex_file,	E_NOGLOBAL|E_NORC,
	    "f1o",	"f[ile] [name]"},
/* C_GLOBAL */
	{"global",	ex_global,	E_ADDR2_ALL|E_NOGLOBAL|E_NORC,
	    "!s",
	    "[line [,line]] g[lobal][!] [;/]pattern[;/] [commands]"},
/* C_INSERT */
	{"insert",	ex_append,	E_ADDR1|E_NORC,
	    "!",	"[line] i[nsert][!]"},
/* C_JOIN */
	{"join",	ex_join,	E_ADDR2|E_NORC,
	    "!c1",	"[line [,line]] j[oin][!] [count] [flags]"},
/* C_K */
	{"k",		ex_mark,	E_ADDR1|E_NORC,
	    "w1r",	"[line] k key"},
/* C_LIST */
	{"list",	ex_list,	E_ADDR2|E_F_PRCLEAR|E_NORC|E_SETLAST,
	    "c1",	"[line [,line]] l[ist] [count] [#]"},
/* C_MOVE */
	{"move",	ex_move,	E_ADDR2|E_NORC,
	    "l",	"[line [,line]] m[ove] line"},
/* C_MARK */
	{"mark",	ex_mark,	E_ADDR1|E_NORC,
	    "w1r",	"[line] ma[rk] key"},
/* C_MAKE */
	{"make",	ex_make,	E_NOGLOBAL|E_NORC|E_NOPERM,
	    "s",	"make [argument ...]"},
/* C_MAP */
	{"map",		ex_map,		0,
	    "!W",	"map[!] [key replace]"},
/* C_MKEXRC */
	{"mkexrc",	ex_mkexrc,	E_NOGLOBAL|E_NORC,
	    "!f1r",	"mkexrc[!] file"},
/* C_NEXT */
	{"next",	ex_next,	E_NOGLOBAL|E_NORC,
	    "!fN",	"n[ext][!] [file ...]"},
/* C_NUMBER */
	{"number",	ex_number,	E_ADDR2|E_F_PRCLEAR|E_NORC|E_SETLAST,
	    "c1",	"[line [,line]] nu[mber] [count] [l]"},
/* C_PRINT */
	{"print",	ex_pr,		E_ADDR2|E_F_PRCLEAR|E_NORC|E_SETLAST,
	    "c1",	"[line [,line]] p[rint] [count] [#l]"},
/* C_PRESERVE */
	{"preserve",	ex_preserve,	E_NOGLOBAL|E_NORC,
	    "",		"pre[serve]"},
/* C_PREVIOUS */
	{"previous",	ex_prev,	E_NOGLOBAL|E_NORC,
	    "!",	"prev[ious][!]"},
/* C_PUT */
	{"put",		ex_put,		E_ADDR1|E_NORC|E_ZERO,
	    "b",	"[line] pu[t] [buffer]"},
/* C_QUIT */
	{"quit",	ex_quit,	E_NOGLOBAL,
	    "!",	"q[uit][!]"},
/* C_READ */
	{"read",	ex_read,	E_ADDR1|E_NORC|E_ZERO|E_ZERODEF,
	    "!s",	"[line] r[ead] [!cmd | [file]]"},
/* C_REWIND */
	{"rewind",	ex_rew,		E_NOGLOBAL|E_NORC,
	    "!",	"rew[ind][!]"},
/* C_SUBSTITUTE */
	{"substitute",	ex_substitute,	E_ADDR2|E_NORC,
	    "s",
	"[line [,line]] s[ubstitute] [[/;]pat[/;]/repl[/;] [count] [#cglpr]]"},
/* C_SET */
	{"set",		ex_set,		E_NOGLOBAL,
	    "wN",
	    "se[t] [option[=[value]]...] [nooption ...] [option? ...] [all]"},
/* C_SHELL */
	{"shell",	ex_shell,	E_NOGLOBAL|E_NORC,
	    "", 	"sh[ell]"},
/* C_SPLIT */
	{"split",	ex_split,	E_NOGLOBAL|E_NORC,
	    "fNo",	"sp[lit] [file ...]"},
/* C_SOURCE */
	{"source",	ex_source,	E_NOGLOBAL,
	    "f1r", 	"so[urce] file"},
/* C_STOP */
	{"stop",	ex_stop,	E_NOGLOBAL|E_NORC,
	    "!",	"st[op][!]"},
/* C_T */
	{"t",		ex_move,	E_ADDR2|E_NORC,
	    "l1", 	"[line [,line]] t line [flags]"},
/* C_TAG */
	{"tag",		ex_tagpush,	E_NOGLOBAL,
	    "!w1o", 	"ta[g][!] [string]"},
/* C_TAGPOP */
	{"tagpop",	ex_tagpop,	E_NOGLOBAL|E_NORC,
	    "!", 	"tagp[op][!]"},
/* C_TAGTOP */
	{"tagtop",	ex_tagtop,	E_NOGLOBAL|E_NORC,
	    "!", 	"tagt[op][!]"},
/* C_UNDOL */
	{"Undo",	ex_undol,	E_NOGLOBAL|E_NORC,
	    "", 	"U[ndo]"},
/* C_UNDO */
	{"undo",	ex_undo,	E_NOGLOBAL|E_NORC,
	    "", 	"u[ndo]"},
/* C_UNABBREVIATE */
	{"unabbreviate",ex_unabbr,	E_NOGLOBAL,
	    "w1r", 	"una[bbrev] word"},
/* C_UNMAP */
	{"unmap",	ex_unmap,	E_NOGLOBAL,
	    "!w1r", 	"unm[ap][!] word"},
/* C_VGLOBAL */
	{"vglobal",	ex_vglobal,	E_ADDR2_ALL|E_NOGLOBAL|E_NORC,
	    "s", 	"[line [,line]] v[global] [;/]pattern[;/] [commands]"},
/* C_VERSION */
	{"version",	ex_version,	E_NOGLOBAL|E_NORC,
	    "", 	"version"},
/* C_VISUAL_EX */
	{"visual",	ex_visual,	E_ADDR1|E_NOGLOBAL|E_NORC,
	    "2c1", 	"[line] vi[sual] [-|.|+|^] [window_size] [flags]"},
/* C_VISUAL_VI */
	{"visual",	ex_edit,	E_NOGLOBAL|E_NORC,
	    "!+f1o",	"vi[sual][!] [+cmd] [file]"},
/* C_VIUSAGE */
	{"viusage",	ex_viusage,	E_NOGLOBAL|E_NORC,
	    "w1r",	"[viu]sage key"},
/* C_WRITE */
	{"write",	ex_write,	E_ADDR2_ALL|E_NOGLOBAL|E_NORC|E_ZERODEF,
	    "!s",	"[line [,line]] w[rite][!] [!cmd | [>>] [file]]"},
/* C_WQ */
	{"wq",		ex_wq,		E_ADDR2_ALL|E_NOGLOBAL|E_NORC|E_ZERODEF,
	    "!s",	"[line [,line]] wq[!] [>>] [file]"},
/* C_XIT */
	{"xit",		ex_xit,		E_ADDR2_ALL|E_NOGLOBAL|E_NORC|E_ZERODEF,
	    "!f1o",	"[line [,line]] x[it][!] [file]"},
/* C_YANK */
	{"yank",	ex_yank,	E_ADDR2|E_NORC,
	    "bc",	"[line [,line]] ya[nk] [buffer] [count]"},
	{NULL},
};
