/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_cmd.c,v 5.16 1992/04/22 08:09:14 bostic Exp $ (Berkeley) $Date: 1992/04/22 08:09:14 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"

/*
 * This array maps keystrokes to vi command functions.
 */
VIKEYS vikeys[] = {
/* 000 NUL */
	{},
/* 001  ^A find cursor word */
	{m_wsrch,	KEYWORD,	MVMT|NREL|VIZ},
/* 002  ^B page backward 1 page */
	{m_scroll,	CURSOR,		FRNT|VIZ},
/* 003  ^C */
	{},
/* 004  ^D scroll down 1/2 page */
	{m_scroll,	CURSOR,		NCOL|VIZ},
/* 005  ^E scroll up 1 line */
	{m_scroll,	CURSOR,		NCOL|VIZ},
/* 006  ^F page forward 1 page */
	{m_scroll,	CURSOR,		FRNT|VIZ},
/* 007  ^G show file status */
	{v_status,	0, 	0},
/* 010  ^H move left 1 column */
	{m_left,	CURSOR,		MVMT|VIZ},
/* 011  ^I */
	{},
/* 012  ^J move down 1 line */
	{m_updnto,	CURSOR,		MVMT|LNMD|VIZ},
/* 013  ^K */
	{},
/* 014  ^L redraw screen */
	{v_redraw,	0,	0|VIZ},
/* 015  ^M move to column 1 of next line */
	{m_updnto,	CURSOR,		MVMT|FRNT|LNMD|VIZ},
/* 016  ^N move down 1 line */
	{m_updnto,	CURSOR,		MVMT|LNMD|VIZ},
/* 017  ^O */
	{},
/* 020  ^P move up 1 line */
	{m_updnto,	CURSOR,		MVMT|LNMD|VIZ},
/* 021  ^Q */
	{},
/* 022  ^R redraw screen */
	{v_redraw,	0,	0|VIZ},
/* 023  ^S */
	{},
/* 024  ^T */
	{},
/* 025  ^U scroll up 1/2 page */
	{m_scroll,	CURSOR,		NCOL|VIZ},
/* 026  ^V */
	{},
/* 027  ^W */
	{},
/* 030  ^X move to physical column */
	{m_tocol,	CURSOR,		NREL|VIZ},
/* 031  ^Y */
	{},
/* 032  ^Z */
	{},
/* 033  ^[ */
	{},
/* 034  ^\ */
	{},
/* 035  ^] tag cursor word */
	{v_tag,		KEYWORD,	0},
/* 036  ^^ previous file */
	{v_switch,	CURSOR,		0},
/* 037  ^_ */
	{},
/* 040 ' ' move right 1 column */
	{m_right,	CURSOR,		MVMT|VIZ},
/* 041   ! run through filter */
	{v_filter,	CURSOR_MOVED,	FRNT|LNMD|INCL|VIZ},
/* 042   " select cut buffer */
	{v_selcut,	C_C_K_CUT,	PTMV|VIZ},
/* 043   # increment number */
	{v_increment,	KEYWORD,	SDOT},
/* 044   $ move to last column */
	{m_rear,	CURSOR,		MVMT|INCL|VIZ},
/* 045   % move to match */
	{m_match,	CURSOR,		MVMT|INCL|VIZ},
/* 046   & repeat substitution */
	{v_again,	CURSOR_MOVED,	SDOT|NCOL|LNMD|INCL},
/* 047   ' move to mark */
	{m_tomark,	C_C_K_MARK,	MVMT|FRNT|NREL|LNMD|INCL|VIZ},
/* 050   ( move back sentence */
	{m_bsentence,	CURSOR,		MVMT|VIZ},
/* 051   ) move forward sentence */
	{m_fsentence,	CURSOR,		MVMT|VIZ},
/* 052   * errlist  */
	{v_errlist,	CURSOR,		FRNT|NREL},
/* 053   + move to column 1 of next line */
	{m_updnto,	CURSOR,		MVMT|FRNT|LNMD|VIZ},
/* 054   , reverse [fFtT] cmd */
	{m__ch,		CURSOR,		MVMT|INCL|VIZ},
/* 055   - move to column 1 of previous line */
	{m_updnto,	CURSOR,		MVMT|FRNT|LNMD|VIZ},
/* 056   . special. */
	{},
/* 057   / forward search */
	{m_fsrch,	CURSOR_TEXT,	MVMT|NREL|VIZ},
/* 060   0 part of count? */
	{NULL,	ZERO,		MVMT|PTMV|VIZ},
/* 061   1 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 062   2 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 063   3 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 064   4 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 065   5 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 066   6 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 067   7 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 070   8 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 071   9 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 072   : run single ex command */
	{v_ex,		CURSOR_TEXT,	0},
/* 073   ; repeat [fFtT] command */
	{m__ch,		CURSOR,		MVMT|INCL|VIZ},
/* 074   < shift text left */
	{v_shiftl,	CURSOR_MOVED,	SDOT|FRNT|LNMD|INCL|VIZ},
/* 075   = */
	{},
/* 076   > shift text right */
	{v_shiftr,	CURSOR_MOVED,	SDOT|FRNT|LNMD|INCL|VIZ},
/* 077   ? backward search */
	{m_bsrch,	CURSOR_TEXT,	MVMT|NREL|VIZ},
/* 100   @ execute a cut buffer */
	{v_at,		C_C_K_CUT,	0},
/* 101   A append at EOL */
	{v_insert,	CURSOR,		SDOT},
/* 102   B move back word */
	{m_bword,	CURSOR,		MVMT|VIZ},
/* 103   C change to EOL */
	{v_change,	CURSOR_EOL,	SDOT},
/* 104   D delete to EOL */
	{v_delete,	CURSOR_EOL,	SDOT},
/* 105   E move to end of word */
	{m_eword,	CURSOR,		MVMT|INCL|VIZ},
/* 106   F move back to char */
	{m_Fch,		C_C_K_CHAR,	MVMT|INCL|VIZ},
/* 107   G move to line # */
	{m_updnto,	CURSOR,		MVMT|NREL|LNMD|FRNT|INCL|VIZ},
/* 110   H move to row */
	{m_row,		CURSOR,		MVMT|FRNT|VIZ},
/* 111   I insert at front */
	{v_insert,	CURSOR,		SDOT},
/* 112   J join lines */
	{v_join,	CURSOR,		SDOT},
/* 113   K */
	{},
/* 114   L move to last row */
	{m_row,		CURSOR,		MVMT|FRNT|VIZ},
/* 115   M move to middle row */
	{m_row,		CURSOR,		MVMT|FRNT|VIZ},
/* 116   N reverse previous search */
	{m_Nsrch,	CURSOR,		MVMT|NREL|VIZ},
/* 117   O insert above line */
	{v_insert,	CURSOR,		SDOT},
/* 120   P paste before */
	{v_paste,	CURSOR,		SDOT},
/* 121   Q quit to ex mode */
	{v_quit,	0,	0},
/* 122   R overtype	 */
	{v_overtype,	CURSOR,		SDOT},
/* 123   S change line */
	{v_change,	CURSOR_MOVED,	SDOT},
/* 124   T move back to char */
	{m_Tch,		C_C_K_CHAR,	MVMT|INCL|VIZ},
/* 125   U undo line */
	{v_undoline,	CURSOR,		FRNT},
/* 126   V start visible */
	{v_start,	CURSOR,		INCL|LNMD|VIZ},
/* 127   W move forward word */
	{m_fword,	CURSOR,		MVMT|INCL|VIZ},
/* 130   X delete to left */
	{v_xchar,	CURSOR,		SDOT},
/* 131   Y yank text */
	{v_yank,	CURSOR_MOVED,	NCOL},
/* 132   Z save file and exit */
	{v_xit,		CURSOR_CNT_KEY,	0},
/* 133   [ move back section */
	{m_paragraph,	CURSOR,		MVMT|LNMD|NREL|VIZ},
/* 134   \ */
	{},
/* 135   ] move forward section */
	{m_paragraph,	CURSOR,		MVMT|LNMD|NREL|VIZ},
/* 136   ^ move to front */
	{m_front,	CURSOR,		MVMT|VIZ},
/* 137   _ current line */
	{m_updnto,	CURSOR,		MVMT|LNMD|FRNT|INCL},
/* 140   ` move to mark */
	{m_tomark,	C_C_K_MARK,	MVMT|NREL|VIZ},
/* 141   a append at cursor */
	{v_insert,	CURSOR,		SDOT},
/* 142   b move back word */
	{m_bword,	CURSOR,		MVMT|VIZ},
/* 143   c change text */
	{v_change,	CURSOR_MOVED,	SDOT|VIZ},
/* 144   d delete operation */
	{v_delete,	CURSOR_MOVED,	SDOT|VIZ},
/* 145   e move to word end */
	{m_eword,	CURSOR,		MVMT|INCL|VIZ},
/* 146   f move forward to char */
	{m_fch,		C_C_K_CHAR,	MVMT|INCL|VIZ},
/* 147   g */
	{},
/* 150   h move left 1 column */
	{m_left,	CURSOR,		MVMT|VIZ},
/* 151   i insert at cursor */
	{v_insert,	CURSOR,		SDOT},
/* 152   j move down 1 line */
	{m_updnto,	CURSOR,		MVMT|NCOL|LNMD|VIZ},
/* 153   k move up 1 line */
	{m_updnto,	CURSOR,		MVMT|NCOL|LNMD|VIZ},
/* 154   l move right 1 column */
	{m_right,	CURSOR,		MVMT|VIZ},
/* 155   m define a mark */
	{v_mark,	C_C_K_MARK,	0},
/* 156   n repeat previous search */
	{m_nsrch,	CURSOR, 	MVMT|NREL|VIZ},
/* 157   o insert below line */
	{v_insert,	CURSOR,		SDOT},
/* 160   p paste after */
	{v_paste,	CURSOR,		SDOT},
/* 161   q */
	{},
/* 162   r replace chars */
	{v_replace,	C_C_K_REP1,	SDOT},
/* 163   s substitute N chars */
	{v_subst,	CURSOR,		SDOT},
/* 164   t move forward to char */
	{m_tch,		C_C_K_CHAR,	MVMT|INCL|VIZ},
/* 165   u undo	 */
	{v_undo,	CURSOR,		0},
/* 166   v start visible */
	{v_start,	CURSOR,		INCL|VIZ},
/* 167   w move forward word */
	{m_fword,	CURSOR,		MVMT|INCL|VIZ},
/* 170   x delete character */
	{v_xchar,	CURSOR,		SDOT},
/* 171   y yank text */
	{v_yank,	CURSOR_MOVED,	NCOL|VIZ},
/* 172   z adjust screen row */
	{m_z, 		CURSOR_CNT_KEY,	NCOL|VIZ},
/* 173   { back paragraph */
	{m_paragraph,	CURSOR,		MVMT|LNMD|VIZ},
/* 174   | move to column */
	{m_tocol,	CURSOR,		MVMT|NREL|VIZ},
/* 175   } forward paragraph */
	{m_paragraph,	CURSOR,		MVMT|LNMD|VIZ},
/* 176   ~ upper/lowercase */
	{v_ulcase,	CURSOR,		SDOT},
/* 177  ^? */
	{},
};
