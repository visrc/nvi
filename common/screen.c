/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: screen.c,v 10.4 1995/09/21 12:06:17 bostic Exp $ (Berkeley) $Date: 1995/09/21 12:06:17 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "../ex/tag.h"
#include "../vi/vi.h"

/*
 * screen_init --
 *	Do the default initialization of an SCR structure.
 *
 * PUBLIC: int screen_init __P((GS *, SCR *, SCR **));
 */
int
screen_init(gp, orig, spp)
	GS *gp;
	SCR *orig, **spp;
{
	SCR *sp;
	size_t len;

	*spp = NULL;
	CALLOC_RET(orig, sp, SCR *, 1, sizeof(SCR));
	*spp = sp;

/* INITIALIZED AT SCREEN CREATE. */
	sp->refcnt = 1;

	sp->gp = gp;				/* All ref the GS structure. */

	sp->ccnt = 2;				/* Anything > 1 */

	/*
	 * XXX
	 * sp->defscroll is initialized by the opts_init() code because
	 * we don't have the option information yet.
	 */

	CIRCLEQ_INIT(&sp->tiq);

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	if (orig == NULL) {
		sp->searchdir = NOTSET;
	} else {
		if (orig->alt_name != NULL &&
		    (sp->alt_name = strdup(orig->alt_name)) == NULL)
			goto mem;

		/* Retain all searching/substitution information. */
		if (F_ISSET(orig, S_RE_SEARCH)) {
			F_SET(sp, S_RE_SEARCH);
			sp->sre = orig->sre;
		}
		if (F_ISSET(orig, S_RE_SUBST)) {
			F_SET(sp, S_RE_SUBST);
			sp->subre = orig->subre;
		}
		sp->searchdir = orig->searchdir == NOTSET ? NOTSET : FORWARD;

		if (orig->repl_len) {
			MALLOC(sp, sp->repl, char *, orig->repl_len);
			if (sp->repl == NULL)
				goto mem;
			sp->repl_len = orig->repl_len;
			memmove(sp->repl, orig->repl, orig->repl_len);
		}
		if (orig->newl_len) {
			len = orig->newl_len * sizeof(size_t);
			MALLOC(sp, sp->newl, size_t *, len);
			if (sp->newl == NULL) {
mem:				msgq(orig, M_SYSERR, NULL);
				goto err;
			}
			sp->newl_len = orig->newl_len;
			sp->newl_cnt = orig->newl_cnt;
			memmove(sp->newl, orig->newl, len);
		}

		if (F_ISSET(orig, S_AT_SET)) {
			F_SET(sp, S_AT_SET);
			sp->at_lbuf = orig->at_lbuf;
		}

		if (opts_copy(orig, sp))
			goto err;

		F_SET(sp, F_ISSET(orig, S_EX | S_VI));
	}

	if (ex_screen_copy(orig, sp))		/* Ex. */
		goto err;
	if (v_screen_copy(orig, sp))		/* Vi. */
		goto err;

	*spp = sp;
	return (0);

err:	screen_end(sp);
	return (1);
}

/*
 * screen_end --
 *	Release a screen, no matter what had (and had not) been
 *	initialized.
 *
 * PUBLIC: int screen_end __P((SCR *));
 */
int
screen_end(sp)
	SCR *sp;
{
	int rval;

	/* If multiply referenced, just decrement the count and return. */
	 if (--sp->refcnt != 0)
		 return (0);

	/*
	 * Remove the screen from the displayed queue.
	 *
	 * If a created screen failed during initialization, it may not
	 * be linked into the chain.
	 */
	if (sp->q.cqe_next != NULL)
		CIRCLEQ_REMOVE(&sp->gp->dq, sp, q);

	/* No more messages. */
	F_CLR(sp, S_SCREEN_READY);

	rval = 0;
	if (v_screen_end(sp))			/* End vi. */
		rval = 1;
	if (ex_screen_end(sp))			/* End ex. */
		rval = 1;

	/* Free file names. */
	{ char **ap;
		if (!F_ISSET(sp, S_ARGNOFREE) && sp->argv != NULL) {
			for (ap = sp->argv; *ap != NULL; ++ap)
				free(*ap);
			free(sp->argv);
		}
	}

	/* Free any text input. */
	if (sp->tiq.cqh_first != NULL)
		text_lfree(&sp->tiq);

	/* Free any script information. */
	if (F_ISSET(sp, S_SCRIPT))
		sscr_end(sp);

	/* Free alternate file name. */
	if (sp->alt_name != NULL)
		free(sp->alt_name);

	/* Free up search information. */
	if (sp->repl != NULL)
		FREE(sp->repl, sp->repl_len);
	if (sp->newl != NULL)
		FREE(sp->newl, sp->newl_len);

	/* Free all the options */
	opts_free(sp);

	/* Free the screen itself. */
	FREE(sp, sizeof(SCR));

	return (rval);
}

/*
 * screen_next --
 *	Return the next screen in the queue.
 *
 * PUBLIC: SCR *screen_next __P((SCR *));
 */
SCR *
screen_next(sp)
	SCR *sp;
{
	GS *gp;
	SCR *next;

	/* Try the display queue, without returning the current screen. */
	gp = sp->gp;
	for (next = gp->dq.cqh_first;
	    next != (void *)&gp->dq; next = next->q.cqe_next)
		if (next != sp)
			break;
	if (next != (void *)&gp->dq)
		return (next);

	/* Try the hidden queue; if found, move screen to the display queue. */
	if (gp->hq.cqh_first != (void *)&gp->hq) {
		next = gp->hq.cqh_first;
		CIRCLEQ_REMOVE(&gp->hq, next, q);
		CIRCLEQ_INSERT_HEAD(&gp->dq, next, q);
		return (next);
	}
	return (NULL);
}
