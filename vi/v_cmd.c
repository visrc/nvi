/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_cmd.c,v 5.15 1992/04/19 10:54:11 bostic Exp $ (Berkeley) $Date: 1992/04/19 10:54:11 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"

/*
 * This array maps keystrokes to vi command functions.
 */
VIKEYS vikeys[] = {
/* 0000 NUL */
	{},
/* 0001  ^A find cursor word */
	{m_wsrch,	KEYWORD,	MVMT|NREL|VIZ},
/* 0002  ^B page backward 1 page */
	{m_scroll,	CURSOR,		FRNT|VIZ},
/* 0003  ^C */
	{},
/* 0004  ^D scroll down 1/2 page */
	{m_scroll,	CURSOR,		NCOL|VIZ},
/* 0005  ^E scroll up 1 line */
	{m_scroll,	CURSOR,		NCOL|VIZ},
/* 0006  ^F page forward 1 page */
	{m_scroll,	CURSOR,		FRNT|VIZ},
/* 0007  ^G show file status */
	{v_status,	0, 	0},
/* 0010  ^H move left 1 column */
	{m_left,	CURSOR,		MVMT|VIZ},
/* 0011  ^I */
	{},
/* 0012  ^J move down 1 line */
	{m_updnto,	CURSOR,		MVMT|LNMD|VIZ},
/* 0013  ^K */
	{},
/* 0014  ^L redraw screen */
	{v_redraw,	0,	0|VIZ},
/* 0015  ^M move to column 1 of next line */
	{m_updnto,	CURSOR,		MVMT|FRNT|LNMD|VIZ},
/* 0016  ^N move down 1 line */
	{m_updnto,	CURSOR,		MVMT|LNMD|VIZ},
/* 0017  ^O */
	{},
/* 0020  ^P move up 1 line */
	{m_updnto,	CURSOR,		MVMT|LNMD|VIZ},
/* 0021  ^Q */
	{},
/* 0022  ^R redraw screen */
	{v_redraw,	0,	0|VIZ},
/* 0023  ^S */
	{},
/* 0024  ^T */
	{},
/* 0025  ^U scroll up 1/2 page */
	{m_scroll,	CURSOR,		NCOL|VIZ},
/* 0026  ^V */
	{},
/* 0027  ^W */
	{},
/* 0030  ^X move to physical column */
	{m_tocol,	CURSOR,		NREL|VIZ},
/* 0031  ^Y */
	{},
/* 0032  ^Z */
	{},
/* 0033  ^[ */
	{},
/* 0034  ^\ */
	{},
/* 0035  ^] tag cursor word */
	{v_tag,		KEYWORD,	0},
/* 0036  ^^ previous file */
	{v_switch,	CURSOR,		0},
/* 0037  ^_ */
	{},
/* 0040 ' ' move right 1 column */
	{m_right,	CURSOR,		MVMT|VIZ},
/* 0041   ! run through filter */
	{v_filter,	CURSOR_MOVED,	FRNT|LNMD|INCL|VIZ},
/* 0042   " select cut buffer */
	{v_selcut,	C_C_K_CUT,	PTMV|VIZ},
/* 0043   # increment number */
	{v_increment,	KEYWORD,	SDOT},
/* 0044   $ move to last column */
	{m_rear,	CURSOR,		MVMT|INCL|VIZ},
/* 0045   % move to match */
	{m_match,	CURSOR,		MVMT|INCL|VIZ},
/* 0046   & repeat substitution */
	{v_again,	CURSOR_MOVED,	SDOT|NCOL|LNMD|INCL},
/* 0047   ' move to mark */
	{m_tomark,	C_C_K_MARK,	MVMT|FRNT|NREL|LNMD|INCL|VIZ},
/* 0050   ( move back sentence */
	{m_bsentence,	CURSOR,		MVMT|VIZ},
/* 0051   ) move forward sentence */
	{m_fsentence,	CURSOR,		MVMT|VIZ},
/* 0052   * errlist  */
	{v_errlist,	CURSOR,		FRNT|NREL},
/* 0053   + move to column 1 of next line */
	{m_updnto,	CURSOR,		MVMT|FRNT|LNMD|VIZ},
/* 0054   , reverse [fFtT] cmd */
	{m__ch,		CURSOR,		MVMT|INCL|VIZ},
/* 0055   - move to column 1 of previous line */
	{m_updnto,	CURSOR,		MVMT|FRNT|LNMD|VIZ},
/* 0056   . special. */
	{},
/* 0057   / forward search */
	{m_fsrch,	CURSOR_TEXT,	MVMT|NREL|VIZ},
/* 0060   0 part of count? */
	{NULL,	ZERO,		MVMT|PTMV|VIZ},
/* 0061   1 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 0062   2 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 0063   3 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 0064   4 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 0065   5 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 0066   6 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 0067   7 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 0070   8 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 0071   9 part of count */
	{NULL,	DIGIT,		PTMV|VIZ},
/* 0072   : run single ex command */
	{v_ex,		CURSOR_TEXT,	0},
/* 0073   ; repeat [fFtT] command */
	{m__ch,		CURSOR,		MVMT|INCL|VIZ},
/* 0074   < shift text left */
	{v_lshift,	CURSOR_MOVED,	SDOT|FRNT|LNMD|INCL|VIZ},
/* 0075   = */
	{},
/* 0076   > shift text right */
	{v_rshift,	CURSOR_MOVED,	SDOT|FRNT|LNMD|INCL|VIZ},
/* 0077   ? backward search */
	{m_bsrch,	CURSOR_TEXT,	MVMT|NREL|VIZ},
/* 0100   @ execute a cut buffer */
	{v_at,		C_C_K_CUT,	0},
/* 0101   A append at EOL */
	{v_insert,	CURSOR,		SDOT},
/* 0102   B move back word */
	{m_bword,	CURSOR,		MVMT|VIZ},
/* 0103   C change to EOL */
	{v_change,	CURSOR_EOL,	SDOT},
/* 0104   D delete to EOL */
	{v_delete,	CURSOR_EOL,	SDOT},
/* 0105   E move to end of word */
	{m_eword,	CURSOR,		MVMT|INCL|VIZ},
/* 0106   F move back to char */
	{m_Fch,		C_C_K_CHAR,	MVMT|INCL|VIZ},
/* 0107   G move to line # */
	{m_updnto,	CURSOR,		MVMT|NREL|LNMD|FRNT|INCL|VIZ},
/* 0110   H move to row */
	{m_row,		CURSOR,		MVMT|FRNT|VIZ},
/* 0111   I insert at front */
	{v_insert,	CURSOR,		SDOT},
/* 0112   J join lines */
	{v_join,	CURSOR,		SDOT},
/* 0113   K */
	{},
/* 0114   L move to last row */
	{m_row,		CURSOR,		MVMT|FRNT|VIZ},
/* 0115   M move to middle row */
	{m_row,		CURSOR,		MVMT|FRNT|VIZ},
/* 0116   N reverse previous search */
	{m_Nsrch,	CURSOR,		MVMT|NREL|VIZ},
/* 0117   O insert above line */
	{v_insert,	CURSOR,		SDOT},
/* 0120   P paste before */
	{v_paste,	CURSOR,		SDOT},
/* 0121   Q quit to ex mode */
	{v_quit,	0,	0},
/* 0122   R overtype	 */
	{v_overtype,	CURSOR,		SDOT},
/* 0123   S change line */
	{v_change,	CURSOR_MOVED,	SDOT},
/* 0124   T move back to char */
	{m_Tch,		C_C_K_CHAR,	MVMT|INCL|VIZ},
/* 0125   U undo line */
	{v_undoline,	CURSOR,		FRNT},
/* 0126   V start visible */
	{v_start,	CURSOR,		INCL|LNMD|VIZ},
/* 0127   W move forward word */
	{m_fword,	CURSOR,		MVMT|INCL|VIZ},
/* 0130   X delete to left */
	{v_xchar,	CURSOR,		SDOT},
/* 0131   Y yank text */
	{v_yank,	CURSOR_MOVED,	NCOL},
/* 0132   Z save file and exit */
	{v_xit,		CURSOR_CNT_KEY,	0},
/* 0133   [ move back section */
	{m_paragraph,	CURSOR,		MVMT|LNMD|NREL|VIZ},
/* 0134   \ */
	{},
/* 0135   ] move forward section */
	{m_paragraph,	CURSOR,		MVMT|LNMD|NREL|VIZ},
/* 0136   ^ move to front */
	{m_front,	CURSOR,		MVMT|VIZ},
/* 0137   _ current line */
	{m_updnto,	CURSOR,		MVMT|LNMD|FRNT|INCL},
/* 0140   ` move to mark */
	{m_tomark,	C_C_K_MARK,	MVMT|NREL|VIZ},
/* 0141   a append at cursor */
	{v_insert,	CURSOR,		SDOT},
/* 0142   b move back word */
	{m_bword,	CURSOR,		MVMT|VIZ},
/* 0143   c change text */
	{v_change,	CURSOR_MOVED,	SDOT|VIZ},
/* 0144   d delete operation */
	{v_delete,	CURSOR_MOVED,	SDOT|VIZ},
/* 0145   e move to word end */
	{m_eword,	CURSOR,		MVMT|INCL|VIZ},
/* 0146   f move forward to char */
	{m_fch,		C_C_K_CHAR,	MVMT|INCL|VIZ},
/* 0147   g */
	{},
/* 0150   h move left 1 column */
	{m_left,	CURSOR,		MVMT|VIZ},
/* 0151   i insert at cursor */
	{v_insert,	CURSOR,		SDOT},
/* 0152   j move down 1 line */
	{m_updnto,	CURSOR,		MVMT|NCOL|LNMD|VIZ},
/* 0153   k move up 1 line */
	{m_updnto,	CURSOR,		MVMT|NCOL|LNMD|VIZ},
/* 0154   l move right 1 column */
	{m_right,	CURSOR,		MVMT|VIZ},
/* 0155   m define a mark */
	{v_mark,	C_C_K_MARK,	0},
/* 0156   n repeat previous search */
	{m_nsrch,	CURSOR, 	MVMT|NREL|VIZ},
/* 0157   o insert below line */
	{v_insert,	CURSOR,		SDOT},
/* 0160   p paste after */
	{v_paste,	CURSOR,		SDOT},
/* 0161   q */
	{},
/* 0162   r replace chars */
	{v_replace,	C_C_K_REP1,	SDOT},
/* 0163   s substitute N chars */
	{v_subst,	CURSOR,		SDOT},
/* 0164   t move forward to char */
	{m_tch,		C_C_K_CHAR,	MVMT|INCL|VIZ},
/* 0165   u undo	 */
	{v_undo,	CURSOR,		0},
/* 0166   v start visible */
	{v_start,	CURSOR,		INCL|VIZ},
/* 0167   w move forward word */
	{m_fword,	CURSOR,		MVMT|INCL|VIZ},
/* 0170   x delete character */
	{v_xchar,	CURSOR,		SDOT},
/* 0171   y yank text */
	{v_yank,	CURSOR_MOVED,	NCOL|VIZ},
/* 0172   z adjust screen row */
	{m_z, 		CURSOR_CNT_KEY,	NCOL|VIZ},
/* 0173   { back paragraph */
	{m_paragraph,	CURSOR,		MVMT|LNMD|VIZ},
/* 0174   | move to column */
	{m_tocol,	CURSOR,		MVMT|NREL|VIZ},
/* 0175   } forward paragraph */
	{m_paragraph,	CURSOR,		MVMT|LNMD|VIZ},
/* 0176   ~ upper/lowercase */
	{v_ulcase,	CURSOR,		SDOT},
/* 0177  ^? */
	{},
};
