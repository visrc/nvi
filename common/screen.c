/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: screen.c,v 8.34 1993/11/17 10:21:05 bostic Exp $ (Berkeley) $Date: 1993/11/17 10:21:05 $";
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

/*
 * screen_init --
 *	Do the default initialization of an SCR structure.
 */
int
screen_init(orig, spp, flags)
	SCR *orig, **spp;
	u_int flags;
{
	SCR *sp;
	extern CHNAME const asciiname[];	/* XXX */
	size_t len;

	if ((sp = malloc(sizeof(SCR))) == NULL) {
		msgq(NULL, M_SYSERR, NULL);
		return (1);
	}
	memset(sp, 0, sizeof(SCR));

/* INITIALIZED AT SCREEN CREATE. */
	queue_init(&sp->screenq);

	sp->gp = __global_list;			/* All ref the GS structure. */

	queue_init(&sp->frefq);

	sp->ccnt = 2;				/* Anything > 1 */

	FD_ZERO(&sp->rdfd);

	HDR_INIT(sp->txthdr, next, prev);

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	if (orig == NULL) {
		sp->searchdir = NOTSET;
		sp->csearchdir = CNOTSET;

		sp->cname = asciiname;			/* XXX */

		sp->saved_vi_mode = LF_ISSET(S_VI_CURSES | S_VI_XAW);

		switch (flags & S_SCREENS) {
		case S_EX:
			if (sex_screen_init(sp))
				return (1);
			break;
		case S_VI_CURSES:
			if (svi_screen_init(sp))
				return (1);
			break;
		case S_VI_XAW:
			if (xaw_screen_init(sp))
				return (1);
			break;
		default:
			abort();
		}

		sp->flags = flags;
		list_enter_head(&__global_list->screens, sp, SCR *, screenq);
	} else {
		if (orig->alt_fname != NULL &&
		    (sp->alt_fname = strdup(orig->alt_fname)) == NULL)
			goto mem;

		/* Retain all searching/substitution information. */
		if (F_ISSET(orig, S_SRE_SET)) {
			F_SET(sp, S_SRE_SET);
			sp->sre = orig->sre;
		}
		if (F_ISSET(orig, S_SUBRE_SET)) {
			F_SET(sp, S_SUBRE_SET);
			sp->subre = orig->subre;
		}
		sp->searchdir = orig->searchdir == NOTSET ? NOTSET : FORWARD;
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

		if (opts_copy(orig, sp)) {
mem:			msgq(orig, M_SYSERR, "new screen attributes");
			(void)screen_end(sp);
			return (1);
		}

		sp->s_bell		= orig->s_bell;
		sp->s_bg		= orig->s_bg;
		sp->s_busy		= orig->s_busy;
		sp->s_change		= orig->s_change;
		sp->s_chposition	= orig->s_chposition;
		sp->s_clear		= orig->s_clear;
		sp->s_confirm		= orig->s_confirm;
		sp->s_copy		= orig->s_copy;
		sp->s_down		= orig->s_down;
		sp->s_edit		= orig->s_edit;
		sp->s_end		= orig->s_end;
		sp->s_ex_cmd		= orig->s_ex_cmd;
		sp->s_ex_run		= orig->s_ex_run;
		sp->s_ex_write		= orig->s_ex_write;
		sp->s_fg		= orig->s_fg;
		sp->s_fill		= orig->s_fill;
		sp->s_get		= orig->s_get;
		sp->s_key_read		= orig->s_key_read;
		sp->s_optchange		= orig->s_optchange;
		sp->s_position		= orig->s_position;
		sp->s_refresh		= orig->s_refresh;
		sp->s_relative		= orig->s_relative;
		sp->s_resize		= orig->s_resize;
		sp->s_split		= orig->s_split;
		sp->s_suspend		= orig->s_suspend;
		sp->s_up		= orig->s_up;

		F_SET(sp, F_ISSET(orig, S_SCREENS));
		list_insert_after(&orig->screenq, sp, SCR *, screenq);
	}

	/* Copy screen private information. */
	if (sp->s_copy(orig, sp))
		return (1);

	*spp = sp;
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
	/* Cleanup screen private information. */
	(void)sp->s_end(sp);

	/* Free remembered file names. */
	{ FREF *frp;
		while ((frp = sp->frefq.qe_next) != NULL) {
			queue_remove(&sp->frefq, frp, FREF *, q);
			FREE(frp->fname, frp->nlen);
			FREE(frp, sizeof(FREF));
		}
	}

	/* Free text input chain. */
	hdr_text_free(&sp->txthdr);

	/* Free any script information. */
	if (F_ISSET(sp, S_SCRIPT))
		sscr_end(sp);

	/* Free alternate file name. */
	if (sp->alt_fname != NULL)
		FREE(sp->alt_fname, strlen(sp->alt_fname) + 1);

	/* Free up search information. */
	if (sp->match != NULL)
		FREE(sp->match, sizeof(regmatch_t));
	if (sp->repl != NULL)
		FREE(sp->repl, sp->repl_len);
	if (sp->newl != NULL)
		FREE(sp->newl, sp->newl_len);

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
		} else if (sp->screenq.qe_next != NULL) {
			c_sp = sp->screenq.qe_next;
			c_mp = c_sp->msgp;
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(c_sp, S_BELLSCHED);
		} else {
			c_sp = NULL;
			c_mp = sp->gp->msgp;
			if (F_ISSET(sp, S_BELLSCHED))
				F_SET(sp->gp, G_BELLSCHED);
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

	/* Remove the screen from the global screen chain. */
	list_remove(sp, SCR *, screenq);

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
