/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems, Inc.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ex_tag.c,v 10.21 1996/04/09 09:43:01 bostic Exp $ (Berkeley) $Date: 1996/04/09 09:43:01 $";
#endif /* not lint */

#include <sys/param.h>
/*
 * XXX
 * AIX 3.2.5, AIX 4.1 <sys/param.h> doesn't include <sys/types.h>.
 * Idiots.
 */
#include <sys/types.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "../vi/vi.h"
#include "tag.h"

static char	*binary_search __P((char *, char *, char *));
static int	 compare __P((char *, char *, char *));
static int	 ctag_get __P((SCR *, char *,
		    char **, size_t *, char **, size_t *, char **, size_t *));
static int	 ctag_search __P((SCR *, char *, char *));
static char	*linear_search __P((char *, char *, char *));
static int	 search __P((SCR *, TAGF *, char *, char **));
static int	 tag_copy __P((SCR *, TAG *, TAG **));
static int	 tag_pop __P((SCR *, TAGQ *, int));
static int	 tagf_copy __P((SCR *, TAGF *, TAGF **));
static int	 tagf_free __P((SCR *, TAGF *));
static int	 tagq_copy __P((SCR *, TAGQ *, TAGQ **));
static int	 tagq_free __P((SCR *, TAGQ *));

enum tagmsg {TAG_BADLNO, TAG_EMPTY, TAG_SEARCH};
static void	 tag_msg __P((SCR *, enum tagmsg, char *));

/* XXX */
int
cscope_search(sp, tp)
	SCR *sp;
	TAG *tp;
{
	return (0);
}

/*
 * ex_ct_first --
 *	The tag code can be entered from main, e.g., "vi -t tag".
 *
 * PUBLIC: int ex_ct_first __P((SCR *, char *));
 */
int
ex_ct_first(sp, tagarg)
	SCR *sp;
	char *tagarg;
{
	EX_PRIVATE *exp;
	FREF *frp;
	long tl;
	size_t notused;
	char *tag, *name, *search;

	exp = EXP(sp);

	/* Taglength may limit the number of characters. */
	if ((tl = O_VAL(sp, O_TAGLENGTH)) != 0 && strlen(tagarg) > tl)
		tagarg[tl] = '\0';

	/* Get the tag information. */
	if (ctag_get(sp,
	    tagarg, &tag, &notused, &name, &notused, &search, &notused))
		return (0);

	/* Create the file entry. */
	if ((frp = file_add(sp, name)) == NULL)
		return (1);
	if (file_init(sp, frp, NULL, 0))
		return (1);

	/* Search the file for the tag. */
	(void)ctag_search(sp, search, tag);
	free(tag);

	/* Might as well make this the default tag. */
	if ((exp->tag_last = strdup(tagarg)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}
	F_SET(sp, S_SCR_CENTER);
	return (0);
}

/*
 * ex_ct_push -- ^]
 *		 :tag[!] [string]
 *
 * Enter a new TAGQ context based on a ctag string.
 *
 * PUBLIC: int ex_ct_push __P((SCR *, EXCMD *));
 */
int
ex_ct_push(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	EX_PRIVATE *exp;
	FREF *frp;
	TAG *rtp, *tp;
	TAGQ *rtqp, *tqp;
	size_t nlen, slen, tlen;
	long tl;
	int force, rval;
	char *name, *search, *tag;

	exp = EXP(sp);
	switch (cmdp->argc) {
	case 1:
		if (exp->tag_last != NULL)
			free(exp->tag_last);
		if ((exp->tag_last = strdup(cmdp->argv[0]->bp)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}
		break;
	case 0:
		if (exp->tag_last == NULL) {
			msgq(sp, M_ERR, "158|No previous tag entered");
			return (1);
		}
		break;
	default:
		abort();
	}

	/* Taglength may limit the number of characters. */
	if ((tl = O_VAL(sp, O_TAGLENGTH)) != 0 && strlen(exp->tag_last) > tl)
		exp->tag_last[tl] = '\0';

	/* Get the tag information. */
	if (ctag_get(sp, exp->tag_last,
	    &tag, &tlen, &name, &nlen, &search, &slen))
		return (1);

	/*
	 * If we're in a temporary file, we don't have a context to which
	 * we can return, so don't bother setting up a tags stack.  This
	 * doesn't apply if we're creating a new screen, however.
	 */
	force = FL_ISSET(cmdp->iflags, E_C_FORCE);
	if (F_ISSET(sp->frp, FR_TMPFILE) && !F_ISSET(cmdp, E_NEWSCREEN)) {
		rval = ex_tag_switch(sp, name, NULL, search, tag, force);
		free(tag);
		return (rval);
	}

	/* Allocate all necessary memory before swapping screens. */
	rtp = tp = NULL;
	rtqp = tqp = NULL;
	if (exp->tq.cqh_first == (void *)&exp->tq) {
		CALLOC_GOTO(sp, rtqp, TAGQ *, 1, sizeof(TAGQ));
		CALLOC_GOTO(sp, rtp, TAG *, 1, sizeof(TAG));
	}
	CALLOC_GOTO(sp, tqp, TAGQ *, 1, sizeof(TAGQ));
	CALLOC_GOTO(sp, tp,
	    TAG *, 1, sizeof(TAG) + nlen + 1 + slen + 1 + tlen + 1);

	/*
	 * Stick the current context in a convenient place, we're about
	 * to lose it.
	 */
	tp->frp = sp->frp;
	tp->lno = sp->lno;
	tp->cno = sp->cno;

	/* Try to switch to the tag. */
	if (F_ISSET(cmdp, E_NEWSCREEN)) {
		if (ex_tag_Nswitch(sp, name, &frp, search, tag, force))
			goto err;

		/* Everything else gets done in the new screen. */
		sp = sp->nextdisp;
		exp = EXP(sp);
	} else
		if (ex_tag_switch(sp, name, &frp, search, tag, force))
			goto err;

	/*
	 * If this is the first tag, put a `current location' queue entry
	 * in place, so we can pop all the way back to the current mark.
	 */
	if (exp->tq.cqh_first == (void *)&exp->tq) {
		/* Initialize and link the tag queue structure. */
		CIRCLEQ_INSERT_HEAD(&exp->tq, rtqp, q);
		CIRCLEQ_INIT(&rtqp->tagq);
		rtqp->current = rtp;

		/* Initialize and link the tag structure. */
		CIRCLEQ_INSERT_HEAD(&rtqp->tagq, rtp, q);
	} else
		rtp = exp->tq.cqh_first->current;

	/* Save the current context, including the file pointer. */
	rtp->frp = tp->frp;
	rtp->lno = tp->lno;
	rtp->cno = tp->cno;

	/* Initialize and link the tag queue structure. */
	CIRCLEQ_INSERT_HEAD(&exp->tq, tqp, q);
	CIRCLEQ_INIT(&tqp->tagq);
	tqp->current = tp;

	/* Initialize and link the tag structure. */
	tp->frp = frp;
	tp->fname = tp->buf;
	memcpy(tp->fname, name, (tp->fnlen = nlen) + 1);
	tp->search = tp->fname + nlen + 1;
	memcpy(tp->search, search, (tp->slen = slen) + 1);
	tp->tag = tp->search + slen + 1;
	memcpy(tp->tag, tag, (tp->tlen = tlen) + 1);
	CIRCLEQ_INSERT_HEAD(&tqp->tagq, tp, q);

	free(tag);
	return (0);

err:
alloc_err:
	if (tag != NULL)
		free(tag);
	if (rtqp != NULL)
		free(rtqp);
	if (rtp != NULL)
		free(rtp);
	if (tqp != NULL)
		free(tqp);
	if (tp != NULL)
		free(tp);
	return (1);
}

/*
 * ex_tag_switch --
 *	Switch context to a TAG.
 *
 * PUBLIC: int ex_tag_switch __P((SCR *, char *, FREF **, char *, char *, int));
 */
int
ex_tag_switch(sp, fname, frpp, search, tag, force)
	SCR *sp;
	char *fname, *search, *tag;
	FREF **frpp;
	int force;
{
	FREF *frp;

	/*
	 * Get the (possibly new) FREF structure, and return it to the
	 * caller.
	 */
	if ((frp = file_add(sp, fname)) == NULL)
		return (1);
	if (frpp != NULL)
		*frpp = frp;

	/* If not changing files, do the search and return. */
	if (frp == sp->frp)
		return (ctag_search(sp, search, tag));

	/* Check for permission to leave. */
	if (file_m1(sp, force, FS_ALL | FS_POSSIBLE))
		return (1);

	/* Initialize the new file. */
	if (file_init(sp, frp, NULL, FS_SETALT))
		return (1);

	/*
	 * Search for the tag.  Unlike searching within the current file,
	 * failure to find the tag isn't an error, which matches historical
	 * practice, i.e., we always enter the new file.
	 */
	(void)ctag_search(sp, search, tag);

	/* Switch. */
	F_SET(sp, S_FSWITCH | S_SCR_CENTER);

	return (0);
}

/*
 * ex_tag_Nswitch --
 *	Switch context to a TAG in a new screen.
 *
 * PUBLIC: int ex_tag_Nswitch
 * PUBLIC:    __P((SCR *, char *, FREF **, char *, char *, int));
 */
int
ex_tag_Nswitch(sp, fname, frpp, search, tag, force)
	SCR *sp;
	char *fname, *search, *tag;
	FREF **frpp;
	int force;
{
	FREF *frp;
	SCR *new;

	/*
	 * Get the (possibly new) FREF structure, and return it to the
	 * caller.
	 */
	if ((frp = file_add(sp, fname)) == NULL)
		return (1);
	if (frpp != NULL)
		*frpp = frp;

	/* Get a new screen. */
	if (screen_init(sp->gp, sp, &new))
		return (1);
	if (vs_split(sp, new, 0)) {
		(void)file_end(new, new->ep, 1);
		(void)screen_end(new);
		return (1);
	}

	/* Get a backing file. */
	if (frp == sp->frp) {
		/* Copy file state. */
		new->ep = sp->ep;
		++new->ep->refcnt;

		new->frp = frp;
		new->frp->flags = sp->frp->flags;
	} else if (file_init(new, frp, NULL, force)) {
		(void)vs_discard(new, NULL);
		(void)screen_end(new);
		return (1);
	}

	/*
	 * Search for the tag.  Unlike searching within the current file,
	 * failure to find the tag isn't an error, which matches historical
	 * practice, i.e., we always enter the new file.
	 */
	(void)ctag_search(new, search, tag);

	/* Switch. */
	sp->nextdisp = new;
	F_SET(sp, S_SSWITCH);

	return (0);
}

/*
 * ex_tag_pop -- ^T
 *		 :tagp[op][!] [number | file]
 *
 *	Pop to a previous TAGQ context.
 *
 * PUBLIC: int ex_tag_pop __P((SCR *, EXCMD *));
 */
int
ex_tag_pop(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	EX_PRIVATE *exp;
	TAGQ *tqp, *dtqp;
	size_t arglen;
	long off;
	char *arg, *p, *t;

	/* Check for an empty stack. */
	exp = EXP(sp);
	if (exp->tq.cqh_first == (void *)&exp->tq) {
		tag_msg(sp, TAG_EMPTY, NULL);
		return (1);
	}

	/* Find the last TAG structure that we're going to DISCARD! */
	switch (cmdp->argc) {
	case 0:				/* Pop one tag. */
		dtqp = exp->tq.cqh_first;
		break;
	case 1:				/* Name or number. */
		arg = cmdp->argv[0]->bp;
		off = strtol(arg, &p, 10);
		if (*p != '\0')
			goto filearg;

		/* Number: pop that many queue entries. */
		if (off < 1)
			return (0);
		for (tqp = exp->tq.cqh_first;
		    tqp != (void *)&exp->tq && --off > 1;
		    tqp = tqp->q.cqe_next);
		if (tqp == (void *)&exp->tq) {
			msgq(sp, M_ERR,
	"159|Less than %s entries on the tags stack; use :display t[ags]",
			    arg);
			return (1);
		}
		dtqp = tqp;
		break;

		/* File argument: pop to that queue entry. */
filearg:	arglen = strlen(arg);
		for (tqp = exp->tq.cqh_first;
		    tqp != (void *)&exp->tq;
		    dtqp = tqp, tqp = tqp->q.cqe_next) {
			/* Don't pop to the current file. */
			if (tqp == exp->tq.cqh_first)
				continue;
			p = tqp->current->frp->name;
			if ((t = strrchr(p, '/')) == NULL)
				t = p;
			else
				++t;
			if (!strncmp(arg, t, arglen))
				break;
		}
		if (tqp == (void *)&exp->tq) {
			msgq_str(sp, M_ERR, arg,
	"160|No file %s on the tags stack to return to; use :display t[ags]");
			return (1);
		}
		if (tqp == exp->tq.cqh_first)
			return (0);
		break;
	default:
		abort();
	}

	return (tag_pop(sp, dtqp, FL_ISSET(cmdp->iflags, E_C_FORCE)));
}

/*
 * ex_tag_top -- :tagt[op][!]
 *	Clear the tag stack.
 *
 * PUBLIC: int ex_tag_top __P((SCR *, EXCMD *));
 */
int
ex_tag_top(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	EX_PRIVATE *exp;

	exp = EXP(sp);

	/* Check for an empty stack. */
	if (exp->tq.cqh_first == (void *)&exp->tq) {
		tag_msg(sp, TAG_EMPTY, NULL);
		return (1);
	}

	/* Return to the oldest information. */
	return (tag_pop(sp,
	    exp->tq.cqh_last->q.cqe_prev, FL_ISSET(cmdp->iflags, E_C_FORCE)));
}

/*
 * tag_pop --
 *	Pop up to and including the specified TAGQ context.
 */
static int
tag_pop(sp, dtqp, force)
	SCR *sp;
	TAGQ *dtqp;
	int force;
{
	EX_PRIVATE *exp;
	TAG *tp;
	TAGQ *tqp;

	exp = EXP(sp);

	/*
	 * Update the cursor from the saved TAG information of the TAG
	 * structure we're moving to.
	 */
	tp = dtqp->q.cqe_next->current;
	if (tp->frp == sp->frp) {
		sp->lno = tp->lno;
		sp->cno = tp->cno;
	} else {
		if (file_m1(sp, force, FS_ALL | FS_POSSIBLE))
			return (1);

		tp->frp->lno = tp->lno;
		tp->frp->cno = tp->cno;
		F_SET(sp->frp, FR_CURSORSET);
		if (file_init(sp, tp->frp, NULL, FS_SETALT))
			return (1);

		F_SET(sp, S_FSWITCH);
	}

	/* Pop entries off the queue up to and including dtqp. */
	do {
		tqp = exp->tq.cqh_first;
		if (tagq_free(sp, tqp))
			return (0);
	} while (tqp != dtqp);

	/*
	 * If only a single tag left, we've returned to the first tag point,
	 * and the stack is now empty.
	 */
	if (exp->tq.cqh_first->q.cqe_next == (void *)&exp->tq)
		tagq_free(sp, exp->tq.cqh_first);

	return (0);
}

/*
 * ex_tagdisplay --
 *	Display the list of tags.
 *
 * PUBLIC: int ex_tagdisplay __P((SCR *));
 */
int
ex_tagdisplay(sp)
	SCR *sp;
{
	EX_PRIVATE *exp;
	TAG *tp;
	TAGQ *tqp;
	int cnt, nf;
	size_t len;
	char *p, *t;

	exp = EXP(sp);
	if ((tqp = exp->tq.cqh_first) == (void *)&exp->tq) {
		tag_msg(sp, TAG_EMPTY, NULL);
		return (0);
	}

	/*
	 * We give the file name 20 columns and the search string the rest.
	 * If there's not enough room, we don't do anything special, it's
	 * not worth the effort, it just makes the display more confusing.
	 */
#define	L_NAME		20
#define	L_SEARCH	20
#define	L_SPACE		 5
	if (sp->cols <= L_NAME) {
		msgq(sp, M_ERR, "309|Display too small.");
		return (0);
	}
	for (cnt = 1, tqp = exp->tq.cqh_first;
	    tqp != (void *)&exp->tq; ++cnt, tqp = tqp->q.cqe_next) {
		if (INTERRUPTED(sp))
			break;

		tp = tqp->current;
		len = strlen(p = msg_print(sp, tp->frp->name, &nf));
		if (len > L_NAME) {
			t = p + (len - L_NAME);
			t[0] = '.';
			t[1] = '.';
			t[2] = '.';
			t[3] = ' ';
		} else
			t = p;
		(void)ex_printf(sp, "%2d %*.*s", cnt, L_NAME, L_NAME, t);
		if (nf)
			FREE_SPACE(sp, p, 0);
		if (tp->search != NULL &&
		    (sp->cols - L_NAME) >= L_SEARCH + L_SPACE) {
			len = strlen(p = msg_print(sp, tp->search, &nf));
			if (len > sp->cols - (L_NAME + L_SPACE))
				len = sp->cols - (L_NAME + L_SPACE);
			(void)ex_printf(sp, "     %.*s", (int)len, tp->search);
			if (nf)
				FREE_SPACE(sp, p, 0);
		}
		(void)ex_printf(sp, "\n");
	}
	return (0);
}

/*
 * ex_tagcopy --
 *	Copy a screen's tag structures.
 *
 * PUBLIC: int ex_tagcopy __P((SCR *, SCR *));
 */
int
ex_tagcopy(orig, sp)
	SCR *orig, *sp;
{
	EX_PRIVATE *oexp, *nexp;
	TAGQ *aqp, *tqp;
	TAG *ap, *tp;
	TAGF *atfp, *tfp;

	oexp = EXP(orig);
	nexp = EXP(sp);

	/* Copy tag queue and tags stack. */
	for (aqp = oexp->tq.cqh_first;
	    aqp != (void *)&oexp->tq; aqp = aqp->q.cqe_next) {
		if (tagq_copy(sp, aqp, &tqp))
			return (1);
		for (ap = aqp->tagq.cqh_first;
		    ap != (void *)&aqp->tagq; ap = ap->q.cqe_next) {
			if (tag_copy(sp, ap, &tp))
				return (1);
			/* Set the current pointer. */
			if (aqp->current == ap)
				tqp->current = tp;
			CIRCLEQ_INSERT_TAIL(&tqp->tagq, tp, q);
		}
		CIRCLEQ_INSERT_TAIL(&nexp->tq, tqp, q);
	}

	/* Copy list of tag files. */
	for (atfp = oexp->tagfq.tqh_first;
	    atfp != NULL; atfp = atfp->q.tqe_next) {
		if (tagf_copy(sp, atfp, &tfp))
			return (1);
		TAILQ_INSERT_TAIL(&nexp->tagfq, tfp, q);
	}

	/* Copy the last tag. */
	if (oexp->tag_last != NULL &&
	    (nexp->tag_last = strdup(oexp->tag_last)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}
	return (0);
}

/*
 * tagf_copy --
 *	Copy a TAGF structure and return it in new memory.
 */
static int
tagf_copy(sp, otfp, tfpp)
	SCR *sp;
	TAGF *otfp, **tfpp;
{
	TAGF *tfp;

	MALLOC_RET(sp, tfp, TAGF *, sizeof(TAGF));
	*tfp = *otfp;

	/* XXX: Allocate as part of the TAGF structure!!! */
	if ((tfp->name = strdup(otfp->name)) == NULL)
		return (1);

	*tfpp = tfp;
	return (0);
}

/*
 * tagq_copy --
 *	Copy a TAGQ structure and return it in new memory.
 */
static int
tagq_copy(sp, otqp, tqpp)
	SCR *sp;
	TAGQ *otqp, **tqpp;
{
	TAGQ *tqp;

	MALLOC_RET(sp, tqp, TAGQ *, sizeof(TAGQ));
	*tqp = *otqp;

	CIRCLEQ_INIT(&tqp->tagq);
	tqp->current = NULL;

	*tqpp = tqp;
	return (0);
}

/*
 * tag_copy --
 *	Copy a TAG structure and return it in new memory.
 */
static int
tag_copy(sp, otp, tpp)
	SCR *sp;
	TAG *otp, **tpp;
{
	TAG *tp;
	size_t len;

	len = sizeof(TAG);
	if (otp->fname != NULL)
		len += otp->fnlen + 1;
	if (otp->search != NULL)
		len += otp->slen + 1;
	if (otp->tag != NULL)
		len += otp->tlen + 1;
	MALLOC_RET(sp, tp, TAG *, len);
	memcpy(tp, otp, len);

	if (otp->fname != NULL)
		tp->fname = tp->buf;
	if (otp->search != NULL)
		tp->search = tp->fname + otp->fnlen + 1;
	if (otp->tag != NULL)
		tp->tag = tp->search + otp->slen + 1;

	*tpp = tp;
	return (0);
}

/*
 * tag_msg
 *	A few common messages.
 */
static void
tag_msg(sp, msg, tag)
	SCR *sp;
	enum tagmsg msg;
	char *tag;
{
	switch (msg) {
	case TAG_BADLNO:
		msgq_str(sp, M_ERR, tag, "164|%s: the tag line doesn't exist");
		break;
	case TAG_EMPTY:
		msgq(sp, M_INFO, "165|The tags stack is empty");
		break;
	case TAG_SEARCH:
		msgq_str(sp, M_ERR, tag, "166|%s: search pattern not found");
		break;
	default:
		abort();
	}
}

/*
 * tagq_free --
 *	Free a TAGQ structure (and associated TAG structures).
 *
 * PUBLIC: int tagq_free __P((SCR *, TAGQ *));
 */
static int
tagq_free(sp, tqp)
	SCR *sp;
	TAGQ *tqp;
{
	EX_PRIVATE *exp;
	TAG *tp;

	exp = EXP(sp);
	while ((tp = tqp->tagq.cqh_first) != (void *)&tqp->tagq) {
		CIRCLEQ_REMOVE(&tqp->tagq, tp, q);
		free(tp);
	}
	CIRCLEQ_REMOVE(&exp->tq, tqp, q);
	free(tqp);
	return (0);
}

/*
 * tagf_free --
 *	Free a TAGF structure.
 */
static int
tagf_free(sp, tfp)
	SCR *sp;
	TAGF *tfp;
{
	EX_PRIVATE *exp;

	exp = EXP(sp);
	TAILQ_REMOVE(&exp->tagfq, tfp, q);
	free(tfp->name);
	free(tfp);
	return (0);
}

/*
 * ex_ctf_alloc --
 *	Create a new list of ctag files.
 *
 * PUBLIC: int ex_ctf_alloc __P((SCR *, char *));
 */
int
ex_ctf_alloc(sp, str)
	SCR *sp;
	char *str;
{
	EX_PRIVATE *exp;
	TAGF *tfp;
	size_t len;
	char *p, *t;

	/* Free current queue. */
	exp = EXP(sp);
	while ((tfp = exp->tagfq.tqh_first) != NULL)
		tagf_free(sp, tfp);

	/* Create new queue. */
	for (p = t = str;; ++p) {
		if (*p == '\0' || isblank(*p)) {
			if ((len = p - t) > 1) {
				MALLOC_RET(sp, tfp, TAGF *, sizeof(TAGF));
				MALLOC(sp, tfp->name, char *, len + 1);
				if (tfp->name == NULL) {
					free(tfp);
					return (1);
				}
				memmove(tfp->name, t, len);
				tfp->name[len] = '\0';
				tfp->flags = 0;
				TAILQ_INSERT_TAIL(&exp->tagfq, tfp, q);
			}
			t = p + 1;
		}
		if (*p == '\0')
			 break;
	}
	return (0);
}
						/* Free previous queue. */
/*
 * ex_tagfree --
 *	Free the ex tag information.
 *
 * PUBLIC: int ex_tagfree __P((SCR *));
 */
int
ex_tagfree(sp)
	SCR *sp;
{
	EX_PRIVATE *exp;
	TAGF *tfp;
	TAGQ *tqp;

	/* Free up tag information. */
	exp = EXP(sp);
	while ((tqp = exp->tq.cqh_first) != (void *)&exp->tq)
		tagq_free(sp, tqp);
	while ((tfp = exp->tagfq.tqh_first) != NULL)
		tagf_free(sp, tfp);
	if (exp->tag_last != NULL)
		free(exp->tag_last);
	return (0);
}

/*
 * ctag_search --
 *	Search a file for a tag.
 */
static int
ctag_search(sp, search, tag)
	SCR *sp;
	char *search, *tag;
{
	MARK m;
	char *p;

	/*
	 * !!!
	 * The historic tags file format (from a long, long time ago...)
	 * used a line number, not a search string.  I got complaints, so
	 * people are still using the format.
	 */
	if (isdigit(search[0])) {
		m.lno = atoi(search);
		if (!db_exist(sp, m.lno)) {
			tag_msg(sp, TAG_BADLNO, tag);
			return (1);
		}
	} else {
		/*
		 * Search for the tag; cheap fallback for C functions
		 * if the name is the same but the arguments have changed.
		 */
		m.lno = 1;
		m.cno = 0;
		if (f_search(sp, &m, &m,
		    search, NULL, SEARCH_FILE | SEARCH_TAG))
			if ((p = strrchr(search, '(')) != NULL) {
				p[1] = '\0';
				if (f_search(sp, &m, &m,
				    search, NULL, SEARCH_FILE | SEARCH_TAG)) {
					p[1] = '(';
					goto notfound;
				}
				p[1] = '(';
			} else {
notfound:			tag_msg(sp, TAG_SEARCH, tag);
				return (1);
			}
		/*
		 * !!!
		 * Historically, tags set the search direction if it wasn't
		 * already set.
		 */
		if (sp->searchdir == NOTSET)
			sp->searchdir = FORWARD;
	}

	/*
	 * !!!
	 * Tags move to the first non-blank, NOT the search pattern start.
	 */
	sp->lno = m.lno;
	sp->cno = 0;
	(void)nonblank(sp, sp->lno, &sp->cno);
	return (0);
}

/*
 * ctag_get --
 *	Get a tag from the tags files.
 */
static int
ctag_get(sp, tag, tagp, taglenp, filep, filelenp, searchp, searchlenp)
	SCR *sp;
	char *tag, **tagp, **filep, **searchp;
	size_t *taglenp, *filelenp, *searchlenp;
{
	struct stat sb;
	EX_PRIVATE *exp;
	TAGF *tfp;
	size_t len;
	int echk, nf1, nf2;
	char *p, *t, pbuf[MAXPATHLEN];

	/*
	 * Find the tag, only display missing file messages once, and
	 * then only if we didn't find the tag.
	 */
	echk = 0;
	exp = EXP(sp);
	for (p = NULL, tfp = exp->tagfq.tqh_first;
	    tfp != NULL && p == NULL; tfp = tfp->q.tqe_next)
		if (search(sp, tfp, tag, &p)) {
			F_SET(tfp, TAGF_ERR);
			echk = 1;
		} else {
			F_CLR(tfp, TAGF_ERR | TAGF_ERR_WARN);
			if (p != NULL)
				break;
		}

	if (p == NULL) {
		msgq_str(sp, M_ERR, tag, "162|%s: tag not found");
		if (echk)
			for (tfp = exp->tagfq.tqh_first;
			    tfp != NULL; tfp = tfp->q.tqe_next)
				if (F_ISSET(tfp, TAGF_ERR) &&
				    !F_ISSET(tfp, TAGF_ERR_WARN)) {
					errno = tfp->errno;
					msgq_str(sp, M_SYSERR, tfp->name, "%s");
					F_SET(tfp, TAGF_ERR_WARN);
				}
		return (1);
	}

	/*
	 * Set the return pointers; tagp points to the tag, and, incidentally
	 * the allocated string, filep points to the file name, and searchp
	 * points to the search string.  All three are nul-terminated.
	 */
	for (*tagp = p; *p && !isblank(*p); ++p);
	if (*p == '\0')
		goto malformed;
	*taglenp = p - *tagp;
	for (*p++ = '\0'; isblank(*p); ++p);
	for (*filep = p; *p && !isblank(*p); ++p);
	if (*p == '\0')
		goto malformed;
	*filelenp = p - *filep;
	for (*p++ = '\0'; isblank(*p); ++p);
	*searchp = p;
	if (*p == '\0') {
malformed:	free(*tagp);
		p = msg_print(sp, tag, &nf1);
		t = msg_print(sp, tfp->name, &nf2);
		msgq(sp, M_ERR, "163|%s: corrupted tag in %s", p, t);
		if (nf1)
			FREE_SPACE(sp, p, 0);
		if (nf2)
			FREE_SPACE(sp, t, 0);
		return (1);
	}
	*searchlenp = strlen(p);

	/*
	 * !!!
	 * If the tag file path is a relative path, see if it exists.  If it
	 * doesn't, look relative to the tags file path.  It's okay for a tag
	 * file to not exist, and, historically, vi simply displayed a "new"
	 * file.  However, if the path exists relative to the tag file, it's
	 * pretty clear what's happening, so we may as well do it right.
	 */
	if ((*filep)[0] != '/'
	    && stat(*filep, &sb) && (p = strrchr(tfp->name, '/')) != NULL) {
		*p = '\0';
		len = snprintf(pbuf, sizeof(pbuf), "%s/%s", tfp->name, *filep);
		*p = '/';
		if (stat(pbuf, &sb) == 0) {
			MALLOC(sp, p,
			    char *, len + *searchlenp + *taglenp + 5);
			if (p != NULL) {
				memmove(p, *tagp, *taglenp);
				free(*tagp);
				*tagp = p;
				*(p += *taglenp) = '\0';
				memmove(++p, pbuf, len);
				*filep = p;
				*filelenp = len;
				*(p += len) = '\0';
				memmove(++p, *searchp, *searchlenp);
				*searchp = p;
				*(p += *searchlenp) = '\0';
			}
		}
	}
	return (0);
}

#define	EQUAL		0
#define	GREATER		1
#define	LESS		(-1)

/*
 * search --
 *	Search a file for a tag.
 */
static int
search(sp, tfp, tname, tag)
	SCR *sp;
	TAGF *tfp;
	char *tname, **tag;
{
	struct stat sb;
	int fd, len;
	char *endp, *back, *front, *map, *p;

	if ((fd = open(tfp->name, O_RDONLY, 0)) < 0) {
		tfp->errno = errno;
		return (1);
	}

	/*
	 * XXX
	 * Some old BSD systems require MAP_FILE as an argument when mapping
	 * regular files.
	 */
#ifndef MAP_FILE
#define	MAP_FILE	0
#endif
	/*
	 * XXX
	 * We'd like to test if the file is too big to mmap.  Since we don't
	 * know what size or type off_t's or size_t's are, what the largest
	 * unsigned integral type is, or what random insanity the local C
	 * compiler will perpetrate, doing the comparison in a portable way
	 * is flatly impossible.  Hope that malloc fails if the file is too
	 * large.
	 */
	if (fstat(fd, &sb) || (map = mmap(NULL, (size_t)sb.st_size,
	    PROT_READ, MAP_FILE | MAP_PRIVATE, fd, (off_t)0)) == (caddr_t)-1) {
		tfp->errno = errno;
		(void)close(fd);
		return (1);
	}
	front = map;
	back = front + sb.st_size;

	front = binary_search(tname, front, back);
	front = linear_search(tname, front, back);

	if (front == NULL || (endp = strchr(front, '\n')) == NULL) {
		*tag = NULL;
		goto done;
	}

	len = endp - front;
	MALLOC(sp, p, char *, len + 1);
	if (p == NULL) {
		*tag = NULL;
		goto done;
	}
	memmove(p, front, len);
	p[len] = '\0';
	*tag = p;

done:	if (munmap(map, (size_t)sb.st_size))
		msgq(sp, M_SYSERR, "munmap");
	if (close(fd))
		msgq(sp, M_SYSERR, "close");
	return (0);
}

/*
 * Binary search for "string" in memory between "front" and "back".
 *
 * This routine is expected to return a pointer to the start of a line at
 * *or before* the first word matching "string".  Relaxing the constraint
 * this way simplifies the algorithm.
 *
 * Invariants:
 * 	front points to the beginning of a line at or before the first
 *	matching string.
 *
 * 	back points to the beginning of a line at or after the first
 *	matching line.
 *
 * Base of the Invariants.
 * 	front = NULL;
 *	back = EOF;
 *
 * Advancing the Invariants:
 *
 * 	p = first newline after halfway point from front to back.
 *
 * 	If the string at "p" is not greater than the string to match,
 *	p is the new front.  Otherwise it is the new back.
 *
 * Termination:
 *
 * 	The definition of the routine allows it return at any point,
 *	since front is always at or before the line to print.
 *
 * 	In fact, it returns when the chosen "p" equals "back".  This
 *	implies that there exists a string is least half as long as
 *	(back - front), which in turn implies that a linear search will
 *	be no more expensive than the cost of simply printing a string or two.
 *
 * 	Trying to continue with binary search at this point would be
 *	more trouble than it's worth.
 */
#define	SKIP_PAST_NEWLINE(p, back)	while (p < back && *p++ != '\n');

static char *
binary_search(string, front, back)
	register char *string, *front, *back;
{
	register char *p;

	p = front + (back - front) / 2;
	SKIP_PAST_NEWLINE(p, back);

	while (p != back) {
		if (compare(string, p, back) == GREATER)
			front = p;
		else
			back = p;
		p = front + (back - front) / 2;
		SKIP_PAST_NEWLINE(p, back);
	}
	return (front);
}

/*
 * Find the first line that starts with string, linearly searching from front
 * to back.
 *
 * Return NULL for no such line.
 *
 * This routine assumes:
 *
 * 	o front points at the first character in a line.
 *	o front is before or at the first line to be printed.
 */
static char *
linear_search(string, front, back)
	char *string, *front, *back;
{
	while (front < back) {
		switch (compare(string, front, back)) {
		case EQUAL:		/* Found it. */
			return (front);
		case LESS:		/* No such string. */
			return (NULL);
		case GREATER:		/* Keep going. */
			break;
		}
		SKIP_PAST_NEWLINE(front, back);
	}
	return (NULL);
}

/*
 * Return LESS, GREATER, or EQUAL depending on how the string1 compares
 * with string2 (s1 ??? s2).
 *
 * 	o Matches up to len(s1) are EQUAL.
 *	o Matches up to len(s2) are GREATER.
 *
 * The string "s1" is null terminated.  The string s2 is '\t', space, (or
 * "back") terminated.
 *
 * !!!
 * Reasonably modern ctags programs use tabs as separators, not spaces.
 * However, historic programs did use spaces, and, I got complaints.
 */
static int
compare(s1, s2, back)
	register char *s1, *s2, *back;
{
	for (; *s1 && s2 < back && (*s2 != '\t' && *s2 != ' '); ++s1, ++s2)
		if (*s1 != *s2)
			return (*s1 < *s2 ? LESS : GREATER);
	return (*s1 ? GREATER : s2 < back &&
	    (*s2 != '\t' && *s2 != ' ') ? LESS : EQUAL);
}
