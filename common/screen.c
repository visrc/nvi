/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: screen.c,v 5.3 1993/04/06 11:36:29 bostic Exp $ (Berkeley) $Date: 1993/04/06 11:36:29 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "tag.h"

static int	 cut_copy __P((SCR *, SCR *));
static int	 opt_copy __P((SCR *, SCR *));
static int	 seq_copy __P((SCR *, SCR *));
static int	 tag_copy __P((SCR *, SCR *));

/*
 * scr_init --
 *	Do the default initialization of an SCR structure.
 */
int
scr_init(orig, sp)
	SCR *orig, *sp;
{
	extern CHNAME asciiname[];		/* XXX */
	int nore;

/* INITIALIZED AT SCREEN CREATE. */
	memset(sp, 0, sizeof(SCR));

	if (orig != NULL)
		HDR_APPEND(sp, orig, next, prev, SCR);

	sp->olno = OOBLNO;
	sp->lno = 1;
	sp->cno = sp->ocno = 0;

#ifdef FWOPEN_NOT_AVAILABLE
	sp->trapped_fd = -1;
#endif

	FD_ZERO(&sp->rdfd);

	sp->lastcmd = &cmds[C_PRINT];

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	if (orig != NULL) {
		sp->gp = orig->gp;
		
		/* User can replay the last input, but nothing else. */
		sp->ib.start.lno = sp->ib.stop.lno = OOBLNO;
		if (orig->ib.replen != 0)
			if ((sp->ib.rep = malloc(orig->ib.replen)) == NULL)
				goto mem;
			else {
				memmove(sp->ib.rep,
				    orig->ib.rep, orig->ib.replen);
				sp->ib.replen = orig->ib.replen;
			}

		if (orig->VB != NULL && (sp->VB = strdup(orig->VB)) == NULL)
			goto mem;
		
		if (orig->lastbcomm != NULL &&
		    (sp->lastbcomm = strdup(orig->lastbcomm)) == NULL)
			goto mem;

		sp->inc_lastch = orig->inc_lastch;
		sp->inc_lastval = orig->inc_lastval;

		if (cut_copy(orig, sp))

		tag_copy(orig, sp);
			
		/* Retain all searching/substitution information. */
		if (orig->searchdir == NOTSET)
			sp->searchdir = NOTSET;
		else {
			sp->sre = orig->sre;
			sp->searchdir = FORWARD;
		}
		sp->csearchdir = CNOTSET;
		sp->lastckey = orig->lastckey;

		nore = 0;
		if (orig->matchsize && (sp->match =
		    malloc(orig->matchsize * sizeof(regmatch_t))) == NULL)
			goto mem;
		else {
			if (sp->matchsize = orig->matchsize)
				memmove(sp->match, orig->match,
				    orig->matchsize * sizeof(regmatch_t));
		}
		if (sp->repl_len &&
		    (sp->repl = malloc(orig->repl_len)) == NULL)
			goto mem;
		else {
			if (sp->repl_len = orig->repl_len)
				memmove(sp->repl, orig->repl, orig->repl_len);
		}
		if (sp->newl_len && (sp->newl =
		    malloc(orig->newl_len * sizeof(size_t))) == NULL) {
mem:			msgq(orig, M_ERROR,
			    "new screen attributes: %s", strerror(errno));
			scr_end(sp);
			return (1);
		} else {
			sp->newl_len = orig->newl_len;
			if (sp->newl_cnt = orig->newl_cnt)
				memmove(sp->newl, orig->newl,
				    orig->newl_len * sizeof(size_t));
		}
		if (!nore && F_ISSET(orig, S_RE_SET))
			F_SET(sp, S_RE_SET);

		sp->cname = orig->cname;
		memmove(sp->special, orig->special, sizeof(sp->special));

		seq_copy(orig, sp);

		opt_copy(orig, sp);

		sp->flags = orig->flags & S_SCREEN_RETAIN;
	} else {
		if (isatty(STDIN_FILENO))
			F_SET(sp, S_ISFROMTTY);

		sp->inc_lastch = '+';
		sp->inc_lastval = 1;

		sp->searchdir = NOTSET;
		sp->csearchdir = CNOTSET;

		sp->cname = asciiname;			/* XXX */
	}

	return (0);
}

/*
 * scr_end --
 *	Release a screen.
 */
int
scr_end(sp)
	SCR *sp;
{
	/* Free the memory map. */
	if (sp->h_smap != NULL)
		free(sp->h_smap);

	/* Free the argument list. */
	{ int cnt;
		for (cnt = 0; cnt < sp->argscnt; ++cnt)
			if (F_ISSET(&sp->args[cnt], A_ALLOCATED))
				free(sp->args[cnt].bp);
		free(sp->args);
		free(sp->argv);
	}

	/* Free globbing information. */
	if (sp->g.gl_pathc)
		globfree(&sp->g);

	/* Free input character arrays. */
	if (sp->gb_cb != NULL) {
		free(sp->gb_cb);
		free(sp->gb_qb);
		free(sp->gb_wb);
	}

	/* Free line input buffer. */
	if (sp->ibp != NULL)
		free(sp->ibp);

	/* Free visual bell termcap string. */
	if (sp->VB != NULL)
		free(sp->VB);

	/* Free last bang command. */
	if (sp->lastbcomm != NULL)
		free(sp->lastbcomm);

	/* Free cut buffers. */
	{ CB *p; int cnt;
		for (p = sp->cuts, cnt = 0; cnt < UCHAR_MAX; ++p, ++cnt)
			if (p->head != NULL)
				text_free(p->head);
	}

	/* Free up tag information. */
	{ int cnt;
		if (F_ISSET(&sp->opts[O_TAGS], OPT_ALLOCATED) &&
		    sp->tfhead != NULL) {
			for (cnt = 0; sp->tfhead[cnt] != NULL; ++cnt)
				free(sp->tfhead[cnt]->fname);
			free(sp->tfhead);
		}
		if (sp->tlast != NULL)
			free(sp->tlast);
	}

	/* Free up search information. */
	if (sp->match != NULL)
		free(sp->match);
	if (sp->repl != NULL)
		free(sp->repl);
	if (sp->newl != NULL)
		free(sp->newl);

	/* Free up linked lists of sequences. */
	{ SEQ *qp, *next;
		for (qp = sp->seqhdr.next;
		    qp != (SEQ *)&sp->seqhdr; qp = next) {
			next = qp->next;
			free(qp->name);
			free(qp->output);
			free(qp->input);
			free(qp);
		}
	}

	/* Free up executed buffer. *
	if (sp->atkey_buf)
		free(sp->atkey_buf);

	/*
	 * Free the message chain last, so previous failures have a place
	 * to put messages.  If there are any messages remaining, copy
	 * them to (in order) a related screen, any screen, the global area. 
	 */
	{ SCR *c_sp; MSG *c_mp, *mp, *next;
		if (sp->parent != NULL) {
			c_sp = sp->parent;
			c_mp = c_sp->msgp;
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(c_sp, S_BELLSCHED);
		} else if (sp->child != NULL) {
			c_sp = sp->child;
			c_mp = c_sp->msgp;
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(c_sp, S_BELLSCHED);
		} else if (sp->next != (SCR *)&sp->gp->scrhdr) {
			c_sp = sp->next;
			c_mp = c_sp->msgp;
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(c_sp, S_BELLSCHED);
		} else {
			c_sp = NULL;
			c_mp = sp->gp->msgp;
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(sp->gp, S_BELLSCHED);
		}

		for (mp = sp->msgp;
		    mp != NULL && !F_ISSET(mp, M_EMPTY); mp = mp->next)
			msga(sp->gp, c_sp, mp->flags, mp->mbuf, mp->len);

		for (mp = sp->msgp; mp != NULL; mp = next) {
			next = mp->next;
			if (mp->mbuf != NULL)
				free(mp->mbuf);
			free(mp);
		}
	}

	/* Remove the screen from the global chain of screens. */
	HDR_DELETE(sp, next, prev);

	/* Remove the screen from the chain of window screens. */
	if (sp->parent != NULL) {
		sp->parent->child = sp->child;
		if (sp->child != NULL)
			sp->child->parent = sp->parent;
	} else if (sp->child != NULL)
		sp->child->parent = NULL;

	/* Free the screen itself. */
	free(sp);

	return (0);
}

/*
 * cut_copy --
 *	Copy a screen's cut buffers.
 */
static int
cut_copy(a, b)
	SCR *a, *b;
{
	CB *cbp;
	int cnt;

	memmove(b->cuts, a->cuts, sizeof(a->cuts));

	for (cbp = b->cuts, cnt = 0; cnt < UCHAR_MAX; ++cbp, ++cnt)
		if (cbp->head != NULL)
			cbp->head = text_copy(a, cbp->head);
	return (0);
}

/*
 * opt_copy --
 *	Copy a screen's OPTION array.
 */
static int
opt_copy(a, b)
	SCR *a, *b;
{
	OPTION *op;
	int cnt;

	/* Copy most everything without change. */
	memmove(b->opts, a->opts, sizeof(a->opts));

	/*
	 * Allocate copies of the strings -- keep trying to reallocate
	 * after ENOMEM failure, otherwise end up with more than one
	 * screen referencing the original memory.
	 */
	for (op = b->opts, cnt = 0; cnt < O_OPTIONCOUNT; ++cnt, ++op)
		if (F_ISSET(&b->opts[cnt], OPT_ALLOCATED) &&
		    (O_STR(b, cnt) = strdup(O_STR(b, cnt))) == NULL) {
			msgq(a, M_ERROR,
			    "Error: option copy: %s", strerror(errno));
			return (1);
		}
	return (0);
}

/*
 * seq_copy --
 *	Copy a screen's SEQ structures.
 */
static int
seq_copy(a, b)
	SCR *a, *b;
{
	SEQ *ap;

	/* Initialize linked list. */
	HDR_INIT(b->seqhdr, next, prev, SEQ);

	for (ap = a->seqhdr.next; ap != (SEQ *)&a->seqhdr; ap = ap->next)
		if (seq_set(b,
		    ap->name, ap->input, ap->output, ap->stype,
		    F_ISSET(ap, S_USERDEF)))
			return (1);
	return (0);
}

/*
 * tag_copy --
 *	Copy a screen's tag structures.
 */
static int
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
	if (a->tlast != NULL && (b->tlast = strdup(a->tlast)) == NULL)
		goto nomem;
	return (0);

nomem:	msgq(a, M_ERROR, "Error: tag copy: %s", strerror(errno));
	return (1);
}
