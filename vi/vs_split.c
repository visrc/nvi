/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_split.c,v 10.5 1995/06/08 19:02:20 bostic Exp $ (Berkeley) $Date: 1995/06/08 19:02:20 $";
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

#include "common.h"
#include "vi.h"

/*
 * vs_split --
 *	Create a new screen.
 *
 * PUBLIC: int vs_split __P((SCR *, SCR *));
 */
int
vs_split(sp, new)
	SCR *sp, *new;
{
	SMAP *smp;
	size_t half;
	int issmallscreen, splitup;

	/* Check to see if it's possible. */
	half = sp->rows / 2;
	if (half < MINIMUM_SCREEN_ROWS) {
		msgq(sp, M_ERR, "221|Screen must be larger than %d to split",
		     MINIMUM_SCREEN_ROWS);
		return (1);
	}

	/* Get a new screen map. */
	CALLOC(sp, _HMAP(new), SMAP *, SIZE_HMAP(sp), sizeof(SMAP));
	if (_HMAP(new) == NULL)
		return (1);
	_HMAP(new)->lno = sp->lno;

	/*
	 * Small screens: see vs_refresh.c section 6a.  Set a flag so
	 * we know to fix the screen up later.
	 */
	issmallscreen = IS_SMALL(sp);

	/*
	 * Split the screen, and link the screens together.  If the cursor
	 * is in the top half of the current screen, the new screen goes
	 * under the current screen.  Else, it goes above the current screen.
	 *
	 * The columns in the screen don't change.
	 */
	new->cols = sp->cols;

	/*
	 * Recalculate current cursor position based on sp->lno, we're called
	 * with the cursor on the colon command line.
	 */
	splitup = (vs_sm_cursor(sp, &smp) ? 0 : smp - HMAP) > half;
		
	/* Get new screen real-estate. */
	if (sp->gp->scr_split(sp, new, splitup))
		return (1);

	/* Enter the new screen into the queue. */
	SIGBLOCK(sp->gp);
	if (splitup) {				/* Link in before old. */
		CIRCLEQ_INSERT_BEFORE(&sp->gp->dq, sp, new, q);
	} else {				/* Link in after old. */
		CIRCLEQ_INSERT_AFTER(&sp->gp->dq, sp, new, q);
	}
	SIGUNBLOCK(sp->gp);

	/*
	 * If the parent is the bottom half of the screen, shift the map
	 * down to match on-screen text.
	 */
	if (splitup)
		memmove(_HMAP(sp),
		    _HMAP(sp) + new->rows, sp->t_maxrows * sizeof(SMAP));

	/*
	 * Small screens: see vs_refresh.c, section 6a.
	 *
	 * The child may have different screen options sizes than the parent,
	 * so use them.  Guarantee that text counts aren't larger than the
	 * new screen sizes.
	 */
	if (issmallscreen) {
		/* Fix the text line count for the parent. */
		if (splitup)
			sp->t_rows -= new->rows;

		/* Fix the parent screen. */
		if (sp->t_rows > sp->t_maxrows)
			sp->t_rows = sp->t_maxrows;
		if (sp->t_minrows > sp->t_maxrows)
			sp->t_minrows = sp->t_maxrows;

		/* Fix the child screen. */
		new->t_minrows = new->t_rows = O_VAL(sp, O_WINDOW);
		if (new->t_rows > new->t_maxrows)
			new->t_rows = new->t_maxrows;
		if (new->t_minrows > new->t_maxrows)
			new->t_minrows = new->t_maxrows;
	} else {
		sp->t_minrows = sp->t_rows = IS_ONELINE(sp) ? 1 : sp->rows - 1;

		/*
		 * The new screen may be a small screen, even if the parent
		 * was not.  Don't complain if O_WINDOW is too large, we're
		 * splitting the screen so the screen is much smaller than
		 * normal.
		 */
		new->t_minrows = new->t_rows = O_VAL(sp, O_WINDOW);
		if (new->t_rows > new->rows - 1)
			new->t_minrows = new->t_rows =
			    IS_ONELINE(new) ? 1 : new->rows - 1;
	}

	/* Adjust maximum text count. */
	sp->t_maxrows = IS_ONELINE(sp) ? 1 : sp->rows - 1;
	new->t_maxrows = IS_ONELINE(new) ? 1 : new->rows - 1;

	/* Adjust the ends of the new and old maps. */
	_TMAP(sp) = IS_ONELINE(sp) ?
	    _HMAP(sp) : _HMAP(sp) + (sp->t_rows - 1);
	_TMAP(new) = IS_ONELINE(new) ?
	    _HMAP(new) : _HMAP(new) + (new->t_rows - 1);

	/* Reset the length of the default scroll. */
	if ((sp->defscroll = sp->t_maxrows / 2) == 0)
		sp->defscroll = 1;
	if ((new->defscroll = new->t_maxrows / 2) == 0)
		new->defscroll = 1;

	/* The new screen has to be drawn from scratch. */
	F_SET(new, S_SCR_REFORMAT | S_STATUS);
	return (0);
}

/*
 * vs_discard --
 *	Discard the screen, folding the real-estate into a related screen,
 *	if one exists, and return that screen.
 *
 * PUBLIC: int vs_discard __P((SCR *, SCR **));
 */
int
vs_discard(csp, spp)
	SCR *csp, **spp;
{
	SCR *sp;

	/* Discard the underlying screen. */
	if (csp->gp->scr_discard(csp, &sp))
		return (1);
	if (spp != NULL)
		*spp = sp;
	if (sp == NULL)
		return (0);
		
	/*
	 * Make no effort to clean up the discarded screen's information.  If
	 * it's not exiting, we'll do the work when the user redisplays it.
	 *
	 * Small screens: see vs_refresh.c section 6a.  Adjust text line info,
	 * unless it's a small screen.
	 *
	 * Reset the length of the default scroll.
	 */
	if (!IS_SMALL(sp))
		sp->t_rows = sp->t_minrows = sp->rows - 1;
	sp->t_maxrows = sp->rows - 1;
	sp->defscroll = sp->t_maxrows / 2;
	TMAP = HMAP + (sp->t_rows - 1);

	/*
	 * Save the old screen's cursor information.
	 *
	 * XXX
	 * If called after file_end(), if the underlying file was a tmp file
	 * it may have gone away.
	 */
	if (csp->frp != NULL) {
		csp->frp->lno = csp->lno;
		csp->frp->cno = csp->cno;
		F_SET(csp->frp, FR_CURSORSET);
	}

	/*
	 * The new screen has to be drawn from scratch.
	 *
	 * XXX
	 * Actually, we could play games with the map, but it's unclear it's
	 * worth the effort.
	 */
	F_SET(sp, S_SCR_REFORMAT);
	return (0);
}

/*
 * vs_fg --
 *	Background the current screen, and foreground a new one.
 *
 * PUBLIC: int vs_fg __P((SCR *, CHAR_T *));
 */
int
vs_fg(csp, name)
	SCR *csp;
	CHAR_T *name;
{
	SCR *sp;
	int nf;
	char *p;

	if (vs_swap(csp, &sp, name))
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
 * vs_bg --
 *	Background the screen, and switch to the next one.
 *
 * PUBLIC: int vs_bg __P((SCR *));
 */
int
vs_bg(csp)
	SCR *csp;
{
	SCR *sp;

	/* Try and join with another screen. */
	if (vs_discard(csp, &sp))
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

	/* Toss the screen map. */
	FREE(_HMAP(csp), SIZE_HMAP(csp) * sizeof(SMAP));
	_HMAP(csp) = NULL;

	/* Switch screens. */
	csp->nextdisp = sp;
	F_SET(csp, S_SSWITCH | S_STATUS);

	return (0);
}

/*
 * vs_swap --
 *	Swap the current screen with a hidden one.
 *
 * PUBLIC: int vs_swap __P((SCR *, SCR **, char *));
 */
int
vs_swap(csp, nsp, name)
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
	 * If called after file_end(), if the underlying file was a tmp file
	 * it may have gone away.
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
	VIP(sp)->srows = VIP(csp)->srows;

	issmallscreen = IS_SMALL(sp);

	/* Initialize screen information. */
	sp->cols = csp->cols;
	sp->rows = csp->rows;	/* XXX: Only place in vi that sets rows. */
	sp->woff = csp->woff;	/* XXX: Only place in vi that sets woff. */

	/*
	 * Small screens: see vs_refresh.c, section 6a.
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

	/* Allocate a new screen map. */
	CALLOC_RET(sp, HMAP, SMAP *, SIZE_HMAP(sp), sizeof(SMAP));
	TMAP = HMAP + (sp->t_rows - 1);

	/* Fill the map. */
	if (vs_sm_fill(sp, sp->lno, P_FILL))
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

	/*
	 * Don't change the screen's cursor information other than to
	 * note that the cursor is wrong.
	 */
	F_SET(VIP(sp), VIP_CUR_INVALID);

	/* Redraw from scratch. */
	F_SET(sp, S_SCR_REDRAW);
	return (0);
}

/*
 * vs_resize --
 *	Change the absolute size of the current screen.
 *
 * PUBLIC: int vs_resize __P((SCR *, long, adj_t));
 */
int
vs_resize(sp, count, adj)
	SCR *sp;
	long count;
	adj_t adj;
{
	SCR *g, *s;
	size_t g_off, s_off;

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

	g_off = s_off = 0;
	if (adj == A_DECREASE) {
		if (count < 0)
			count = -count;
		s = sp;
		if (s->t_maxrows < MINIMUM_SCREEN_ROWS + count)
			goto toosmall;
		if ((g = sp->q.cqe_prev) == (void *)&sp->gp->dq) {
			if ((g = sp->q.cqe_next) == (void *)&sp->gp->dq)
				goto toobig;
			g_off = -count;
		} else
			s_off = count;
	} else {
		g = sp;
		if ((s = sp->q.cqe_next) != (void *)&sp->gp->dq)
			if (s->t_maxrows < MINIMUM_SCREEN_ROWS + count)
				s = NULL;
			else
				s_off = count;
		else
			s = NULL;
		if (s == NULL) {
			if ((s = sp->q.cqe_prev) == (void *)&sp->gp->dq)
				goto toobig;
			if (s->t_maxrows < MINIMUM_SCREEN_ROWS + count) {
toosmall:			msgq(sp, M_BERR,
				    "227|The screen can only shrink to %d rows",
				    MINIMUM_SCREEN_ROWS);
				return (1);
			}
			g_off = -count;
		}
	}

	/* Update the underlying screens. */
	if (sp->gp->scr_resize(s, -count, s_off, g, count, g_off)) {
toobig:		msgq(sp, M_BERR, adj == A_DECREASE ?
		    "225|The screen cannot shrink" :
		    "226|The screen cannot grow");
		return (1);
	}

	/*
	 * Fix up the screens; we could optimize the reformatting of the
	 * screen, but this isn't likely to be a common enough operation
	 * to make it worthwhile.
	 */
	g->t_rows += count;
	if (g->t_minrows == g->t_maxrows)
		g->t_minrows += count;
	g->t_maxrows += count;
	_TMAP(g) += count;
	F_SET(g, S_SCR_REFORMAT | S_STATUS);

	s->t_rows -= count;
	s->t_maxrows -= count;
	if (s->t_minrows > s->t_maxrows)
		s->t_minrows = s->t_maxrows;
	_TMAP(s) -= count;
	F_SET(s, S_SCR_REFORMAT | S_STATUS);

	return (0);
}
