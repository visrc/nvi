/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_itxt.c,v 10.3 1995/07/04 12:45:59 bostic Exp $ (Berkeley) $Date: 1995/07/04 12:45:59 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "vi.h"

/*
 * !!!
 * Repeated input in the historic vi is mostly wrong and this isn't very
 * backward compatible.  For example, if the user entered "3Aab\ncd" in
 * the historic vi, the "ab" was repeated 3 times, and the "\ncd" was then
 * appended to the result.  There was also a hack which I don't remember
 * right now, where "3o" would open 3 lines and then let the user fill them
 * in, to make screen movements on 300 baud modems more tolerable.  I don't
 * think it's going to be missed.
 *
 * !!!
 * There's a problem with the way that we do logging for change commands with
 * implied motions (e.g. A, I, O, cc, etc.).  Since the main vi loop logs the
 * starting cursor position before the change command "moves" the cursor, the
 * cursor position to which we return on undo will be where the user entered
 * the change command, not the start of the change.  Several of the following
 * routines re-log the cursor to make this work correctly.  Historic vi tried
 * to do the same thing, and mostly got it right.  (The only spectacular way
 * it fails is if the user entered 'o' from anywhere but the last character of
 * the line, the undo returned the cursor to the start of the line.  If the
 * user was on the last character of the line, the cursor returned to that
 * position.)  We also check for mapped keys waiting, i.e. if we're in the
 * middle of a map, don't bother logging the cursor.
 */
#define	LOG_CORRECT {							\
	if (!MAPPED_KEYS_WAITING(sp))					\
		(void)log_cursor(sp);					\
}

static void set_txt_std __P((SCR *, VICMD *, u_int));

/*
 * v_iA -- [count]A
 *	Append text to the end of the line.
 *
 * PUBLIC: int v_iA __P((SCR *, VICMD *));
 */
int
v_iA(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	size_t len;

	if (file_gline(sp, vp->m_start.lno, &len) != NULL)
		sp->cno = len == 0 ? 0 : len - 1;

	LOG_CORRECT;

	return (v_ia(sp, vp));
}

/*
 * v_ia -- [count]a
 *	   [count]A
 *	Append text to the cursor position.
 *
 * PUBLIC: int v_ia __P((SCR *, VICMD *));
 */
int
v_ia(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	VI_PRIVATE *vip;
	recno_t lno;
	size_t len;
	char *p;

	set_txt_std(sp, vp, 0);
	sp->showmode = SM_APPEND;
	sp->lno = vp->m_start.lno;

	/* Move the cursor one column to the right and repaint the screen. */
	vip = VIP(sp);
	if ((p = file_gline(sp, sp->lno, &len)) == NULL) {
		if (file_lline(sp, &lno))
			return (1);
		if (lno != 0) {
			FILE_LERR(sp, lno);
			return (1);
		}
		len = 0;
		FL_SET(vip->im_flags, TXT_APPENDEOL);
	} else if (len) {
		if (len == sp->cno + 1) {
			sp->cno = len;
			FL_SET(vip->im_flags, TXT_APPENDEOL);
		} else
			++sp->cno;
	} else
		FL_SET(vip->im_flags, TXT_APPENDEOL);

	return (v_txt_setup(sp, vp, NULL, p, len, 0, OOBLNO));
}

/*
 * v_iI -- [count]I
 *	Insert text at the first nonblank.
 *
 * PUBLIC: int v_iI __P((SCR *, VICMD *));
 */
int
v_iI(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	sp->cno = 0;
	if (nonblank(sp, vp->m_start.lno, &sp->cno))
		return (1);

	LOG_CORRECT;

	return (v_ii(sp, vp));
}

/*
 * v_ii -- [count]i
 *	   [count]I
 *	Insert text at the cursor position.
 *
 * PUBLIC: int v_ii __P((SCR *, VICMD *));
 */
int
v_ii(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	recno_t lno;
	size_t len;
	char *p;

	set_txt_std(sp, vp, 0);
	sp->showmode = SM_INSERT;
	sp->lno = vp->m_start.lno;

	if ((p = file_gline(sp, sp->lno, &len)) == NULL) {
		if (file_lline(sp, &lno))
			return (1);
		if (lno != 0) {
			FILE_LERR(sp, vp->m_start.lno);
			return (1);
		}
		len = 0;
	}

	if (len == 0)
		FL_SET(VIP(sp)->im_flags, TXT_APPENDEOL);
	return (v_txt_setup(sp, vp, NULL, p, len, 0, OOBLNO));
}

enum which { o_cmd, O_cmd };
static int io __P((SCR *, VICMD *, enum which));

/*
 * v_iO -- [count]O
 *	Insert text above this line.
 *
 * PUBLIC: int v_iO __P((SCR *, VICMD *));
 */
int
v_iO(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	return (io(sp, vp, O_cmd));
}

/*
 * v_io -- [count]o
 *	Insert text after this line.
 *
 * PUBLIC: int v_io __P((SCR *, VICMD *));
 */
int
v_io(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	return (io(sp, vp, o_cmd));
}

static int
io(sp, vp, cmd)
	SCR *sp;
	VICMD *vp;
	enum which cmd;
{
	recno_t ai_line, lno;
	size_t len;
	char *p;

	set_txt_std(sp, vp, TXT_ADDNEWLINE | TXT_APPENDEOL);
	sp->showmode = SM_INSERT;

	if (sp->lno == 1) {
		if (file_lline(sp, &lno))
			return (1);
		if (lno != 0)
			goto insert;
		p = NULL;
		len = 0;
		ai_line = OOBLNO;
	} else {
insert:		p = "";
		sp->cno = 0;
		LOG_CORRECT;

		if (cmd == O_cmd) {
			if (file_iline(sp, sp->lno, p, 0))
				return (1);
			if ((p = file_gline(sp, sp->lno, &len)) == NULL) {
				FILE_LERR(sp, sp->lno);
				return (1);
			}
			ai_line = sp->lno + 1;
		} else {
			if (file_aline(sp, 1, sp->lno, p, 0))
				return (1);
			if ((p = file_gline(sp, ++sp->lno, &len)) == NULL) {
				FILE_LERR(sp, sp->lno);
				return (1);
			}
			ai_line = sp->lno - 1;
		}
	}
	return (v_txt_setup(sp, vp, NULL, p, len, 0, ai_line));
}

/*
 * v_change -- [buffer][count]c[count]motion
 *	       [buffer][count]C
 *	       [buffer][count]S
 *	Change command.
 *
 * PUBLIC: int v_change __P((SCR *, VICMD *));
 */
int
v_change(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	VI_PRIVATE *vip;
	recno_t lno;
	size_t blen, len;
	int lmode, rval;
	char *bp, *p;

	vip = VIP(sp);

	/*
	 * Find out if the file is empty, it's easier to handle it as a
	 * special case.
	 */
	if (vp->m_start.lno == vp->m_stop.lno &&
	    (p = file_gline(sp, vp->m_start.lno, &len)) == NULL) {
		if (file_lline(sp, &lno))
			return (1);
		if (lno != 0) {
			FILE_LERR(sp, vp->m_start.lno);
			return (1);
		}
		return (v_ia(sp, vp));
	}

	sp->showmode = SM_CHANGE;
	set_txt_std(sp, vp, 0);

	/*
	 * Move the cursor to the start of the change.  Note, if autoindent
	 * is turned on, the cc command in line mode changes from the first
	 * *non-blank* character of the line, not the first character.  And,
	 * to make it just a bit more exciting, the initial space is handled
	 * as auto-indent characters.
	 */
	lmode = F_ISSET(vp, VM_LMODE) ? CUT_LINEMODE : 0;
	if (lmode) {
		vp->m_start.cno = 0;
		if (O_ISSET(sp, O_AUTOINDENT)) {
			if (nonblank(sp, vp->m_start.lno, &vp->m_start.cno))
				return (1);
			FL_SET(vip->im_flags, TXT_AICHARS);
		}
	}
	sp->lno = vp->m_start.lno;
	sp->cno = vp->m_start.cno;

	LOG_CORRECT;

	/*
	 * 'c' can be combined with motion commands that set the resulting
	 * cursor position, i.e. "cG".  Clear the VM_RCM flags and make the
	 * resulting cursor position stick, inserting text has its own rules
	 * for cursor positioning.
	 */
	F_CLR(vp, VM_RCM_MASK);
	F_SET(vp, VM_RCM_SET);

	/*
	 * If not in line mode and changing within a single line, copy the
	 * text and overwrite it.
	 */
	if (!lmode && vp->m_start.lno == vp->m_stop.lno) {
		/*
		 * !!!
		 * Historic practice, c did not cut into the numeric buffers,
		 * only the unnamed one.
		 */
		if (cut(sp,
		    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
		    &vp->m_start, &vp->m_stop, lmode))
			return (1);
		if (len == 0)
			FL_SET(vip->im_flags, TXT_APPENDEOL);
		FL_SET(vip->im_flags, TXT_EMARK | TXT_OVERWRITE);
		return (v_txt_setup(sp, vp, &vp->m_stop, p, len, 0, OOBLNO));
	}

	/*
	 * It's trickier if in line mode or changing over multiple lines.  If
	 * we're in line mode delete all of the lines and insert a replacement
	 * line which the user edits.  If there was leading whitespace in the
	 * first line being changed, we copy it and use it as the replacement.
	 * If we're not in line mode, we delete the text and start inserting.
	 *
	 * !!!
	 * Copy the text.  Historic practice, c did not cut into the numeric
	 * buffers, only the unnamed one.
	 */
	if (cut(sp,
	    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    &vp->m_start, &vp->m_stop, lmode))
		return (1);

	/* If replacing entire lines and there's leading text. */
	if (lmode && vp->m_start.cno) {
		/* Get a copy of the first line changed. */
		if ((p = file_gline(sp, vp->m_start.lno, &len)) == NULL) {
			FILE_LERR(sp, vp->m_start.lno);
			return (1);
		}
		/* Copy the leading text elsewhere. */
		GET_SPACE_RET(sp, bp, blen, vp->m_start.cno);
		memmove(bp, p, vp->m_start.cno);
	} else
		bp = NULL;

	/* Delete the text. */
	if (delete(sp, &vp->m_start, &vp->m_stop, lmode))
		return (1);

	/* If replacing entire lines, insert a replacement line. */
	if (lmode) {
		if (file_iline(sp, vp->m_start.lno, bp, vp->m_start.cno))
			return (1);
		sp->lno = vp->m_start.lno;
		len = sp->cno = vp->m_start.cno;
	}

	/* Get the line we're editing. */
	if ((p = file_gline(sp, vp->m_start.lno, &len)) == NULL) {
		if (file_lline(sp, &lno))
			return (1);
		if (lno != 0) {
			FILE_LERR(sp, vp->m_start.lno);
			return (1);
		}
		len = 0;
	}

	/* Check to see if we're appending to the line. */
	if (vp->m_start.cno >= len)
		FL_SET(vip->im_flags, TXT_APPENDEOL);

	rval = v_txt_setup(sp, vp, NULL, p, len, 0, OOBLNO);

	if (bp != NULL)
		FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * v_Replace -- [count]R
 *	Overwrite multiple characters.
 *
 * !!!
 * Special case.  The historic vi handled [count]R badly, in that R would
 * replace some number of characters, and then the count appended count-1
 * copies of the replacing chars to the replaced space.  This seems wrong,
 * so this version counts R commands.
 *
 * PUBLIC: int v_Replace __P((SCR *, VICMD *));
 */
int
v_Replace(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	VI_PRIVATE *vip;
	recno_t lno;
	size_t len;
	char *p;

	vip = VIP(sp);
	set_txt_std(sp, vp, 0);
	sp->showmode = SM_REPLACE;

	if ((p = file_gline(sp, vp->m_start.lno, &len)) == NULL) {
		if (file_lline(sp, &lno))
			return (1);
		if (lno != 0) {
			FILE_LERR(sp, vp->m_start.lno);
			return (1);
		}
		len = 0;
		FL_SET(vip->im_flags, TXT_APPENDEOL);
	} else {
		if (len == 0)
			FL_SET(vip->im_flags, TXT_APPENDEOL);
		FL_SET(vip->im_flags, TXT_OVERWRITE | TXT_REPLACE);
	}
	vp->m_stop.lno = vp->m_start.lno;
	vp->m_stop.cno = len ? len - 1 : 0;

	return (v_txt_setup(sp, vp, &vp->m_stop, p, len, 0, OOBLNO));
}

/*
 * v_subst -- [buffer][count]s
 *	Substitute characters.
 *
 * PUBLIC: int v_subst __P((SCR *, VICMD *));
 */
int
v_subst(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	VI_PRIVATE *vip;
	recno_t lno;
	size_t len;
	char *p;

	vip = VIP(sp);
	sp->showmode = SM_CHANGE;
	set_txt_std(sp, vp, 0);
	if ((p = file_gline(sp, vp->m_start.lno, &len)) == NULL) {
		if (file_lline(sp, &lno))
			return (1);
		if (lno != 0) {
			FILE_LERR(sp, vp->m_start.lno);
			return (1);
		}
		len = 0;
		FL_SET(vip->im_flags, TXT_APPENDEOL);
	} else {
		if (len == 0)
			FL_SET(vip->im_flags, TXT_APPENDEOL);
		FL_SET(vip->im_flags, TXT_EMARK | TXT_OVERWRITE);
	}

	vp->m_stop.lno = vp->m_start.lno;
	vp->m_stop.cno =
	    vp->m_start.cno + (F_ISSET(vp, VC_C1SET) ? vp->count - 1 : 0);
	if (vp->m_stop.cno > len - 1)
		vp->m_stop.cno = len - 1;

	if (p != NULL && cut(sp,
	    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    &vp->m_start, &vp->m_stop, 0))
		return (1);

	return (v_txt_setup(sp, vp, &vp->m_stop, p, len, 0, OOBLNO));
}

/*
 * set_txt_std --
 *	Initialize text processing flags.
 */
static void
set_txt_std(sp, vp, init)
	SCR *sp;
	VICMD *vp;
	u_int init;
{
	VI_PRIVATE *vip;

	vip = VIP(sp);
	FL_INIT(vip->im_flags,
	    init | TXT_CNTRLT | TXT_ESCAPE | TXT_RECORD | TXT_RESOLVE);

	if (O_ISSET(sp, O_ALTWERASE))
		FL_SET(vip->im_flags, TXT_ALTWERASE);
	if (O_ISSET(sp, O_AUTOINDENT))
		FL_SET(vip->im_flags, TXT_AUTOINDENT);
	if (O_ISSET(sp, O_BEAUTIFY))
		FL_SET(vip->im_flags, TXT_BEAUTIFY);
	if (O_ISSET(sp, O_SHOWMATCH))
		FL_SET(vip->im_flags, TXT_SHOWMATCH);
	if (F_ISSET(sp, S_SCRIPT))
		FL_SET(vip->im_flags, TXT_CR);
	if (O_ISSET(sp, O_TTYWERASE))
		FL_SET(vip->im_flags, TXT_TTYWERASE);
	if (F_ISSET(vp,  VC_ISDOT))
		FL_SET(vip->im_flags, TXT_REPLAY);

	/*
	 * !!!
	 * Mapped keys were sometimes unaffected by the wrapmargin option
	 * in the historic 4BSD vi.  Consider the following commands, where
	 * each is executed on an empty line, in an 80 column screen, with
	 * the wrapmargin value set to 60.
	 *
	 *	aABC DEF <ESC>....
	 *	:map K aABC DEF ^V<ESC><CR>KKKKK
	 *	:map K 5aABC DEF ^V<ESC><CR>K
	 * 
	 * The first and second commands are affected by wrapmargin.  The
	 * third is not.  (If the inserted text is itself longer than the
	 * wrapmargin value, i.e. if the "ABC DEF " string is replaced by
	 * something that's longer than 60 columns from the beginning of
	 * the line, the first two commands behave as before, but the third
	 * command gets fairly strange.)  The problem is that people wrote
	 * macros that depended on the third command NOT being affected by
	 * wrapmargin, as in this gem which centers lines:
	 *
	 *	map #c $mq81a ^V^[81^V^V|D`qld0:s/  / /g^V^M$p
	 *
	 * For compatibility reasons, we try and make it all work here.  I
	 * offer no hope that this is right, but it's probably pretty close.
	 */
	if ((O_ISSET(sp, O_WRAPLEN) || O_ISSET(sp, O_WRAPMARGIN)) &&
	    (!MAPPED_KEYS_WAITING(sp) || !F_ISSET(vp, VC_C1SET)))
		FL_SET(vip->im_flags, TXT_WRAPMARGIN);
}

/*
 * v_tcmd_setup --
 *	Fill a buffer from the terminal for vi.
 *
 * PUBLIC: int v_tcmd_setup __P((SCR *, VICMD *, ARG_CHAR_T, u_int));
 */
int
v_tcmd_setup(sp, vp, prompt, flags)
	SCR *sp;
	VICMD *vp;
	ARG_CHAR_T prompt;
	u_int flags;
{
	SMAP *esmp;
	VI_PRIVATE *vip;

	vip = VIP(sp);

	/* Save current cursor. */
	vip->gsv_lno = sp->lno;
	vip->gsv_cno = sp->cno;

	if (!IS_ONELINE(sp)) {
		/*
		 * Fake like the user is doing input on the last line of the
		 * screen.  This makes all of the scrolling work correctly,
		 * and allows us the use of the vi text editing routines, not
		 * to mention practically infinite length ex commands.
		 *
		 * Save the current location.
		 */
		vip->gsv_tm_lno = TMAP->lno;
		vip->gsv_tm_off = TMAP->off;
		vip->gsv_t_rows = sp->t_rows;
		vip->gsv_t_minrows = sp->t_minrows;
		vip->gsv_t_maxrows = sp->t_maxrows;

		/*
		 * If it's a small screen, TMAP may be small for the screen.
		 * Fix it, filling in fake lines as we go.
		 */
		if (IS_SMALL(sp))
			for (esmp =
			    HMAP + (sp->t_maxrows - 1); TMAP < esmp; ++TMAP) {
				TMAP[1].lno = TMAP[0].lno + 1;
				TMAP[1].off = 1;
			}

		/* Build the fake entry. */
		TMAP[1].lno = TMAP[0].lno + 1;
		TMAP[1].off = 1;
		SMAP_FLUSH(&TMAP[1]);
		++TMAP;

		/* Reset the screen information. */
		sp->t_rows = sp->t_minrows = ++sp->t_maxrows;
	}

	/* Move to the last line. */
	sp->lno = TMAP[0].lno;
	sp->cno = 0;

	/* Don't update the modeline for now. */
	F_SET(sp, S_INPUT_INFO);

	FL_INIT(vip->im_flags,
	    flags | TXT_APPENDEOL | TXT_CR | TXT_ESCAPE | TXT_INFOLINE);
	if (O_ISSET(sp, O_ALTWERASE))
		FL_SET(vip->im_flags, TXT_ALTWERASE);
	if (O_ISSET(sp, O_TTYWERASE))
		FL_SET(vip->im_flags, TXT_TTYWERASE);

	return (v_txt_setup(sp, vp, NULL, NULL, 0, prompt, 0));
}

/*
 * v_tcmd_td --
 *	Tear down the v_tcmd_setup routine.
 *
 * PUBLIC: int v_tcmd_td __P((SCR *, VICMD *));
 */
int
v_tcmd_td(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	VI_PRIVATE *vip;
	size_t cnt;

	F_CLR(sp, S_INPUT_INFO);

	vip = VIP(sp);
	if (!IS_ONELINE(sp)) {
		/* Restore the screen information. */
		sp->t_rows = vip->gsv_t_rows;
		sp->t_minrows = vip->gsv_t_minrows;
		sp->t_maxrows = vip->gsv_t_maxrows;

		/*
		 * If it's a small screen, TMAP may be wrong.  Clear any
		 * lines that might have been overwritten.
		 */
		if (IS_SMALL(sp)) {
			for (cnt = sp->t_rows; cnt <= sp->t_maxrows; ++cnt) {
				(void)sp->gp->scr_move(sp, cnt, 0);
				(void)sp->gp->scr_clrtoeol(sp);
			}
			TMAP = HMAP + (sp->t_rows - 1);
		} else
			--TMAP;

		/*
		 * The map may be wrong if the user entered more than one
		 * (logical) line.  Fix it.  If the user entered a whole
		 * screen, this will be slow, but we probably don't care.
		 */
		while (vip->gsv_tm_lno != TMAP->lno ||
		    vip->gsv_tm_off != TMAP->off)
			if (vs_sm_1down(sp))
				return (1);
	} else
		F_SET(sp, S_SCR_REDRAW);

	/*
	 * Invalidate the cursor and the line size cache, the line never
	 * really existed.  This fixes bugs where the user searches for
	 * the last line on the screen + 1 and the refresh routine thinks
	 * that's where we just were.
	 */
	VI_SCR_CFLUSH(vip);
	F_SET(vip, VIP_CUR_INVALID);

	/* Restore the original cursor. */
	sp->lno = vip->gsv_lno;
	sp->cno = vip->gsv_cno;
	return (0);
}
