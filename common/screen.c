/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: screen.c,v 8.19 1993/10/28 11:20:18 bostic Exp $ (Berkeley) $Date: 1993/10/28 11:20:18 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "vcmd.h"
#include "excmd.h"
#include "tag.h"

static int	 opt_copy __P((SCR *, SCR *));
static int	 seq_copy __P((SCR *, SCR *));
static int	 tag_copy __P((SCR *, SCR *));

/*
 * screen_init --
 *	Do the default initialization of an SCR structure.
 */
int
screen_init(orig, sp)
	SCR *orig, *sp;
{
	extern CHNAME asciiname[];		/* XXX */
	size_t len;
	sigset_t bmask, omask;

/* INITIALIZED AT SCREEN CREATE. */

	/*
	 * NULL the sp->ep field first, so the recovery timer won't access.
	 * Block SIGALRM and SIGHUP when manipulating the SCR chain.
	 */
	memset(sp, 0, sizeof(SCR));
	if (orig != NULL) {
		sigemptyset(&bmask);
		sigaddset(&bmask, SIGALRM);
		sigaddset(&bmask, SIGHUP);
		(void)sigprocmask(SIG_BLOCK, &bmask, &omask);
		HDR_APPEND(sp, orig, next, prev, SCR);
		(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	}

	queue_init(&sp->frefq);

	sp->lno = sp->olno = OOBLNO;
	sp->cno = sp->ocno = 0;

	sp->ccnt = 2;				/* Anything > 1 */

	sp->trapped_fd = -1;

	FD_ZERO(&sp->rdfd);

	HDR_INIT(sp->bhdr, next, prev);
	HDR_INIT(sp->txthdr, next, prev);

	sp->lastcmd = &cmds[C_PRINT];

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	if (orig != NULL) {
		sp->gp = orig->gp;
		
		/* User can replay the last input, but nothing else. */
		if (orig->rep_len != 0)
			if ((sp->rep = malloc(orig->rep_len)) == NULL)
				goto mem;
			else {
				memmove(sp->rep, orig->rep, orig->rep_len);
				sp->rep_len = orig->rep_len;
			}

		if (orig->lastbcomm != NULL &&
		    (sp->lastbcomm = strdup(orig->lastbcomm)) == NULL)
			goto mem;

		if (orig->alt_fname != NULL &&
		    (sp->alt_fname = strdup(orig->alt_fname)) == NULL)
			goto mem;

		sp->inc_lastch = orig->inc_lastch;
		sp->inc_lastval = orig->inc_lastval;

		/*
		 * XXX
		 * dot, dotmotion not copied.
		 */

		if (orig->paragraph != NULL &&
		    (sp->paragraph = strdup(orig->paragraph)) == NULL)
			goto mem;
	
		if (tag_copy(orig, sp))
			goto mem;
			
		/* Retain all searching/substitution information. */
		if (orig->searchdir == NOTSET)
			sp->searchdir = NOTSET;
		else {
			sp->sre = orig->sre;
			sp->searchdir = FORWARD;
		}
		sp->csearchdir = CNOTSET;
		sp->lastckey = orig->lastckey;

		if (orig->matchsize) {
			len = orig->matchsize * sizeof(regmatch_t);
			if ((sp->match = malloc(len)) == NULL)
				goto mem;
			sp->matchsize = orig->matchsize;
			memmove(sp->match, orig->match, len);
		}
		if (orig->repl_len) {
			if ((sp->repl = malloc(orig->repl_len)) == NULL)
				goto mem;
			sp->repl_len = orig->repl_len;
			memmove(sp->repl, orig->repl, orig->repl_len);
		}
		if (orig->newl_len) {
			len = orig->newl_len * sizeof(size_t);
			if ((sp->newl = malloc(len)) == NULL)
				goto mem;
			sp->newl_len = orig->newl_len;
			sp->newl_cnt = orig->newl_cnt;
			memmove(sp->newl, orig->newl, len);
		}

		sp->cname = orig->cname;
		memmove(sp->special, orig->special, sizeof(sp->special));

		if (seq_copy(orig, sp))
			goto mem;

		sp->at_lbuf = orig->at_lbuf;

		if (opt_copy(orig, sp)) {
mem:			msgq(orig, M_ERR,
			    "new screen attributes: %s", strerror(errno));
			(void)screen_end(sp);
			return (1);
		}

		F_SET(sp, (orig->flags & S_SCREEN_RETAIN));
	} else {
		sp->inc_lastch = '+';
		sp->inc_lastval = 1;

		queue_init(&sp->tagq);
		queue_init(&sp->tagfq);

		sp->searchdir = NOTSET;
		sp->csearchdir = CNOTSET;

		sp->cname = asciiname;			/* XXX */

		sp->at_lbuf = OOBCB;

		sp->flags = 0;
	}

	return (0);
}

/*
 * screen_end --
 *	Release a screen.
 */
int
screen_end(sp)
	SCR *sp;
{
	sigset_t bmask, omask;

	/* Free remembered file names. */
	{ FREF *frp;
		while ((frp = sp->frefq.qe_next) != NULL) {
			queue_remove(&sp->frefq, frp, FREF *, q);
			FREE(frp->fname, frp->nlen);
			FREE(frp, sizeof(FREF));
		}
	}

	/* Free the argument list. */
	(void)free_argv(sp);

	/* Free line input buffer. */
	if (sp->ibp != NULL)
		FREE(sp->ibp, sp->ibp_len);

	/* Free text input, command chains. */
	hdr_text_free(&sp->txthdr);
	hdr_text_free(&sp->bhdr);

	/* Free any script information. */
	if (F_ISSET(sp, S_SCRIPT))
		sscr_end(sp);

	/* Free vi text input memory. */
	if (sp->rep != NULL)
		FREE(sp->rep, sp->rep_len);

	/* Free last bang command. */
	if (sp->lastbcomm != NULL)
		FREE(sp->lastbcomm, strlen(sp->lastbcomm) + 1);

	/* Free alternate file name. */
	if (sp->alt_fname != NULL)
		FREE(sp->alt_fname, strlen(sp->alt_fname) + 1);

	/* Free paragraph search list. */
	if (sp->paragraph != NULL)
		FREE(sp->paragraph, strlen(sp->paragraph) + 1);

	/* Free up tag information. */
	{ TAG *tp;
		while ((tp = sp->tagq.qe_next) != NULL) {
			queue_remove(&sp->tagq, tp, TAG *, q);
			FREE(tp, sizeof(TAG));
		}
	}
	{ TAGF *tp;
		while ((tp = sp->tagfq.qe_next) != NULL) {
			queue_remove(&sp->tagfq, tp, TAGF *, q);
			FREE(tp->fname, strlen(tp->fname) + 1);
			FREE(tp, sizeof(TAG));
		}
	}
	if (sp->tlast != NULL)
		FREE(sp->tlast, strlen(sp->tlast) + 1);

	/* Free up search information. */
	if (sp->match != NULL)
		FREE(sp->match, sizeof(regmatch_t));
	if (sp->repl != NULL)
		FREE(sp->repl, sp->repl_len);
	if (sp->newl != NULL)
		FREE(sp->newl, sp->newl_len);

	/* Free up linked lists of sequences. */
	{ SEQ *qp, *next;
		for (qp = sp->seqhdr.next;
		    qp != (SEQ *)&sp->seqhdr; qp = next) {
			next = qp->next;
			if (qp->name != NULL)
				FREE(qp->name, strlen(qp->name) + 1);
			FREE(qp->output, strlen(qp->output) + 1);
			FREE(qp->input, strlen(qp->input) + 1);
			FREE(qp, sizeof(SEQ));
		}
	}

	/* Free all the options */
	opts_free(sp);

	/*
	 * Free the message chain last, so previous failures have a place
	 * to put messages.  Copy messages to (in order) a related screen,
	 * any screen, the global area. 
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
			msg_app(sp->gp, c_sp,
			    mp->flags & M_INV_VIDEO, mp->mbuf, mp->len);

		for (mp = sp->msgp; mp != NULL; mp = next) {
			next = mp->next;
			if (mp->mbuf != NULL)
				FREE(mp->mbuf, mp->blen);
			FREE(mp, sizeof(MSG));
		}
	}

	/*
	 * Remove the screen from the global chain of screens.
	 * Block SIGALRM and SIGHUP when manipulating the SCR chain.
	 */
	sigemptyset(&bmask);
	sigaddset(&bmask, SIGALRM);
	sigaddset(&bmask, SIGHUP);
	(void)sigprocmask(SIG_BLOCK, &bmask, &omask);
	HDR_DELETE(sp, next, prev, SCR);
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);

	/* Remove the screen from the chain of related screens. */
	if (sp->parent != NULL) {
		sp->parent->child = sp->child;
		if (sp->child != NULL)
			sp->child->parent = sp->parent;
	} else if (sp->child != NULL)
		sp->child->parent = NULL;

	/* Free the screen itself. */
	FREE(sp, sizeof(SCR));

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
			msgq(a, M_ERR,
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
	HDR_INIT(b->seqhdr, next, prev);

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
	TAG *ap, *tp;
	TAGF *atfp, *tfp;
	int cnt;

	/* Initialize queues. */
	queue_init(&b->tagq);
	queue_init(&b->tagfq);

	/* Copy tag stack. */
	for (ap = a->tagq.qe_next; ap != NULL; ap = ap->q.qe_next) {
		if ((tp = malloc(sizeof(TAG))) == NULL)
			goto nomem;
		*tp = *ap;
		queue_enter_tail(&b->tagq, tp, TAG *, q);
	}

	/* Copy list of tag files. */
	for (atfp = a->tagfq.qe_next; atfp != NULL; atfp = atfp->q.qe_next) {
		if ((tfp = malloc(sizeof(TAGF))) == NULL)
			goto nomem;
		*tfp = *atfp;
		if ((tfp->fname = strdup(atfp->fname)) == NULL)
			goto nomem;
		queue_enter_tail(&b->tagfq, tfp, TAGF *, q);
	}
		
	/* Copy the last tag. */
	if (a->tlast != NULL && (b->tlast = strdup(a->tlast)) == NULL) {
nomem:		msgq(a, M_ERR, "Error: tag copy: %s", strerror(errno));
		return (1);
	}
	return (0);
}
