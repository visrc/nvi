/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_split.c,v 9.15 1995/02/15 11:57:44 bostic Exp $ (Berkeley) $Date: 1995/02/15 11:57:44 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <curses.h>
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "svi_screen.h"

/*
 * svi_split --
 *	Split the screen.
 */
int
svi_split(sp, topp, botp)
	SCR *sp, **topp, **botp;
{
	SCR *tsp;
	SMAP *smp;
	size_t cnt, half;
	int issmallscreen, splitup;

	/* Can be called from ex before we're ready. */
	if (!F_ISSET(SVP(sp), SVI_SCR_INIT)) {
		svi_message(sp, "split", SVIM_NOINIT);
		return (1);
	}

	/* Check to see if it's possible. */
	half = sp->rows / 2;
	if (half < MINIMUM_SCREEN_ROWS) {
		msgq(sp, M_ERR, "221|Screen must be larger than %d to split",
		     MINIMUM_SCREEN_ROWS);
		return (1);
	}

	/* Get a new screen. */
	if (screen_init(sp, &tsp))
		return (1);
	CALLOC(sp, _HMAP(tsp), SMAP *, SIZE_HMAP(sp), sizeof(SMAP));
	if (_HMAP(tsp) == NULL)
		return (1);

/* INITIALIZED AT SCREEN CREATE. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	/*
	 * Small screens: see svi/svi_refresh.c:svi_refresh, section 7a.
	 * Set a flag so we know to fix the screen up later.
	 */
	issmallscreen = IS_SMALL(sp);

	/*
	 * Split the screen, and link the screens together.  If the cursor
	 * is in the top half of the current screen, the new screen goes
	 * under the current screen.  Else, it goes above the current screen.
	 *
	 * The columns in the screen don't change.
	 */
	tsp->cols = sp->cols;

	/*
	 * Recalculate current cursor position based on sp->lno, we're called
	 * with the cursor on the colon command line.
	 */
	cnt = svi_sm_cursor(sp, &smp) ? 0 : smp - HMAP;
	if (cnt <= half) {			/* Parent is top half. */
		/* Child. */
		tsp->rows = sp->rows - half;
		tsp->woff = sp->woff + half;
		tsp->t_maxrows = IS_ONELINE(tsp) ? 1 : tsp->rows - 1;

		/* Parent. */
		sp->rows = half;
		sp->t_maxrows = IS_ONELINE(sp) ? 1 : sp->rows - 1;

		*topp = sp;
		*botp = tsp;
		splitup = 0;
	} else {				/* Parent is bottom half. */
		/* Child. */
		tsp->rows = sp->rows - half;
		tsp->woff = sp->woff;
		tsp->t_maxrows = IS_ONELINE(tsp) ? 1 : tsp->rows - 1;

		/* Parent. */
		sp->rows = half;
		sp->woff += tsp->rows;
		sp->t_maxrows = IS_ONELINE(sp) ? 1 : sp->rows - 1;

		/* Shift the parent's map down. */
		memmove(_HMAP(sp),
		    _HMAP(sp) + tsp->rows, sp->t_maxrows * sizeof(SMAP));

		*topp = tsp;
		*botp = sp;
		splitup = 1;
	}

	/*
	 * Small screens: see svi/svi_refresh.c:svi_refresh, section 7a.
	 *
	 * The child may have different screen options sizes than the
	 * parent, so use them.  Make sure that the text counts aren't
	 * larger than the new screen sizes.
	 */
	if (issmallscreen) {
		/* Fix the text line count for the parent. */
		if (splitup)
			sp->t_rows -= tsp->rows;

		/* Fix the parent screen. */
		if (sp->t_rows > sp->t_maxrows)
			sp->t_rows = sp->t_maxrows;
		if (sp->t_minrows > sp->t_maxrows)
			sp->t_minrows = sp->t_maxrows;

		/* Fix the child screen. */
		tsp->t_minrows = tsp->t_rows = O_VAL(sp, O_WINDOW);
		if (tsp->t_rows > tsp->t_maxrows)
			tsp->t_rows = tsp->t_maxrows;
		if (tsp->t_minrows > tsp->t_maxrows)
			tsp->t_minrows = tsp->t_maxrows;

		/*
		 * If we split up, i.e. the child is on top, lines that
		 * were painted in the parent may not be painted in the
		 * child.  Clear any lines not being used in the child
		 * screen.
		 */
		if (splitup)
			for (cnt = tsp->t_rows; ++cnt <= tsp->t_maxrows;) {
				(void)SVP(tsp)->scr_move(tsp,
				    RLNO(tsp, cnt), 0);
				(void)SVP(tsp)->scr_clrtoeol(tsp);
			}
	} else {
		sp->t_minrows = sp->t_rows = IS_ONELINE(sp) ? 1 : sp->rows - 1;

		/*
		 * The new screen may be a small screen, even though the
		 * parent was not.  Don't complain if O_WINDOW is too large,
		 * we're splitting the screen so the screen is much smaller
		 * than normal.  Clear any lines not being used in the child
		 * screen.
		 */
		tsp->t_minrows = tsp->t_rows = O_VAL(sp, O_WINDOW);
		if (tsp->t_rows > tsp->rows - 1)
			tsp->t_minrows = tsp->t_rows =
			    IS_ONELINE(tsp) ? 1 : tsp->rows - 1;
		else
			for (cnt = tsp->t_rows; ++cnt <= tsp->t_maxrows;) {
				(void)SVP(tsp)->scr_move(tsp,
				    RLNO(tsp, cnt), 0);
				(void)SVP(tsp)->scr_clrtoeol(tsp);
			}
	}

	/* Copy the parent's map into the child's map. */
	memmove(_HMAP(tsp), _HMAP(sp), tsp->t_rows * sizeof(SMAP));

	/* Adjust the ends of both maps. */
	_TMAP(sp) = IS_ONELINE(sp) ?
	    _HMAP(sp) : _HMAP(sp) + (sp->t_rows - 1);
	_TMAP(tsp) = IS_ONELINE(tsp) ?
	    _HMAP(tsp) : _HMAP(tsp) + (tsp->t_rows - 1);

	/* Reset the length of the default scroll. */
	if ((sp->defscroll = sp->t_maxrows / 2) == 0)
		sp->defscroll = 1;
	if ((tsp->defscroll = tsp->t_maxrows / 2) == 0)
		tsp->defscroll = 1;

	/* The new screen has to be drawn from scratch. */
	F_SET(tsp, S_SCR_REFORMAT);

	return (0);
}

/*
 * svi_bg --
 *	Background the screen, and switch to the next one.
 */
int
svi_bg(csp)
	SCR *csp;
{
	SCR *sp;

	/* Can be called from ex before we're ready. */
	if (!F_ISSET(SVP(csp), SVI_SCR_INIT)) {
		svi_message(csp, "bg", SVIM_NOINIT);
		return (1);
	}

	/* Try and join with another screen. */
	if ((svi_join(csp, NULL, NULL, &sp)))
		return (1);
	if (sp == NULL) {
		msgq(csp, M_ERR,
		    "222|You may not background your only displayed screen");
		return (1);
	}

	/* Move the old screen to the hidden queue. */
	SIGBLOCK(csp->gp);
	CIRCLEQ_REMOVE(&csp->gp->dq, csp, q);
	CIRCLEQ_INSERT_TAIL(&csp->gp->hq, csp, q);
	SIGUNBLOCK(csp->gp);

	/* Switch screens. */
	csp->nextdisp = sp;
	F_SET(csp, S_SSWITCH);

	return (0);
}

/*
 * svi_join --
 *	Join the screen into a related screen, if one exists,
 *	and return that screen.
 */
int
svi_join(csp, prev, next, nsp)
	SCR *csp, *prev, *next, **nsp;
{
	SCR *sp;
	SMAP *p;
	SVI_PRIVATE *svp;
	size_t cnt;
	int shift;

	/*
	 * If a split screen, add space to the specified previous or next
	 * screen, or, if none specified, to the parent/child.  Make no
	 * effort to clean up the screen's values.  If it's not exiting,
	 * we'll get it when the user asks to display it again.
	 */
	if ((sp = prev) != NULL) {
		shift = 0;
		goto found;
	}
	if ((sp = next) != NULL) {
		shift = 1;
		sp->woff = csp->woff;
		goto found;
	}
	if ((sp = csp->q.cqe_prev) != (void *)&csp->gp->dq) {
		shift = 0;
		goto found;
	}
	if ((sp = csp->q.cqe_next) != (void *)&csp->gp->dq) {
		shift = 1;
		sp->woff = csp->woff;
		goto found;
	}

	if (nsp != NULL)
		*nsp = NULL;
	return (0);

found:	sp->rows += csp->rows;

	svp = SVP(sp);
	if (IS_SMALL(sp)) {
		sp->t_maxrows += csp->rows;
		for (cnt = sp->t_rows; ++cnt <= sp->t_maxrows;) {
			(void)svp->scr_move(sp, RLNO(sp, cnt), 0);
			(void)svp->scr_clrtoeol(sp);
		}
		TMAP = HMAP + (sp->t_rows - 1);
	} else {
		if (!IS_ONELINE(csp)) {
			sp->t_maxrows += csp->rows;
			sp->t_rows = sp->t_minrows = sp->t_maxrows;
		}
		if (shift) {
			/*
			 * Shift the screen map down, and fill in the missing
			 * information.  If we fail, assume it's because there
			 * aren't enough lines and fill in from scratch.
			 */
			memmove(HMAP + csp->rows,
			    HMAP, csp->rows * sizeof(SMAP));
			for (p = HMAP + csp->rows; p > HMAP;) {
				if (svi_sm_prev(sp, p, p - 1)) {
					if (svi_sm_fill(sp, 1, P_TOP))
						return (1);
					break;
				}
				/* svi_sm_prev() flushed the cache. */
				if (svi_line(sp, --p, NULL, NULL))
					return (1);
			}
			TMAP = HMAP + (sp->t_rows - 1);
		} else
			/* Fill in the end of the screen map. */
			for (p = TMAP,
			    TMAP = HMAP + (sp->t_rows - 1); p < TMAP;) {
				if (svi_sm_next(sp, p, p + 1))
					return (1);
				/* svi_sm_next() flushed the cache. */
				if (svi_line(sp, ++p, NULL, NULL))
					return (1);
			}
	}

	/* Reset the length of the default scroll. */
	sp->defscroll = sp->t_maxrows / 2;

	/*
	 * Save the old screen's cursor information.
	 *
	 * XXX
	 * If called after file_end(), if the underlying file was a tmp
	 * file it may have gone away.
	 */
	if (csp->frp != NULL) {
		csp->frp->lno = csp->lno;
		csp->frp->cno = csp->cno;
		F_SET(csp->frp, FR_CURSORSET);
	}

	if (nsp != NULL)
		*nsp = sp;
	return (0);
}

/*
 * svi_fg --
 *	Background the current screen, and foreground a new one.
 */
int
svi_fg(csp, name)
	SCR *csp;
	CHAR_T *name;
{
	SCR *sp;
	int nf;
	char *p;

	/* Can be called from ex before we're ready. */
	if (!F_ISSET(SVP(csp), SVI_SCR_INIT)) {
		svi_message(csp, "fg", SVIM_NOINIT);
		return (1);
	}

	if (svi_swap(csp, &sp, name))
		return (1);
	if (sp == NULL) {
		if (name == NULL)
			msgq(csp, M_ERR, "223|There are no background screens");
		else {
			p = msg_print(sp, name, &nf);
			msgq(csp, M_ERR,
		    "224|There's no background screen editing a file named %s",
			    p);
			if (nf)
				FREE_SPACE(sp, p, 0);
		}
		return (1);
	}

	/* Move the old screen to the hidden queue. */
	SIGBLOCK(csp->gp);
	CIRCLEQ_REMOVE(&csp->gp->dq, csp, q);
	CIRCLEQ_INSERT_TAIL(&csp->gp->hq, csp, q);
	SIGUNBLOCK(csp->gp);

	return (0);
}

/*
 * svi_swap --
 *	Swap the current screen with a hidden one.
 */
int
svi_swap(csp, nsp, name)
	SCR *csp, **nsp;
	char *name;
{
	SCR *sp;
	int issmallscreen;

	/* Find the screen, or, if name is NULL, the first screen. */
	for (sp = csp->gp->hq.cqh_first;
	    sp != (void *)&csp->gp->hq; sp = sp->q.cqe_next)
		if (name == NULL || !strcmp(sp->frp->name, name))
			break;
	if (sp == (void *)&csp->gp->hq) {
		*nsp = NULL;
		return (0);
	}
	*nsp = sp;

	/*
	 * Save the old screen's cursor information.
	 *
	 * XXX
	 * If called after file_end(), if the underlying file was a tmp
	 * file it may have gone away.
	 */
	if (csp->frp != NULL) {
		csp->frp->lno = csp->lno;
		csp->frp->cno = csp->cno;
		F_SET(csp->frp, FR_CURSORSET);
	}

	/* Switch screens. */
	csp->nextdisp = sp;
	F_SET(csp, S_SSWITCH);

	/* Initialize terminal information. */
	SVP(sp)->srows = SVP(csp)->srows;

	issmallscreen = IS_SMALL(sp);

	/* Initialize screen information. */
	sp->rows = csp->rows;
	sp->cols = csp->cols;
	sp->woff = csp->woff;

	/*
	 * Small screens: see svi/svi_refresh.c:svi_refresh, section 7a.
	 *
	 * The new screens may have different screen options sizes than the
	 * old one, so use them.  Make sure that text counts aren't larger
	 * than the new screen sizes.
	 */
	if (issmallscreen) {
		sp->t_minrows = sp->t_rows = O_VAL(sp, O_WINDOW);
		if (sp->t_rows > csp->t_maxrows)
			sp->t_rows = sp->t_maxrows;
		if (sp->t_minrows > csp->t_maxrows)
			sp->t_minrows = sp->t_maxrows;
	} else
		sp->t_rows = sp->t_maxrows = sp->t_minrows = sp->rows - 1;

	/* Reset the length of the default scroll. */
	sp->defscroll = sp->t_maxrows / 2;

	/*
	 * Don't change the screen's cursor information other than to
	 * note that the cursor is wrong.
	 */
	F_SET(SVP(sp), SVI_CUR_INVALID);

	/*
	 * The HMAP may be NULL, if the screen got resized and
	 * a bunch of screens had to be hidden.
	 */
	if (HMAP == NULL)
		CALLOC_RET(sp, HMAP, SMAP *, SIZE_HMAP(sp), sizeof(SMAP));
	TMAP = HMAP + (sp->t_rows - 1);

	/* Fill the map. */
	if (svi_sm_fill(sp, sp->lno, P_FILL))
		return (1);

	/*
	 * The new screen replaces the old screen in the parent/child list.
	 * We insert the new screen after the old one.  If we're exiting,
	 * the exit will delete the old one, if we're foregrounding, the fg
	 * code will move the old one to the hidden queue.
	 */
	SIGBLOCK(sp->gp);
	CIRCLEQ_REMOVE(&sp->gp->hq, sp, q);
	CIRCLEQ_INSERT_AFTER(&csp->gp->dq, csp, sp, q);
	SIGUNBLOCK(sp->gp);

	F_SET(sp, S_SCR_REDRAW);
	return (0);
}

/*
 * svi_resize --
 *	Change the absolute size of the current screen.
 */
int
svi_resize(sp, count, adj)
	SCR *sp;
	long count;
	adj_t adj;
{
	SCR *g, *s;

	/* Can be called from ex before we're ready. */
	if (!F_ISSET(SVP(sp), SVI_SCR_INIT)) {
		svi_message(sp, "resize", SVIM_NOINIT);
		return (1);
	}

	/*
	 * Figure out which screens will grow, which will shrink, and
	 * make sure it's possible.
	 */
	if (count == 0)
		return (0);
	if (adj == A_SET) {
		if (sp->t_maxrows == count)
			return (0);
		if (sp->t_maxrows > count) {
			adj = A_DECREASE;
			count = sp->t_maxrows - count;
		} else {
			adj = A_INCREASE;
			count = count - sp->t_maxrows;
		}
	}
	if (adj == A_DECREASE) {
		if (count < 0)
			count = -count;
		s = sp;
		if (s->t_maxrows < MINIMUM_SCREEN_ROWS + count)
			goto toosmall;
		if ((g = sp->q.cqe_prev) == (void *)&sp->gp->dq) {
			if ((g = sp->q.cqe_next) == (void *)&sp->gp->dq)
				goto toobig;
			g->woff -= count;
		} else
			s->woff += count;
	} else {
		g = sp;
		if ((s = sp->q.cqe_next) != (void *)&sp->gp->dq)
			if (s->t_maxrows < MINIMUM_SCREEN_ROWS + count)
				s = NULL;
			else
				s->woff += count;
		else
			s = NULL;
		if (s == NULL) {
			if ((s = sp->q.cqe_prev) == (void *)&sp->gp->dq) {
toobig:				msgq(sp, M_BERR, adj == A_DECREASE ?
				    "225|The screen cannot shrink" :
				    "226|The screen cannot grow");
				return (1);
			}
			if (s->t_maxrows < MINIMUM_SCREEN_ROWS + count) {
toosmall:			msgq(sp, M_BERR,
				    "227|The screen can only shrink to %d rows",
				    MINIMUM_SCREEN_ROWS);
				return (1);
			}
			g->woff -= count;
		}
	}

	/*
	 * Update the screens; we could optimize the reformatting of the
	 * screen, but this isn't likely to be a common enough operation
	 * to make it worthwhile.
	 */
	g->rows += count;
	g->t_rows += count;
	if (g->t_minrows == g->t_maxrows)
		g->t_minrows += count;
	g->t_maxrows += count;
	_TMAP(g) += count;
	(void)msg_status(g, g->lno, 0);
	F_SET(g, S_SCR_REFORMAT);

	s->rows -= count;
	s->t_rows -= count;
	s->t_maxrows -= count;
	if (s->t_minrows > s->t_maxrows)
		s->t_minrows = s->t_maxrows;
	_TMAP(s) -= count;
	(void)msg_status(s, s->lno, 0);
	F_SET(s, S_SCR_REFORMAT);

	return (0);
}
