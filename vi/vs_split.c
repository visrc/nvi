/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_split.c,v 8.7 1993/09/13 17:02:08 bostic Exp $ (Berkeley) $Date: 1993/09/13 17:02:08 $";
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
 *	Split the screen.  Keep the cursor line on the screen, and make
 *	reassembly easy by overallocating on the line maps and linking
 *	from top of the real screen to the bottom.
 */
int
svi_split(sp, argv)
	SCR *sp;
	char *argv[];
{
	SCR *tsp;
	size_t half;
	int nochange;

	/* Check to see if it's possible. */
	half = sp->rows / 2;
	if (half < MINIMUM_SCREEN_ROWS) {
		msgq(sp, M_ERR, "Screen not large enough to split");
		return (1);
	}

	/* Get a new screen, initialize. */
	if ((tsp = malloc(sizeof(SCR))) == NULL) {
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		return (1);
	}
	if (scr_init(sp, tsp))
		goto mem1;
	if ((SVP(tsp) = malloc(sizeof(SVI_PRIVATE))) == NULL) {
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		goto mem2;
	}
/* INITIALIZED AT SCREEN CREATE. */
	memset(SVP(tsp), 0, sizeof(SVI_PRIVATE));

#undef	HMAP
#undef	TMAP
#define	HMAP(sp)	(((SVI_PRIVATE *)((sp)->svi_private))->h_smap)
#define	TMAP(sp)	(((SVI_PRIVATE *)((sp)->svi_private))->t_smap)

	/*
	 * Build a screen map for the child -- large enough to accomodate
	 * the entire window.
	 */
	if ((HMAP(tsp) = malloc(SIZE_HMAP * sizeof(SMAP))) == NULL) {
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		goto mem3;
	}

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	if (SVP(sp)->VB != NULL &&
	    (SVP(tsp)->VB = strdup(SVP(sp)->VB)) == NULL)
		goto mem4;

	/* Split the screen, and link the screens together. */
	nochange = argv == NULL;
	if (sp->sc_row <= half) {		/* Parent is top half. */
		tsp->cols = sp->cols;		/* Stuff in common. */
		tsp->w_rows = sp->w_rows;

		tsp->rows = sp->rows - half;	/* Child. */
		tsp->t_rows = tsp->rows - 1;
		tsp->s_off = sp->s_off + half;
		TMAP(tsp) = HMAP(tsp) + (tsp->t_rows - 1);
		if (nochange)			/* Both start the same place.*/
			memmove(HMAP(tsp), HMAP(sp),
			    tsp->t_rows * sizeof(SMAP));

		sp->rows = half;		/* Parent. */
		sp->t_rows = sp->rows - 1;
		TMAP(sp) = HMAP(sp) + (sp->t_rows - 1);

		tsp->child = sp->child;
		if (sp->child != NULL)
			sp->child->parent = tsp;
		sp->child = tsp;
		tsp->parent = sp;
	} else {				/* Parent is bottom half. */
		tsp->cols = sp->cols;		/* Stuff in common. */
		tsp->w_rows = sp->w_rows;

		tsp->rows = sp->rows - half;	/* Child. */
		tsp->t_rows = tsp->rows - 1;
		tsp->s_off = sp->s_off;
		TMAP(tsp) = HMAP(tsp) + (tsp->t_rows - 1);
		if (nochange)			/* Both start the same place. */
			memmove(HMAP(tsp), TMAP(sp) - (tsp->t_rows - 1),
			    tsp->t_rows * sizeof(SMAP));

		sp->rows = half;		/* Parent. */
		sp->t_rows = sp->rows - 1;
		sp->s_off = sp->s_off + tsp->rows;
		memmove(HMAP(sp),		/* Copy the map down. */
		    TMAP(sp) - (sp->t_rows - 1), sp->t_rows * sizeof(SMAP));
		TMAP(sp) = HMAP(sp) + (sp->t_rows - 1);

		tsp->parent = sp->parent;
		if (sp->parent != NULL)
			sp->parent->child = tsp;
		sp->parent = tsp;
		tsp->child = sp;
	}

	/*
	 * If the size of the scrolling region hasn't been modified by
	 * the user, reset it so it's reasonable for the split screen.
	 */
	if (!F_ISSET(&sp->opts[O_SCROLL], OPT_SET)) {
		O_VAL(sp, O_SCROLL) = sp->t_rows / 2;
		O_VAL(tsp, O_SCROLL) = sp->t_rows / 2;
	}

	/* Init support routines. */
	svi_init(tsp);

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

	/* Start the file. */
	if ((tsp->ep = file_init(tsp,
	    nochange ? sp->ep : NULL, tsp->frp, NULL)) == NULL)
		goto mem4;

	/*
	 * Fill the child's screen map.  If the file is
	 * unchanged, keep the screen and cursor the same.
	 */
	if (nochange) {
		tsp->frp->lno = sp->lno;
		tsp->frp->cno = sp->cno;
		F_SET(tsp->frp, FR_CURSORSET);
	} else
		(void)svi_sm_fill(tsp, tsp->ep, sp->lno, P_FILL);

	/* Clear the information lines. */
	MOVE(sp, INFOLINE(sp), 0);
	clrtoeol();
	MOVE(tsp, INFOLINE(tsp), 0);
	clrtoeol();

	/* Refresh the parent screens, displaying the status line. */
	(void)status(sp, sp->ep, sp->lno, 0);
	(void)svi_refresh(sp, sp->ep);

	/* Completely redraw the child screen. */
	F_SET(tsp, S_REDRAW);

	/* Switch screens. */
	sp->snext = tsp;
	F_SET(sp, S_SSWITCH);
	return (0);

mem4:	FREE(HMAP(tsp), SIZE_HMAP * sizeof(SMAP));
mem3:	FREE(SVP(sp), sizeof(SVI_PRIVATE));
mem2:	(void)scr_end(tsp);
mem1:	FREE(tsp, sizeof(SCR));
	return (1);
}
