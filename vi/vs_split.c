/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_split.c,v 5.2 1993/04/06 11:44:50 bostic Exp $ (Berkeley) $Date: 1993/04/06 11:44:50 $";
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
svi_split(sp, ep)
	SCR *sp;
	EXF *ep;
{
	SCR *tsp;
	size_t half;

	/* Check to see if it's possible. */
	half = sp->rows / 2;
	if (half < MINIMUM_SCREEN_ROWS) {
		msgq(sp, M_ERROR, "Screen not large enough to split");
		return (1);
	}

	/* Get a new screen, initialize. */
	if ((tsp = malloc(sizeof(SCR))) == NULL) {
		msgq(sp, M_ERROR, "Error: %s", strerror(errno));
		return (1);
	}
	if (scr_init(sp, tsp)) {
		free(tsp);
		return (1);
	}

	/* Start the file. */
	if ((tsp->ep = file_start(tsp, ep)) == NULL) {
		free(tsp);
		return (1);
	}

	/*
	 * Build a screen map for the child -- large enough to accomodate
	 * the entire window.
	 */
	if ((tsp->h_smap = malloc(sp->w_rows * sizeof(SMAP))) == NULL) {
		free(tsp);
		return (1);
	}

	/* Split the screen, and link the screens together. */
	if (sp->sc_row <= half) {		/* Parent is top half. */
		tsp->rows = sp->rows - half;	/* Child. */
		tsp->cols = sp->cols;
		tsp->t_rows = tsp->rows - 1;
		tsp->w_rows = sp->w_rows;
		tsp->s_off = sp->s_off + half;
		tsp->sc_col = tsp->sc_row = 0;
		tsp->t_smap = tsp->h_smap + (tsp->t_rows - 1);

		sp->rows = half;		/* Parent. */
		sp->t_rows = sp->rows - 1;
		sp->s_off = sp->s_off;
		sp->t_smap = sp->h_smap + (sp->t_rows - 1);

		tsp->child = sp->child;
		if (sp->child != NULL)
			sp->child->parent = tsp;
		sp->child = tsp;
		tsp->parent = sp;

		/* Clear the old information line. */
		MOVE(sp, INFOLINE(sp), 0);
		clrtoeol();

		svi_divider(sp);
	} else {				/* Parent is bottom half. */
		tsp->rows = sp->rows - half;	/* Child. */
		tsp->cols = sp->cols;
		tsp->t_rows = tsp->rows - 1;
		tsp->w_rows = sp->w_rows;
		tsp->s_off = sp->s_off;
		tsp->sc_col = tsp->sc_row = 0;
		tsp->t_smap = tsp->h_smap + (tsp->t_rows - 1);

		sp->rows = half;		/* Parent. */
		sp->t_rows = sp->rows - 1;
		sp->s_off = sp->s_off + half;
		sp->t_smap = sp->h_smap + sp->t_rows;

		tsp->parent = sp->parent;
		if (sp->parent != NULL)
			sp->parent->child = tsp;
		sp->parent = tsp;
		tsp->child = sp;
	}

	/* Initialize support routines. */
	tsp->bell	= svi_bell;
	tsp->change	= svi_change;
	tsp->confirm	= svi_confirm;
	tsp->down	= svi_sm_down;
	tsp->end	= svi_end;
	tsp->exwrite	= svi_exwrite;
	tsp->fill	= svi_sm_fill;
	tsp->gb		= svi_gb;
	tsp->position	= svi_sm_position;
	tsp->refresh	= svi_refresh;
	tsp->relative	= svi_relative;
	tsp->split	= svi_split;
	tsp->up		= svi_sm_up;
	tsp->vex	= svi_vex;

	/* Fill the child's screen map. */
	(void)svi_sm_fill(tsp, tsp->ep, sp->lno, P_FILL);

	/* Clear the new information line. */
	MOVE(tsp, INFOLINE(sp), 0);
	clrtoeol();

	/* Repaint the child screen. */
	F_SET(tsp, S_REDRAW);

	/* Switch screens. */
	sp->snext = tsp;
	F_SET(sp, S_SSWITCH);
	return (0);
}
