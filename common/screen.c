/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: screen.c,v 5.2 1993/04/05 07:12:38 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:12:38 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "tag.h"

static void	 cut_copy __P((SCR *, SCR *));
static void	 seq_copy __P((SCR *, SCR *));
static void	 tag_copy __P((SCR *, SCR *));

/*
 * scr_def --
 *	Do the default initialization of an SCR structure.
 */
int
scr_def(orig, sp)
	SCR *orig, *sp;
{
	extern CHNAME asciiname[];		/* XXX */
	GS *gp;
	int nomem;

/* INITIALIZED AT SCREEN CREATE. */
	memset(sp, 0, sizeof(SCR));

	if (orig != NULL)
		HDR_APPEND(sp, orig, next, prev, SCR);

	sp->olno = OOBLNO;
	sp->lno = 1;
	sp->cno = sp->ocno = 0;

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	nomem = 0;
	if (orig != NULL) {
		sp->gp = orig->gp;

		/* User can replay the last input, but nothing else. */
		sp->ib.start.lno = sp->ib.stop.lno = OOBLNO;
		if (orig->ib.replen != 0)
			if ((sp->ib.rep = malloc(orig->ib.replen)) == NULL)
				nomem = 1;
			else {
				memmove(sp->ib.rep,
				    orig->ib.rep, orig->ib.replen);
				sp->ib.replen = orig->ib.replen;
			}

		if (orig->VB != NULL &&
		    (sp->VB = strdup(orig->VB)) == NULL)
			nomem = 1;
		
		if (orig->lastbcomm != NULL &&
		    (sp->lastbcomm = strdup(orig->lastbcomm)) == NULL)
			nomem = 1;

		sp->inc_lastch = orig->inc_lastch;
		sp->inc_lastval = orig->inc_lastval;

		cut_copy(orig, sp);

		tag_copy(orig, sp);
			
		if (orig->searchdir == NOTSET)
			sp->searchdir = NOTSET;
		else {
			sp->sre = orig->sre;
			sp->searchdir = FORWARD;
		}
		sp->csearchdir = CNOTSET;

		sp->cname = orig->cname;
		memmove(sp->special, orig->special, sizeof(sp->special));

		seq_copy(orig, sp);

		sp->flags = orig->flags & S_SCREEN_RETAIN;
	} else {
		sp->inc_lastch = '+';
		sp->inc_lastval = 1;

		sp->searchdir = NOTSET;
		sp->csearchdir = CNOTSET;

		sp->cname = asciiname;			/* XXX */
	}

	if (nomem) {
		msgq(sp, M_ERROR, "new screen attributes: %s", strerror(errno));
		return (1);
	}
	return (0);
}

/*
 * cut_copy --
 *	Copy a screen's cut buffers.
 */
static void
cut_copy(a, b)
	SCR *a, *b;
{
	CB *cbp;
	int cnt;

	memmove(b->cuts, a->cuts, sizeof(a->cuts));

	for (cbp = b->cuts, cnt = 0; cnt < UCHAR_MAX; ++cbp, ++cnt)
		if (cbp->head != NULL)
			cbp->head = text_copy(a, cbp->head);
}

/*
 * tag_copy --
 *	Copy a screen's tag structures.
 */
static void
tag_copy(a, b)
	SCR *a, *b;
{
	TAGF **atfp, **btfp;
	TAG *atp, *btp, *tp;
	int cnt;

	/* Copy the tag stack. */
	for (atp = a->thead; atp != NULL; atp = atp->prev) {
		if ((tp = malloc(sizeof(TAG))) == NULL)
			goto nomem;
		if ((tp->tag = strdup(atp->tag)) == NULL) {
			free(tp);
			goto nomem;
		}
		if ((tp->fname = strdup(atp->fname)) == NULL) {
			free(tp);
			free(tp->tag);
			goto nomem;
		}
		if ((tp->line = strdup(atp->line)) == NULL) {
			free(tp);
			free(tp->tag);
			free(tp->fname);
			goto nomem;
		}
		tp->prev = NULL;
		if (b->thead == NULL)
			btp = b->thead = tp;
		else {
			btp->prev = tp;
			btp = tp;
		}
	}

	/* Copy the list of tag files. */
	for (atfp = a->tfhead, cnt = 1; *atfp != NULL; ++atfp, ++cnt);

	if (cnt > 1) {
		if ((b->tfhead = malloc(cnt * sizeof(TAGF **))) == NULL)
			goto nomem;
		for (atfp = a->tfhead,
		    btfp = b->tfhead; *atfp != NULL; ++atfp, ++btfp) {
			if ((*btfp = malloc(sizeof(TAGF))) == NULL)
				goto nomem;
			if (((*btfp)->fname = strdup((*atfp)->fname)) == NULL) {
				free(*btfp);
				*btfp = NULL;
				goto nomem;
			}
			(*btfp)->flags = 0;
		}
	}
		
	/* Copy the last tag. */
	if (a->tlast != NULL)
		b->tlast = strdup(a->tlast);
	return;

nomem:	msgq(a, M_ERROR, "Error: tag copy: %s", strerror(errno));
}

/*
 * seq_copy --
 *	Copy a screen's SEQ structures.
 */
static void
seq_copy(a, b)
	SCR *a, *b;
{
	SEQ *ap;

	/* Initialize linked list. */
	HDR_INIT(b->seqhdr, next, prev, SEQ);

	for (ap = a->seqhdr.next; ap != (SEQ *)&a->seqhdr; ap = ap->next)
		(void)seq_set(b,
		    ap->name, ap->input, ap->output, ap->stype,
		    F_ISSET(ap, S_USERDEF));
}
