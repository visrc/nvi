/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: screen.c,v 10.1 1995/04/13 17:18:30 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:18:30 $";
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
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "../vi/vi.h"
#include "../ex/tag.h"

/*
 * screen_init --
 *	Do the default initialization of an SCR structure.
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

	LIST_INIT(&sp->msgq);

	sp->ccnt = 2;				/* Anything > 1 */

	FD_ZERO(&sp->rdfd);

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
	if (sp->q.cqe_next != NULL) {
		SIGBLOCK(sp->gp);
		CIRCLEQ_REMOVE(&sp->gp->dq, sp, q);
		SIGUNBLOCK(sp->gp);
	}

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

	/* Free any saved input file name. */
	if (sp->if_name != NULL)
		free(sp->if_name);

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

	/*
	 * Clean up the message chain last, so previous failures have a
	 * place to put messages.  Don't free space containing messages,
	 * just move the structures onto the global list.
	 */
	{ MSG *mp, *next;
		if (F_ISSET(sp, S_BELLSCHED))
			F_SET(sp->gp, G_BELLSCHED);

		for (mp = sp->msgq.lh_first; mp != NULL; mp = next) {
			next = mp->q.le_next;
			if (!F_ISSET(mp, M_EMPTY)) {
				LIST_REMOVE(mp, q);
				LIST_INSERT_HEAD(&sp->gp->msgq, mp, q);
			} else {
				if (mp->mbuf != NULL)
					free(mp->mbuf);
				free(mp);
			}
		}
	}

	/* Free the screen itself. */
	FREE(sp, sizeof(SCR));

	return (rval);
}
