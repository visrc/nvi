/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_smap.c,v 5.16 1993/05/04 18:09:53 bostic Exp $ (Berkeley) $Date: 1993/05/04 18:09:53 $";
#endif /* not lint */

#include <curses.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"
#include "svi_screen.h"

/*
 * svi_change --
 *	Make a change to the screen.
 * XXX
 *	I'm not sure that the lines which `invalidate the cursor position'
 *	are right.  It seems like they could be changed to figure out what
 *	the new cursor position has to be.
 */
int
svi_change(sp, ep, lno, op)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	enum operation op;
{
	size_t oldy, oldx;

	/* Appending is the same as inserting, if the line is incremented. */
	if (op == LINE_APPEND)
		++lno;

	/* Ignore the change if the line is not on the screen. */
	if (lno < HMAP->lno || lno > TMAP->lno)
		return (0);

	getyx(stdscr, oldy, oldx);

	switch (op) {
	case LINE_DELETE:
		if (svi_sm_delete(sp, ep, lno))
			return (1);

		/* Invalidate the cursor. */
		F_SET(sp, S_CUR_INVALID);
		break;
	case LINE_APPEND:
	case LINE_INSERT:
		if (svi_sm_insert(sp, ep, lno))
			return (1);

		/* Invalidate the cursor. */
		F_SET(sp, S_CUR_INVALID);
		break;
	case LINE_RESET:
		if (svi_sm_reset(sp, ep, lno))
			return (1);
		break;
	default:
		abort();
	}

	MOVEA(sp, oldy, oldx);

	return (0);
}

/*
 * svi_sm_fill --
 *	Fill in the screen map, placing the specified line at the
 *	right position.
 */
int
svi_sm_fill(sp, ep, lno, pos)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	enum position pos;
{
	SMAP *p, tmp;
	
	switch (pos) {
	case P_FILL:
		tmp.lno = 1;
		tmp.off = 1;
		/* See if less than half a screen from the top. */
		if (svi_sm_nlines(sp, ep,
		    &tmp, lno, HALFSCREEN(sp)) <= HALFSCREEN(sp)) {
			lno = 1;
			goto ftop;
		}
		/* See if less than half a screen from the bottom. */
		tmp.lno = file_lline(sp, ep);
		tmp.off = svi_screens(sp, ep, tmp.lno, NULL);
		if (svi_sm_nlines(sp, ep,
		    &tmp, lno, HALFSCREEN(sp)) <= HALFSCREEN(sp)) {
			TMAP->lno = tmp.lno;
			TMAP->off = tmp.off;
			goto fbot;
		}
		goto fmid;
	case P_TOP:
ftop:		for (p = HMAP, p->lno = lno, p->off = 1; p < TMAP; ++p)
			if (svi_sm_next(sp, ep, p, p + 1))
				goto err;
		break;
	case P_MIDDLE:
fmid:		p = HMAP + (TMAP - HMAP) / 2;
		for (p->lno = lno, p->off = 1; p < TMAP; ++p)
			if (svi_sm_next(sp, ep, p, p + 1))
				goto err;
		p = HMAP + (TMAP - HMAP) / 2;
		for (; p > HMAP; --p)
			if (svi_sm_prev(sp, ep, p, p - 1))
				goto err;
		break;
	case P_BOTTOM:
		TMAP->lno = lno;
		TMAP->off = svi_screens(sp, ep, lno, NULL);
fbot:		for (p = TMAP; p > HMAP; --p)
			if (svi_sm_prev(sp, ep, p, p - 1))
				goto err;
		break;
	}
	return (0);

err:	msgq(sp, M_BERR, "Movement not possible");
	for (p = HMAP; p < TMAP; ++p)
		(void)svi_sm_next(sp, ep, p, p + 1);
	return (1);
}

/*
 * svi_sm_delete --
 *	Delete a line out of the SMAP.
 */
int
svi_sm_delete(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	SMAP *p, *t;
	size_t cnt1, cnt2;

	/* Find the line in the map. */
        for (p = HMAP; p->lno != lno; ++p);

	/*
	 * Count the number of screen lines which display any part
	 * of the deleted line.
	 */
	for (cnt1 = 1, t = p + 1; t <= TMAP && t->lno == lno; ++cnt1, ++t);

	/* Delete that number of lines from the screen. */
	MOVE(sp, p - HMAP, 0);
	for (cnt2 = cnt1; cnt2--;)
		if (svi_deleteln(sp))
			return (1);
	/*
	 * If the screen only contains one line, or, if the line was the
	 * entire screen, this gets fairly exciting.  Skip the fun.
	 */
	if (cnt1 == sp->t_rows) {
		if (cnt1 == 1)
			return (0);
		if (file_gline(sp, ep, ++lno, NULL))
			lno = file_lline(sp, ep);
		return (svi_sm_fill(sp, ep, lno, P_TOP));
	}
		
	/* Shift the screen map up. */
	memmove(p, p + cnt1, (((TMAP - p) - cnt1) + 1) * sizeof(SMAP));

	/* Decrement the line numbers for the rest of the map. */
	for (t = TMAP - cnt1; p <= t; ++p)
		--p->lno;

	/* Display the new lines. */
	for (p = TMAP - cnt1;;) {
		if (p < TMAP && svi_sm_next(sp, ep, p, p + 1))
			return (1);
		if (svi_line(sp, ep, ++p, NULL, 0, NULL, NULL))
			return (1);
		if (p == TMAP)
			break;
	}
	return (0);
}

/*
 * svi_sm_insert --
 *	Insert a line into the SMAP.
 */
int
svi_sm_insert(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	SMAP *p, *t;
	size_t cnt1, cnt2;

	/* Find the line in the map. */
        for (p = HMAP; p->lno != lno; ++p);

	/*
	 * Figure out how many lines needed to display the line.
	 * The lines left on the screen overrides that number.
	 */
	cnt1 = svi_screens(sp, ep, lno, NULL);
	cnt2 = (TMAP - p) + 1;
	if (cnt1 > cnt2)
		cnt1 = cnt2;

	/* Push down that many lines. */
	MOVE(sp, p - HMAP, 0);
	for (cnt2 = cnt1; cnt2--;)
		if (svi_insertln(sp))
			return (1);

	/*
	 * Clear the last line on the screen, it's going to have been
	 * corrupted.
	 */
	MOVE(sp, INFOLINE(sp), 0);
	clrtoeol();

	/* Shift the screen map down. */
	memmove(p + cnt1, p, (((TMAP - p) - cnt1) + 1) * sizeof(SMAP));

	/* Increment the line numbers for the rest of the map. */
	for (t = p + cnt1; t <= TMAP; ++t)
		++t->lno;

	/* Fill in the SMAP for the new lines. */
	for (cnt2 = 1, t = p; cnt2 <= cnt1; ++t, ++cnt2) {
		t->lno = lno;
		t->off = cnt2;
	}

	/* Display the new lines. */
	for (; cnt1--; ++p)
		if (svi_line(sp, ep, p, NULL, 0, NULL, NULL))
			return (1);
	return (0);
}

/*
 * svi_sm_reset --
 *	Reset a line in the SMAP.
 */
int
svi_sm_reset(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	SMAP *p, *t;
	size_t cnt;

	/*
	 * See if the number of screen rows taken up by the old display
	 * for the line is the same as the number needed for the new one.
	 * If so, simply repaint, otherwise do it the hard way.
	 */
        for (p = HMAP; p->lno != lno; ++p);
	for (cnt = 0, t = p; t->lno == lno && t <= TMAP; ++cnt, ++t);
	if (cnt == svi_screens(sp, ep, lno, NULL)) {
		do {
			if (svi_line(sp, ep, p, NULL, 0, NULL, NULL))
				return (1);
		} while (++p < t);
		return (0);
	}
	if (svi_sm_delete(sp, ep, lno))
		return (1);
	if (svi_sm_insert(sp, ep, lno))
		return (1);
	return (0);
}

/*
 * svi_sm_up --
 *	Scroll the SMAP up count logical lines.
 */
int
svi_sm_up(sp, ep, rp, count, cursor_move)
	SCR *sp;
	EXF *ep;
	MARK *rp;
	recno_t count;
	int cursor_move;
{
	SMAP *p, svmap, tmp;
	recno_t last;
	int scrolled;

	/* Set the default return position. */
	rp->lno = sp->lno;
	rp->cno = sp->cno;

	/*
	 * There are two forms of this command, one where the cursor follows
	 * the line, and one where it doesn't.  In the latter, we try and keep
	 * the cursor at the same position on the screen, but, if the screen
	 * is small enough and the line length large enough, the cursor can
	 * end up in very strange places.  Probably not worth fixing.
	 *
	 * Find the line in the SMAP.
	 */
	for (p = HMAP;; ++p) {
		if (p > TMAP) {
			msgq(sp, M_ERR,
			    "Line %lu not on the screen.", sp->lno);
			return (1);
		}
		if (p->lno == sp->lno)
			break;
	}

	last = file_lline(sp, ep);
	for (svmap = *p, scrolled = 0;; scrolled = 1) {
		if (count == 0)
			break;
		--count;

		/* Decide what would show up on the screen. */
		if (svi_sm_next(sp, ep, TMAP, &tmp))
			return (1);

		/* If the line doesn't exist, we're done. */
		if (tmp.lno > last)
			break;
			
		/* Scroll up one logical line. */
		if (svi_sm_1up(sp, ep))
			return (1);
		
		if (!cursor_move && p > HMAP)
			--p;
	}

	if (cursor_move) {
		/*
		 * If didn't move enough lines, it's an error if we're at the
		 * EOF, else move there.  Otherwise, try and place the cursor
		 * roughly where it was before.
		 */
		if (!scrolled || count) {
			if (sp->lno == TMAP->lno) {
				v_eof(sp, ep, NULL);
				return (1);
			}
			p = TMAP;
		}
	} else {
		/*
		 * If the line itself moved, invalidate the cursor, because
		 * the comparison with the old line/new line won't be right
		 */
		F_SET(sp, S_CUR_INVALID);

		/* It's an error if we didn't scroll enough. */
		if (!scrolled || count) {
			v_eof(sp, ep, NULL);
			return (1);
		}

		/* If the cursor moved off the screen, move it to the top. */
		if (sp->lno < HMAP->lno)
			p = HMAP;
	}
	/*
	 * On a logical movement, we try and keep the cursor as close as
	 * possible to the last position, but also set it up so that the
	 * next "real" movement will return the cursor to the closest position
	 * to the last real movement.
	 */
	if (p->lno != svmap.lno || p->off != svmap.off) {
		rp->lno = p->lno;
		rp->cno = svi_lrelative(sp, ep, p->lno, p->off);
	}
	return (0);
}

/*
 * svi_sm_1up --
 *	Scroll the SMAP up one.
 */
int
svi_sm_1up(sp, ep)
	SCR *sp;
	EXF *ep;
{
	/*
	 * Delete the top line of the screen.  Shift the screen map up.
	 * Display a new line at the bottom of the screen.
	 */
	MOVE(sp, 0, 0);
	if (svi_deleteln(sp))
		return (1);

	/* One-line screens can fail. */
	if (HMAP == TMAP) {
		if (svi_sm_next(sp, ep, TMAP, TMAP))
			return (1);
	} else {
		memmove(HMAP, HMAP + 1, sp->rows * sizeof(SMAP));
		if (svi_sm_next(sp, ep, TMAP - 1, TMAP))
			return (1);
	}
	if (svi_line(sp, ep, TMAP, NULL, 0, NULL, NULL))
		return (1);
	return (0);
}

/*
 * svi_deleteln --
 *	Delete a line a la curses.
 */
int
svi_deleteln(sp)
	SCR *sp;
{
	/*
	 * Delete the top line, scrolling everything else.  If
	 * we're not the bottom screen, put everything else back.
	 */
	deleteln();
	if (sp->child != NULL) {
		MOVE(sp, INFOLINE(sp), 0);
		insertln();
	}
	return (0);
}

/*
 * svi_sm_down --
 *	Scroll the SMAP down count logical lines.
 */
int
svi_sm_down(sp, ep, rp, count, cursor_move)
	SCR *sp;
	EXF *ep;
	MARK *rp;
	recno_t count;
	int cursor_move;
{
	SMAP *p, svmap;
	int scrolled;

	/* Set the default return position. */
	rp->lno = sp->lno;
	rp->cno = sp->cno;

	/*
	 * There are two forms of this command, one where the cursor follows
	 * the line, and one where it doesn't.  In the latter, we try and keep
	 * the cursor at the same position on the screen, but, if the screen
	 * is small enough and the line length large enough, the cursor can
	 * end up in very strange places.  Probably not worth fixing.
	 *
	 * Find the line in the SMAP.
	 */
	for (p = HMAP;; ++p) {
		if (p > TMAP) {
			msgq(sp, M_ERR,
			    "Line %lu not on the screen", sp->lno);
			return (1);
		}
		if (p->lno == sp->lno)
			break;
	}

	for (svmap = *p, scrolled = 0;; scrolled = 1) {
		if (count == 0)
			break;
		--count;

		/* If the line doesn't exist, we're done. */
		if (HMAP->lno == 1 && HMAP->off == 1)
			break;
			
		/* Scroll down one logical line. */
		if (svi_sm_1down(sp, ep))
			return (1);
		
		if (!cursor_move && p < TMAP)
			++p;
	}

	if (cursor_move) {
		/*
		 * If didn't move enough lines, it's an error if we're at the
		 * SOF, else move there.  Otherwise, try and place the cursor
		 * roughly where it was before.
		 */
		if (!scrolled || count) {
			if (sp->lno == HMAP->lno) {
				v_sof(sp, NULL);
				return (1);
			}
			p = HMAP;
		}
	} else {
		/*
		 * If the line itself moved, invalidate the cursor, because
		 * the comparison with the old line/new line won't be right
		 */
		F_SET(sp, S_CUR_INVALID);

		/* It's an error if we didn't scroll enough. */
		if (!scrolled || count) {
			v_sof(sp, NULL);
			return (1);
		}

		/* If the cursor moved off the screen, move it to the bottom. */
		if (sp->lno > TMAP->lno)
			p = TMAP;
	}

	/*
	 * On a logical movement, we try and keep the cursor as close as
	 * possible to the last position, but also set it up so that the
	 * next "real" movement will return the cursor to the closest position
	 * to the last real movement.
	 */
	if (p->lno != svmap.lno || p->off != svmap.off) {
		rp->lno = p->lno;
		rp->cno = svi_lrelative(sp, ep, p->lno, p->off);
	}
	return (0);
}

/*
 * svi_sm_1down --
 *	Scroll the SMAP down one.
 */
int
svi_sm_1down(sp, ep)
	SCR *sp;
	EXF *ep;
{
	/*
	 * Clear the bottom line of the screen, insert a line at the top
	 * of the screen.  Shift the screen map down, display a new line
	 * at the top of the screen.
	 */
	MOVE(sp, sp->t_rows, 0);
	clrtoeol();
	MOVE(sp, 0, 0);
	if (svi_insertln(sp))
		return (1);
	memmove(HMAP + 1, HMAP, sp->rows * sizeof(SMAP));
	if (svi_sm_prev(sp, ep, HMAP + 1, HMAP))
		return (1);
	if (svi_line(sp, ep, HMAP, NULL, 0, NULL, NULL))
		return (1);
	return (0);
}

/*
 * svi_insertln --
 *	insertln a line a la curses.
 */
int
svi_insertln(sp)
	SCR *sp;
{
	/*
	 * Insert at the current line, scrolling everything else.
	 * If we're not the bottom screen, put everything else back.
	 */
	insertln();
	if (sp->child != NULL) {
		MOVE(sp, INFOLINE(sp), 0);
		deleteln();
	}
	return (0);
}

/*
 * svi_sm_next --
 *	Fill in the next entry in the SMAP.
 */
int
svi_sm_next(sp, ep, p, t)
	SCR *sp;
	EXF *ep;
	SMAP *p, *t;
{
	size_t lcnt;

	if (O_ISSET(sp, O_LEFTRIGHT)) {
		t->lno = p->lno + 1;
		t->off = p->off;
	} else {
		lcnt = svi_screens(sp, ep, p->lno, NULL);
		if (lcnt == p->off) {
			t->lno = p->lno + 1;
			t->off = 1;
		} else {
			t->lno = p->lno;
			t->off = p->off + 1;
		}
	}
	return (0);
}

/*
 * svi_sm_prev --
 *	Fill in the previous entry in the SMAP.
 */
int
svi_sm_prev(sp, ep, p, t)
	SCR *sp;
	EXF *ep;
	SMAP *p, *t;
{
	if (O_ISSET(sp, O_LEFTRIGHT)) {
		t->lno = p->lno - 1;
		t->off = p->off;
	} else if (p->off != 1) {
		t->lno = p->lno;
		t->off = p->off - 1;
	} else {
		t->lno = p->lno - 1;
		t->off = svi_screens(sp, ep, t->lno, NULL);
	}
	return (t->lno == 0);
}

/*
 * svi_sm_position --
 *	Return the line number of the top, middle or last line on the screen.
 *	(The vi H, M and L commands.)  Here because only the screen routines
 *	know what's really out there.
 */
int
svi_sm_position(sp, ep, lnop, cnt, pos)
	SCR *sp;
	EXF *ep;
	recno_t *lnop;
	u_long cnt;
	enum position pos;
{
	SMAP *p, *t;
	recno_t down, last;
	
	switch (pos) {
	case P_TOP:
		/*
		 * Note, the top line number may not be at the top of the
		 * screen, because we search for a line that starts on the
		 * screen.  It works that way because that's how the historic
		 * vi behaved.
		 *
		 * Set t to point at the map entry one past the last legal
		 * entry in the map.
		 */
		last = file_lline(sp, ep);
		if (TMAP->lno <= last)
			t = TMAP + 1;
		else
			for (t = HMAP; t->lno <= last; ++t);

		/* Step past cnt start-of-lines, stopping at t. */
		for (p = HMAP - 1; cnt; --cnt)
			for (;;) {
				if (++p == t) {
					msgq(sp, M_ERR,
					    "No such line on the screen.");
					return (1);
				}
				if (p->off == 1)
					break;
			}
		*lnop = p->lno;
		break;
	case P_MIDDLE:
		/*
		 * Note, the middle line number may not be anywhere near the
		 * middle of the screen, because that's how the historic vi
		 * behaved.
		 *
		 * Check for less than a full screen of lines.
		 */
		last = file_lline(sp, ep);
		if (TMAP->lno < last)
			last = TMAP->lno;

		down = (last - HMAP->lno + 1) / 2;
		if (down == 0 && HMAP->off != 1) {
			msgq(sp, M_ERR, "No such line on the screen.");
			return (1);
		}
		*lnop = HMAP->lno + down;
		break;
	case P_BOTTOM:
		/* Set p to point at the last legal entry in the map. */
		last = file_lline(sp, ep);
		if (TMAP->lno <= last)
			p = TMAP + 1;
		else
			for (p = HMAP; p->lno <= last; ++p);

		/* Step past cnt start-of-lines, stopping at HMAP. */
		for (; cnt; --cnt)
			for (;;) {
				if (--p < HMAP) {
					msgq(sp, M_ERR,
					    "No such line on the screen.");
					return (1);
				}
				if (p->off == 1)
					break;
			}
		*lnop = p->lno;
		break;
	case P_FILL:
		abort();
		break;
	}
	return (0);
}

/*
 * svi_sm_nlines --
 *	Return the number of screen lines from an SMAP entry to the
 *	start of some file line, less than a maximum value.
 */
recno_t
svi_sm_nlines(sp, ep, from_sp, to_lno, max)
	SCR *sp;
	EXF *ep;
	SMAP *from_sp;
	recno_t to_lno;
	size_t max;
{
	recno_t lno, lcnt;

	if (O_ISSET(sp, O_LEFTRIGHT))
		if (from_sp->lno > to_lno)
			return (from_sp->lno - to_lno);
		else
			return (to_lno - from_sp->lno);

	if (from_sp->lno == to_lno)
		return (from_sp->off - 1);

	if (from_sp->lno > to_lno) {
		lcnt = from_sp->off - 1;	/* Correct for off-by-one. */
		for (lno = from_sp->lno; --lno >= to_lno && lcnt <= max;)
			lcnt += svi_screens(sp, ep, lno, NULL);
	} else {
		lno = from_sp->lno;
		lcnt = (svi_screens(sp, ep, lno, NULL) - from_sp->off) + 1;
		for (; ++lno < to_lno && lcnt <= max;)
			lcnt += svi_screens(sp, ep, lno, NULL);
	}
	return (lcnt);
}
