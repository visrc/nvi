/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_cmd.c,v 5.25 1992/09/01 15:35:37 bostic Exp $ (Berkeley) $Date: 1992/09/01 15:35:37 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>

#define	VIROUTINE		/* XXX */

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * This array maps keystrokes to vi command functions.
 */
VIKEYS vikeys[MAXVIKEY + 1] = {
/* 000 NUL -- The code in vi.c expects key 0 to be undefined. */
	{},
/* 001  ^A */
	v_searchw,	V_ABS|V_CNT|V_KEYW,
	    "search forward for cursor word: [count]^A",
/* 002  ^B */
	v_pageup,	V_CNT,
	    "page up by screens: [count]^B",
/* 003  ^C */
	{},
/* 004  ^D */
	v_hpagedown,	V_CNT,
	    "page down by half screens (set count): [count]^D",	
/* 005  ^E */
	v_linedown,	V_CNT,
	    "page down by lines: [count]^E",
/* 006  ^F */
	v_pagedown,	V_CNT,
	    "page down by screens: [count]^F",
/* 007  ^G */
	v_status,	0,
	    "file status: ^G",
/* 010  ^H */
	v_left,		V_CNT|V_MOVE,
	    "move left by columns: [count]^H",
/* 011  ^I */
	{},
/* 012  ^J */
	v_down,		V_CNT|V_LMODE|V_MOVE,
	    "move down by lines: [count]^J",
/* 013  ^K */
	{},
/* 014  ^L */
	v_redraw,	0,
	    "redraw screen: ^L",
/* 015  ^M */
	v_nbdown,	V_CNT|V_LMODE|V_MOVE,
	    "move down by lines (first non-blank): [count]^M",
/* 016  ^N */
	v_down,		V_CNT|V_LMODE|V_MOVE,
	    "move down by lines: [count]^N",
/* 017  ^O */
	{},
/* 020  ^P */
	v_up,		V_CNT|V_LMODE|V_MOVE,
	    "move up by lines: [count]^P",
/* 021  ^Q */
	{},
/* 022  ^R */
	v_redraw,	0,
	    "redraw screen: ^R",
/* 023  ^S */
	{},
/* 024  ^T */
	{},
/* 025  ^U */
	v_hpageup,	V_CNT,
	    "half page up (set count): [count]^U",
/* 026  ^V */
	{},
/* 027  ^W */
	{},
/* 030  ^X */
	{},
/* 031  ^Y */
	v_lineup,	V_CNT,
	    "page up by lines: [count]^Y",
/* 032  ^Z */
	v_stop, 0,
	    "suspend: ^Z",
/* 033  ^[ */
	{},
/* 034  ^\ */
	{},
/* 035  ^] */
	v_tag,		V_KEYW,
	    "tag cursor word: ^]",
/* 036  ^^ */
	v_switch,	0,
	    "change to previous file: ^^",
/* 037  ^_ */
	{},
/* 040 ' ' */
	v_right,	V_CNT|V_MOVE,
	    "move right by columns: [count]' '",
/* 041   ! */
	v_filter,	V_CNT|V_DOT|V_MOTION,
	    "filter through command(s): [count]![count]motion command(s)",
/* 042   " */
	{},
/* 043   # */
	v_increment,	V_CHAR|V_CNT|V_DOT|V_KEYNUM,
	    "number increment/decrement: [count]#[#+-]",
/* 044   $ */
	v_eol,		V_CNT|V_MOVE,
	    "move to last column: [count]$",
/* 045   % */
	v_match,	V_MOVE,
	    "move to match: %",
/* 046   & */
	v_again,	0,
	    "repeat substitution: &",
/* 047   ' */
	v_marksq,	V_CHAR|V_LMODE|V_MOVE,
	    "move to mark (first non-blank): '['a-z]",
/* 050   ( */
	v_sentenceb,	V_CNT|V_MOVE,
	    "move back sentence: [count](",
/* 051   ) */
	v_sentencef,	V_CNT|V_MOVE,
	    "move forward sentence: [count])",
/* 052   * */
	v_errlist,	0,
	    "step through compile errors: *",
/* 053   + */
	v_nbdown,	V_CNT|V_LMODE|V_MOVE,
	    "move down by lines (first non-blank): [count]+",
/* 054   , */
	v_chrrepeat,	V_CNT|V_MOVE,
	    "reverse last F, f, T or t search: [count],",
/* 055   - */
	v_nbup,		V_CNT|V_LMODE|V_MOVE,
	    "move up by lines (first non-blank): [count]-",
/* 056   . */
	{},
/* 057   / */
	v_searchf,	V_MOVE,
	    "search forward: /RE",
/* 060   0 */
	v_zero,		V_MOVE,
	    "move to first character: 0",
/* 061   1 */
	{},
/* 062   2 */
	{},
/* 063   3 */
	{},
/* 064   4 */
	{},
/* 065   5 */
	{},
/* 066   6 */
	{},
/* 067   7 */
	{},
/* 070   8 */
	{},
/* 071   9 */
	{},
/* 072   : */
	v_ex,		0,
	    "ex command: :command [| command]",
/* 073   ; */
	v_chrepeat,	V_CNT|V_MOVE,
	    "repeat last F, f, T or t search: [count];",
/* 074   < */
	v_shiftl,	V_CNT|V_DOT|V_MOTION,
	    "shift lines left: [count]<[count]motion",
/* 075   = */
	{},
/* 076   > */
	v_shiftr,	V_CNT|V_DOT|V_MOTION,
	    "shift lines right: [count]>[count]motion",
/* 077   ? */
	v_searchb,	V_MOVE,
	    "search backward: /RE",
/* 100   @ */
	v_at,		V_RBUF,	"execute buffer: @buffer",
/* 101   A */
	v_iA,		V_CNT|V_DOT,
	    "append to line: [count]A",
/* 102   B */
	v_wordB,	V_CNT|V_MOVE,
	    "move back bigword: [count]B",
/* 103   C */
	v_Change,	V_CNT|V_DOT|V_OBUF,
	    "change to end-of-line: [buffer][count]C",
/* 104   D */
	v_Delete,	V_CNT|V_DOT|V_OBUF,
	    "delete to end-of-line: [buffer][count]D",
/* 105   E */
	v_wordE,	V_CNT|V_MOVE,
	    "move to end of bigword: [count]E",
/* 106   F */
	v_chF,		V_CHAR|V_CNT|V_MOVE,
	    "character in line backward search: [count]F character",
/* 107   G */
	v_lgoto,	V_ABS|V_CNT|V_LMODE|V_MOVE,
	    "move to line: [count]G",
/* 110   H */
	v_home,		V_CNT|V_LMODE,
	    "move to count lines from screen top: [count]H",
/* 111   I */
	v_iI,		V_CNT|V_DOT,
	    "insert at line beginning: [count]I",
/* 112   J */
	v_join,		V_CNT|V_DOT,
	    "join lines: [count]J",
/* 113   K */
	{},
/* 114   L */
	v_bottom,	V_CNT|V_LMODE,
	    "move to screen bottom: [count]L",
/* 115   M */
	v_middle,	V_LMODE,
	    "move to screen middle: M",
/* 116   N */
	v_searchN,	V_MOVE,
	    "reverse last search: n",
/* 117   O */
	v_iO,		V_CNT|V_DOT,
	    "insert above line: [count]O",
/* 120   P */
	v_Put,		V_CNT|V_DOT|V_OBUF,
	    "insert before cursor from buffer: [buffer]P",
/* 121   Q */
	v_quit,		0,
	    "switch to ex mode: Q",
/* 122   R */
	v_Replace,	V_DOT,
	    "replace characters: [count]R",
/* 123   S */
	v_change,	V_CNT|V_DOT|V_OBUF,
	    "substitute lines: [buffer][count]S",
/* 124   T */
	v_chT,		V_CHAR|V_CNT|V_MOVE,
	    "before character in line backward search: [count]T character",
/* 125   U */
	v_undoline,	0,
	    "undo current line: U",
/* 126   V */
	v_start,	0,
	    "start visible line: V",
/* 127   W */
	v_wordW,	V_CNT|V_MOVE,
	    "move to next bigword: [count]W",
/* 130   X */
	v_Xchar,	V_CNT|V_DOT|V_OBUF,
	    "delete character before cursor: [buffer][count]X",
/* 131   Y */
	v_yank,		V_CNT|V_LMODE|V_OBUF,
	    "copy line: [buffer][count]Y",
/* 132   Z */
	v_exit,		0,
	    "save file and exit: ZZ",
/* 133   [ */
	v_sectionb,	V_ABS|V_MOVE,
	    "move back section: ]]",
/* 134   \ */
	{},
/* 135   ] */
	v_sectionf,	V_ABS|V_MOVE,
	    "move forward section: [[",
/* 136   ^ */
	v_first,	V_MOVE,
	    "move to first non-blank: ^",
/* 137   _ */
	v_first,	V_MOVE,
	    "move to first non-blank: _",
/* 140   ` */
	v_markbt,	V_CHAR|V_MOVE,
	    "move to mark: `[`a-z]",
/* 141   a */
	v_ia,		V_CNT|V_DOT,
	    "append after cursor: [count]a",
/* 142   b */
	v_wordb,	V_CNT|V_MOVE,
	    "move back word: [count]b",
/* 143   c */
	v_change,	V_CNT|V_DOT|V_MOTION|V_OBUF|VC_C,
	    "change: [buffer][count]c[count]motion",
/* 144   d */
	v_delete,	V_CNT|V_DOT|V_MOTION|V_OBUF|VC_D,
	    "delete: [buffer][count]d[count]motion",
/* 145   e */
	v_worde,	V_CNT|V_MOVE,
	    "move to end of word: [count]e",
/* 146   f */
	v_chf,		V_CHAR|V_CNT|V_MOVE,
	    "character in line forward search: [count]f character",
/* 147   g */
	{},
/* 150   h */
	v_left,		V_CNT|V_MOVE,
	    "move left by columns: [count]h",
/* 151   i */
	v_ii,		V_CNT|V_DOT,
	    "insert before cursor: [count]i",
/* 152   j */
	v_down,		V_CNT|V_LMODE|V_MOVE,
	    "move down by lines: [count]j",
/* 153   k */
	v_up,		V_CNT|V_LMODE|V_MOVE,
	    "move up by lines: [count]k",
/* 154   l */
	v_right,	V_CNT|V_MOVE,
	    "move right by columns: [count]l",
/* 155   m */
	v_mark,		V_CHAR,
	    "set mark: m[a-z]",
/* 156   n */
	v_searchn,	V_MOVE,
	    "repeat last search: n",
/* 157   o */
	v_io,		V_CNT|V_DOT,
	    "append after line: [count]o",
/* 160   p */
	v_put,		V_CNT|V_DOT|V_OBUF,
	    "insert after cursor from buffer: [buffer]P",
/* 161   q */
	{},
/* 162   r */
	v_replace,	V_CHAR|V_CNT,
	    "replace character: [count]r character",
/* 163   s */
	v_subst,	V_CNT|V_DOT|V_OBUF,
	    "substitute character: [buffer][count]s",
/* 164   t */
	v_cht,		V_CHAR|V_CNT|V_MOVE,
	    "before character in line forward search: [count]t character",
/* 165   u */
	v_undo,		0,
	    "undo: u",
/* 166   v */
	v_start,	0,
	    "start visible: v",
/* 167   w */
	v_wordw,	V_CNT|V_MOVE,
	    "move to next word: [count]w",
/* 170   x */
	v_xchar,	V_CNT|V_DOT|V_OBUF,
	    "delete character: [buffer][count]x",
/* 171   y */
	v_yank,		V_CNT|V_MOTION|V_OBUF,
	    "copy text: [buffer][count]y[count]motion",
/* 172   z */
	m_z, 		V_CNT,
	    "redraw window: [count1]z[count2]character",
/* 173   { */
	v_paragraphb,	V_CNT|V_MOVE,
	    "move back paragraph: [count]{",
/* 174   | */
	v_ncol,		V_ABS|V_CNT|V_MOVE,
	    "move to column: [count]|",
/* 175   } */
	v_paragraphf,	V_CNT|V_MOVE,
	    "move forward paragraph: [count]}",
/* 176   ~ */
	v_ulcase,	V_CNT|V_DOT,
	    "reverse case: [count]~",
};
