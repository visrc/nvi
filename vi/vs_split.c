/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_split.c,v 8.14 1993/11/13 18:01:24 bostic Exp $ (Berkeley) $Date: 1993/11/13 18:01:24 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "svi_screen.h"

/*
 * svi_split --
 *	Split the screen.
 */
int
svi_split(sp, argv)
	SCR *sp;
	char *argv[];
{
	SCR *tsp;
	SMAP *smp;
	size_t cnt, half;
	int issmallscreen, nochange, splitup;

	/* Check to see if it's possible. */
	half = sp->rows / 2;
	if (half < MINIMUM_SCREEN_ROWS) {
		msgq(sp, M_ERR, "Screen must be larger than %d to split",
		     MINIMUM_SCREEN_ROWS);
		return (1);
	}

#define	_HMAP(sp)	(((SVI_PRIVATE *)((sp)->svi_private))->h_smap)
#define	_TMAP(sp)	(((SVI_PRIVATE *)((sp)->svi_private))->t_smap)
	/* Get a new screen. */
	if (screen_init(sp, &tsp, 0))
		goto mem1;
	if ((SVP(tsp) = malloc(sizeof(SVI_PRIVATE))) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		goto mem2;
	}
	memset(SVP(tsp), 0, sizeof(SVI_PRIVATE));
	if ((_HMAP(tsp) = malloc(SIZE_HMAP * sizeof(SMAP))) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		goto mem3;
	}

/* INITIALIZED AT SCREEN CREATE. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	/*
	 * Small screens: see svi/svi_refresh.c:svi_refresh, section 3b.
	 * Set a flag so we know to fix the screen up later.
	 */
	issmallscreen = ISSMALLSCREEN(sp);

	/* Flag if we're changing screens. */
	nochange = argv == NULL;

	/*
	 * Split the screen, and link the screens together.  If the cursor
	 * is in the top half of the current screen, the new screen goes
	 * under the current screen.  Else, it goes above the current screen.
	 *
	 * Columns and the number of rows in the screen and window don't
	 * change.
	 */
	tsp->cols = sp->cols;
	tsp->w_rows = sp->w_rows;
	
	cnt = svi_sm_cursor(sp, sp->ep, &smp) ? 0 : smp - HMAP;
	if (cnt <= half) {			/* Parent is top half. */
		/* Child. */
		tsp->rows = sp->rows - half;
		tsp->s_off = sp->s_off + half;
		tsp->t_maxrows = tsp->rows - 1;

		/* Parent. */
		sp->rows = half;
		sp->t_maxrows = sp->rows - 1;

		/* Link into place. */
		if ((tsp->child = sp->child) != NULL)
			sp->child->parent = tsp;
		sp->child = tsp;
		tsp->parent = sp;

		splitup = 0;
	} else {				/* Parent is bottom half. */
		/* Child. */
		tsp->rows = sp->rows - half;
		tsp->s_off = sp->s_off;
		tsp->t_maxrows = tsp->rows - 1;

		/* Parent. */
		sp->rows = half;
		sp->s_off += tsp->rows;
		sp->t_maxrows = sp->rows - 1;

		/* Shift the parent's map down. */
		memmove(_HMAP(sp),
		    _HMAP(sp) + tsp->rows, sp->t_maxrows * sizeof(SMAP));

		/* Link into place. */
		if ((tsp->parent = sp->parent) != NULL)
			sp->parent->child = tsp;
		sp->parent = tsp;
		tsp->child = sp;

		splitup = 1;
	}

	/*
	 * Small screens: see svi/svi_refresh.c:svi_refresh, section 3b.
	 *
	 * The child may have different window options sizes than the
	 * parent, so use them.  Make sure that the minimum and current
	 * text counts aren't larger than the new screen sizes.
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
			tsp->t_rows = tsp->t_maxrows;

		/*
		 * If we split up, i.e. the child is on top, lines that
		 * were painted in the parent may not be painted in the
		 * child.  Clear any lines not being used in the child
		 * screen.
		 *
		 */
		if (splitup)
			for (cnt = tsp->t_rows; ++cnt <= tsp->t_maxrows;) {
				MOVE(tsp, cnt, 0)
				clrtoeol();
			}
	} else {
		sp->t_minrows = sp->t_rows = sp->rows - 1;

		/*
		 * The new screen may be a small screen, even though the
		 * parent was not.  Don't complain if O_WINDOW is too large,
		 * we're splitting the screen so the screen is much smaller
		 * than normal.  Clear any lines not being used in the child
		 * screen.
		 */
		tsp->t_minrows = tsp->t_rows = O_VAL(sp, O_WINDOW);
		if (tsp->t_rows > tsp->rows - 1)
			tsp->t_minrows = tsp->t_rows = tsp->rows - 1;
		else
			for (cnt = tsp->t_rows; ++cnt <= tsp->t_maxrows;) {
				MOVE(tsp, cnt, 0)
				clrtoeol();
			}
	}

	/* Adjust the ends of both maps. */
	_TMAP(sp) = _HMAP(sp) + (sp->t_rows - 1);
	_TMAP(tsp) = _HMAP(tsp) + (tsp->t_rows - 1);

	/*
	 * In any case, if the size of the scrolling region hasn't been
	 * modified by the user, reset it so it's reasonable for the split
	 * screen.
	 */
	if (!F_ISSET(&sp->opts[O_SCROLL], OPT_SET)) {
		O_VAL(sp, O_SCROLL) = sp->t_maxrows / 2;
		O_VAL(tsp, O_SCROLL) = sp->t_maxrows / 2;
	}

	/*
	 * If files specified, build the file list, else, link to the
	 * current file.
	 */
	if (argv != NULL && *argv != NULL) {
		for (; *argv != NULL; ++argv)
			if (file_add(tsp, NULL, *argv, 0) == NULL)
				goto mem4;
	} else {
		if (file_add(tsp, NULL, sp->frp->fname, 0) == NULL)
			goto mem4;
	}

	if ((tsp->frp = file_first(tsp, 0)) == NULL) {
		msgq(sp, M_ERR, "No files in the file list.");
		goto mem4;
	}

	/*
	 * Copy the file state flags, start the file.  Fill the child's
	 * screen map.  If the file is unchanged, keep the screen and
	 * cursor the same.
	 */
	if (nochange) {
		tsp->ep = sp->ep;
		++sp->ep->refcnt;

		tsp->frp->flags = sp->frp->flags;
		tsp->frp->lno = sp->lno;
		tsp->frp->cno = sp->cno;
		F_SET(tsp->frp, FR_CURSORSET);

		/* Copy the parent's map into the child's map. */
		memmove(_HMAP(tsp), _HMAP(sp), tsp->t_rows * sizeof(SMAP));
	} else {
		if (file_init(tsp, tsp->frp, NULL, 0))
			goto mem4;
		(void)svi_sm_fill(tsp, tsp->ep, 1, P_TOP);
	}

	/* Clear the information lines. */
	MOVE(sp, INFOLINE(sp), 0);
	clrtoeol();
	MOVE(tsp, INFOLINE(tsp), 0);
	clrtoeol();

	/* Redraw the status line for the parent screen, if it's on top. */
	if (splitup == 0)
		(void)status(sp, sp->ep, sp->lno, 0);

	/* Save the parent window's cursor information. */
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

	/* Completely redraw the child screen. */
	F_SET(tsp, S_REDRAW);

	/* Switch screens. */
	sp->snext = tsp;
	F_SET(sp, S_SSWITCH);
	return (0);

mem4:	FREE(_HMAP(tsp), SIZE_HMAP * sizeof(SMAP));
mem3:	FREE(SVP(sp), sizeof(SVI_PRIVATE));
mem2:	(void)screen_end(tsp);
mem1:	FREE(tsp, sizeof(SCR));
	return (1);
}
#undef	_HMAP
#undef	_TMAP

/*
 * svi_join --
 *	Join the dead screen into a related screen, if one exists, and
 *	return that screen.
 */
int
svi_join(dead, nsp)
	SCR *dead, **nsp;
{
	SCR *sp;
	size_t cnt;

	/* If a split screen, add space to parent/child. */
	if ((sp = dead->parent) == NULL) {
		if ((sp = dead->child) == NULL) {
			*nsp = NULL;
			return (0);
		}
		sp->s_off = dead->s_off;
	}
	sp->rows += dead->rows;
	if (ISSMALLSCREEN(sp)) {
		sp->t_maxrows += dead->rows;
		for (cnt = sp->t_rows; ++cnt <= sp->t_maxrows;) {
			MOVE(sp, cnt, 0);
			clrtoeol();
		}
		TMAP = HMAP + (sp->t_rows - 1);
	} else {
		sp->t_maxrows += dead->rows;
		sp->t_rows = sp->t_minrows = sp->t_maxrows;
		TMAP = HMAP + (sp->t_rows - 1);
		if (svi_sm_fill(sp, sp->ep, sp->lno, P_FILL))
			return (1);
		F_SET(sp, S_REDRAW);
	}

	/*
	 * If the size of the scrolling region hasn't been modified by
	 * the user, reset it so it's reasonable for the new screen.
	 */
	if (!F_ISSET(&sp->opts[O_SCROLL], OPT_SET))
		O_VAL(sp, O_SCROLL) = sp->t_maxrows / 2;

	*nsp = sp;
	return (0);
}
