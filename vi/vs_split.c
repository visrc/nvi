/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_split.c,v 8.4 1993/08/29 15:21:03 bostic Exp $ (Berkeley) $Date: 1993/08/29 15:21:03 $";
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
	EXF *ep;
	SCR *tsp;
	size_t half;

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
	if ((tsp->svi_private = malloc(sizeof(SVI_PRIVATE))) == NULL) {
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		goto mem2;
	}

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

	/* Split the screen, and link the screens together. */
	if (sp->sc_row <= half) {		/* Parent is top half. */
		tsp->rows = sp->rows - half;	/* Child. */
		tsp->cols = sp->cols;
		tsp->t_rows = tsp->rows - 1;
		tsp->w_rows = sp->w_rows;
		tsp->s_off = sp->s_off + half;
		tsp->sc_col = tsp->sc_row = 0;
		TMAP(tsp) = HMAP(tsp) + (tsp->t_rows - 1);

		sp->rows = half;		/* Parent. */
		sp->t_rows = sp->rows - 1;
		TMAP(sp) = HMAP(sp) + (sp->t_rows - 1);

		tsp->child = sp->child;
		if (sp->child != NULL)
			sp->child->parent = tsp;
		sp->child = tsp;
		tsp->parent = sp;
	} else {				/* Parent is bottom half. */
		tsp->rows = sp->rows - half;	/* Child. */
		tsp->cols = sp->cols;
		tsp->t_rows = tsp->rows - 1;
		tsp->w_rows = sp->w_rows;
		tsp->s_off = sp->s_off;
		tsp->sc_col = tsp->sc_row = 0;
		TMAP(tsp) = HMAP(tsp) + (tsp->t_rows - 1);

		sp->rows = half;		/* Parent. */
		sp->t_rows = sp->rows - 1;
		sp->s_off = sp->s_off + half;
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
		ep = NULL;
	} else {
		if (file_add(tsp, NULL, sp->frp->fname, 0) == NULL)
			goto mem4;
		ep = sp->ep;
	}

	if ((tsp->frp = file_first(tsp, 0)) == NULL) {
		msgq(sp, M_ERR, "No files in the file list.");
		goto mem4;
	}

	/* Start the file. */
	if ((tsp->ep = file_init(tsp, ep, tsp->frp, NULL)) == NULL)
		goto mem4;

	/* Fill the child's screen map. */
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
mem3:	FREE(sp->svi_private, sizeof(SVI_PRIVATE));
mem2:	(void)scr_end(tsp);
mem1:	FREE(tsp, sizeof(SCR));
	return (1);
}
