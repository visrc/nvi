/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems, Inc.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_tag.c,v 10.4 1995/06/09 12:51:55 bostic Exp $ (Berkeley) $Date: 1995/06/09 12:51:55 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "vi.h"
#include "tag.h"

static char	*binary_search __P((char *, char *, char *));
static int	 compare __P((char *, char *, char *));
static int	 ex_N_tagpush __P((SCR *, EXCMD *, FREF *, char *, char *));
static char	*linear_search __P((char *, char *, char *));
static int	 search __P((SCR *, TAGF *, char *, char **));
static int	 tag_get __P((SCR *, char *, char **, char **, char **));
static int	 tag_search __P((SCR *, char *, char *));

enum tagmsg {TAG_BADLNO, TAG_EMPTY, TAG_SEARCH};
static void	 tag_msg __P((SCR *, enum tagmsg, char *));

/*
 * ex_tagfirst --
 *	The tag code can be entered from main, i.e. "vi -t tag".
 *
 * PUBLIC: int ex_tagfirst __P((SCR *, char *));
 */
int
ex_tagfirst(sp, tagarg)
	SCR *sp;
	char *tagarg;
{
	FREF *frp;
	long tl;
	char *tag, *name, *search;

	/* Taglength may limit the number of characters. */
	if ((tl = O_VAL(sp, O_TAGLENGTH)) != 0 && strlen(tagarg) > tl)
		tagarg[tl] = '\0';

	/* Get the tag information. */
	if (tag_get(sp, tagarg, &tag, &name, &search))
		return (0);

	/* Create the file entry. */
	if ((frp = file_add(sp, name)) == NULL)
		return (1);
	if (file_init(sp, frp, NULL, 0))
		return (1);

	/* Search the file for the tag. */
	(void)tag_search(sp, search, tag);
	free(tag);

	/* Might as well make this the default tag. */
	if ((EXP(sp)->tlast = strdup(tagarg)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}
	F_SET(sp, S_SCR_CENTER);
	return (0);
}

/* Free a tag or tagf structure from a queue. */
#define	FREETAG(tp) {							\
	TAILQ_REMOVE(&exp->tagq, (tp), q);				\
	if ((tp)->search != NULL)					\
		free((tp)->search);					\
	FREE((tp), sizeof(TAGF));					\
}
#define	FREETAGF(tfp) {							\
	TAILQ_REMOVE(&exp->tagfq, (tfp), q);				\
	free((tfp)->name);						\
	FREE((tfp), sizeof(TAGF));					\
}

/*
 * ex_tagpush -- :tag [file]
 *	Move to a new tag.
 *
 * The tags stacks in nvi are a bit tricky.  Each tag contains a file name,
 * search string, and line/column numbers.  The search string is only used
 * for the first access and for user display.  The first record on the stack
 * is the place where we first did a tag, so it has no search string.  The
 * second record is the first tag, and so on.  Note, this means that the
 * "current" tag is always on the stack.  Each tag has a line/column which is
 * the location from which the user tagged the following TAG entry, and which
 * is used as the return location.
 *
 * PUBLIC: int ex_tagpush __P((SCR *, EXCMD *));
 */
int
ex_tagpush(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	enum {TC_CHANGE, TC_CURRENT} which;
	EX_PRIVATE *exp;
	FREF *frp;
	TAG *tp;
	int sval;
	long tl;
	char *name, *search, *tag;

	exp = EXP(sp);
	switch (cmdp->argc) {
	case 1:
		if (exp->tlast != NULL)
			FREE(exp->tlast, strlen(exp->tlast) + 1);
		if ((exp->tlast = strdup(cmdp->argv[0]->bp)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}
		break;
	case 0:
		if (exp->tlast == NULL) {
			msgq(sp, M_ERR, "158|No previous tag entered");
			return (1);
		}
		break;
	default:
		abort();
	}

	/* Taglength may limit the number of characters. */
	if ((tl = O_VAL(sp, O_TAGLENGTH)) != 0 && strlen(exp->tlast) > tl)
		exp->tlast[tl] = '\0';

	/* Get the tag information. */
	if (tag_get(sp, exp->tlast, &tag, &name, &search))
		return (1);

	/* Get the (possibly new) FREF structure. */
	if ((frp = file_add(sp, name)) == NULL)
		goto err;

	/* New screen now follows a different path. */
	if (F_ISSET(cmdp, E_NEWSCREEN))
		return (ex_N_tagpush(sp, cmdp, frp, search, tag));

	if (sp->frp == frp)
		which = TC_CURRENT;
	else {
		if (file_m1(sp,
		    FL_ISSET(cmdp->iflags, E_C_FORCE), FS_ALL | FS_POSSIBLE))
			goto err;
		which = TC_CHANGE;
	}

	/*
	 * Get a tag structure -- if this is the first tag, push it on the
	 * stack as a placeholder and get another tag structure.  Set the
	 * line/column of the most recent element on the stack to be the
	 * current values, including the file pointer.  Then push the new
	 * TAG onto the stack with the new file and search string for user
	 * display.
	 */
	CALLOC(sp, tp, TAG *, 1, sizeof(TAG));
	if (tp != NULL && exp->tagq.tqh_first == NULL) {
		TAILQ_INSERT_HEAD(&exp->tagq, tp, q);
		CALLOC(sp, tp, TAG *, 1, sizeof(TAG));
	}
	if (exp->tagq.tqh_first != NULL) {
		exp->tagq.tqh_first->frp = sp->frp;
		exp->tagq.tqh_first->lno = sp->lno;
		exp->tagq.tqh_first->cno = sp->cno;
	}
	if (tp != NULL) {
		if ((tp->search = strdup(search)) == NULL)
			msgq(sp, M_SYSERR, NULL);
		else
			tp->slen = strlen(search);
		tp->frp = frp;
		TAILQ_INSERT_HEAD(&exp->tagq, tp, q);
	}

	/* Switch files. */
	if (which == TC_CHANGE && file_init(sp, frp, NULL, FS_SETALT)) {
		if (tp != NULL)
			FREETAG(tp);
		/* Handle special, first-tag case. */
		if (exp->tagq.tqh_first->q.tqe_next == NULL)
			TAILQ_REMOVE(&exp->tagq, exp->tagq.tqh_first, q);
err:		free(tag);
		return (1);
	}

	sval = tag_search(sp, search, tag);
	free(tag);

	switch (which) {
	case TC_CURRENT:
		if (sval)
			return (1);
		break;
	case TC_CHANGE:
		break;
	}
	F_SET(sp, S_SCR_CENTER);
	return (0);
}

/*
 * ex_N_tagpush --
 *	ex_tagpush for new screens.
 */
static int
ex_N_tagpush(sp, cmdp, frp, search, tag)
	SCR *sp;
	EXCMD *cmdp;
	FREF *frp;
	char *search, *tag;
{
	SCR *new;

	/* Get a new screen. */
	if (screen_init(sp->gp, sp, &new))
		return (1);
	if (vs_split(sp, new)) {
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
	} else if (file_init(new, frp, NULL,
	    (FL_ISSET(cmdp->iflags, E_C_FORCE) ? FS_FORCE : 0))) {
		(void)vs_discard(new, NULL);
		(void)screen_end(new);
		return (1);
	}

	/* Search for the tag. */
	(void)tag_search(new, search, tag);
	free(tag);

	/* Set up the switch. */
	sp->nextdisp = new;
	F_SET(sp, S_SSWITCH);

	return (0);
}

/*
 * ex_tagpop -- :tagp[op][!] [number | file]
 *	Pop the tag stack.
 *
 * PUBLIC: int ex_tagpop __P((SCR *, EXCMD *));
 */
int
ex_tagpop(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	EX_PRIVATE *exp;
	TAG *ntp, *tp;
	size_t arglen;
	long off;
	int nf;
	char *arg, *p, *t;

	/* Check for an empty stack. */
	exp = EXP(sp);
	if (exp->tagq.tqh_first == NULL) {
		tag_msg(sp, TAG_EMPTY, NULL);
		return (1);
	}

	switch (cmdp->argc) {
	case 0:				/* Pop one tag. */
		ntp = exp->tagq.tqh_first;
		break;
	case 1:				/* Name or number. */
		arg = cmdp->argv[0]->bp;
		off = strtol(arg, &p, 10);
		if (*p == '\0') {
			if (off < 1)
				return (0);
			for (tp = exp->tagq.tqh_first;
			    tp != NULL && --off > 1; tp = tp->q.tqe_next);
			if (tp == NULL) {
				msgq(sp, M_ERR,
"159|Less than %s entries on the tags stack; use :display t[ags]",
				    arg);
				return (1);
			}
			ntp = tp;
		} else {
			arglen = strlen(arg);
			for (tp = exp->tagq.tqh_first;
			    tp != NULL; ntp = tp, tp = tp->q.tqe_next) {
				/* Use the user's original file name. */
				p = tp->frp->name;
				if ((t = strrchr(p, '/')) == NULL)
					t = p;
				else
					++t;
				if (!strncmp(arg, t, arglen))
					break;
			}
			if (tp == NULL) {
				p = msg_print(sp, arg, &nf);
				msgq(sp, M_ERR,
"160|No file named %s on the tags stack; use :display t[ags]",
				    p);
				if (nf)
					FREE_SPACE(sp, p, 0);
				return (1);
			}
		}
		break;
	default:
		abort();
	}

	/* Update the cursor from the saved TAG information. */
	tp = ntp->q.tqe_next;
	if (tp->frp == sp->frp) {
		sp->lno = tp->lno;
		sp->cno = tp->cno;
	} else {
		if (file_m1(sp,
		    FL_ISSET(cmdp->iflags, E_C_FORCE), FS_ALL | FS_POSSIBLE))
			return (1);
		tp->frp->lno = tp->lno;
		tp->frp->cno = tp->cno;
		F_SET(sp->frp, FR_CURSORSET);
		if (file_init(sp, tp->frp, NULL, FS_SETALT))
			return (1);
	}

	/* Pop entries off the queue up to ntp. */
	for (;;) {
		tp = exp->tagq.tqh_first;
		FREETAG(tp);
		if (tp == ntp)
			break;
	}

	/* If returning to the first tag, the stack is now empty. */
	if (exp->tagq.tqh_first->q.tqe_next == NULL)
		TAILQ_REMOVE(&exp->tagq, exp->tagq.tqh_first, q);
	return (0);
}

/*
 * ex_tagtop -- :tagt[op][!]
 *	Clear the tag stack.
 *
 * PUBLIC: int ex_tagtop __P((SCR *, EXCMD *));
 */
int
ex_tagtop(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	EX_PRIVATE *exp;
	TAG *tp;

	/* Find oldest saved information. */
	exp = EXP(sp);
	for (tp = exp->tagq.tqh_first;
	    tp != NULL && tp->q.tqe_next != NULL; tp = tp->q.tqe_next);
	if (tp == NULL) {
		tag_msg(sp, TAG_EMPTY, NULL);
		return (1);
	}

	/* If not switching files, it's easy; else do the work. */
	if (tp->frp == sp->frp) {
		sp->lno = tp->lno;
		sp->cno = tp->cno;
	} else {
		if (file_m1(sp,
		    FL_ISSET(cmdp->iflags, E_C_FORCE), FS_ALL | FS_POSSIBLE))
			return (1);
		tp->frp->lno = tp->lno;
		tp->frp->cno = tp->cno;
		F_SET(sp->frp, FR_CURSORSET);
		if (file_init(sp, tp->frp, NULL, FS_SETALT))
			return (1);
	}

	/* Empty out the queue. */
	while ((tp = exp->tagq.tqh_first) != NULL)
		FREETAG(tp);
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
	size_t len, maxlen;
	int cnt;
	char *name;

	exp = EXP(sp);
	if ((tp = exp->tagq.tqh_first) == NULL) {
		msgq(sp, M_INFO, "161|No tags to display");
		return (0);
	}

	/*
	 * Figure out the formatting.  MNOC is the maximum
	 * number of file name columns before we split the line.
	 */
#define	MNOC	15
	for (maxlen = 0,
	    tp = exp->tagq.tqh_first; tp != NULL; tp = tp->q.tqe_next) {
		len = strlen(name = tp->frp->name);	/* The original name. */
		if (maxlen < len && len < MNOC)
			maxlen = len;
	}

	for (cnt = 1, tp = exp->tagq.tqh_first;
	    tp != NULL && !INTERRUPTED(sp); ++cnt, tp = tp->q.tqe_next) {
		len = strlen(name = tp->frp->name);	/* The original name. */
		if (len > maxlen || len + tp->slen > sp->cols)
			if (tp == NULL || tp->search == NULL)
				(void)ex_printf(sp, "%2d %s\n", cnt, name);
			else
				(void)ex_printf(sp,
				     "%2d %s\n** %*.*s %s\n", cnt, name,
				     (int)maxlen, (int)maxlen, "", tp->search);
		else
			if (tp == NULL || tp->search == NULL)
				(void)ex_printf(sp, "%2d %*.*s\n",
				    cnt, (int)maxlen, (int)len, name);
			else
				(void)ex_printf(sp, "%2d %*.*s %s\n",
				    cnt, (int)maxlen, (int)len, name,
				    tp->search);
	}
	return (0);
}

/*
 * tag_search --
 *	Search a file for a tag.
 */
static int
tag_search(sp, search, tag)
	SCR *sp;
	char *search, *tag;
{
	MARK m;
	int notused;
	char *p;

	/*
	 * !!!
	 * The historic tags file format (from a long, long time ago...)
	 * used a line number, not a search string.  I got complaints, so
	 * people are still using the format.
	 */
	if (isdigit(search[0])) {
		m.lno = atoi(search);
		if (!file_eline(sp, m.lno)) {
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
			goto notfound;
		if ((p = strrchr(search, '(')) != NULL) {
			p[1] = '\0';
			if (f_search(sp, &m, &m,
			    search, NULL, SEARCH_FILE | SEARCH_TAG)) {
				p[1] = '(';
				goto notfound;
			}
			p[1] = '(';
		} else {
notfound:		tag_msg(sp, TAG_SEARCH, tag);
			return (1);
		}
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
 * ex_tagalloc --
 *	Create a new list of tag files.
 *
 * PUBLIC: int ex_tagalloc __P((SCR *, char *));
 */
int
ex_tagalloc(sp, str)
	SCR *sp;
	char *str;
{
	EX_PRIVATE *exp;
	TAGF *tp;
	size_t len;
	char *p, *t;

	/* Free current queue. */
	exp = EXP(sp);
	while ((tp = exp->tagfq.tqh_first) != NULL)
		FREETAGF(tp);

	/* Create new queue. */
	for (p = t = str;; ++p) {
		if (*p == '\0' || isblank(*p)) {
			if ((len = p - t) > 1) {
				MALLOC_RET(sp, tp, TAGF *, sizeof(TAGF));
				MALLOC(sp, tp->name, char *, len + 1);
				if (tp->name == NULL) {
					FREE(tp, sizeof(TAGF));
					return (1);
				}
				memmove(tp->name, t, len);
				tp->name[len] = '\0';
				tp->flags = 0;
				TAILQ_INSERT_TAIL(&exp->tagfq, tp, q);
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
 *	Free the tags file list.
 *
 * PUBLIC: int ex_tagfree __P((SCR *));
 */
int
ex_tagfree(sp)
	SCR *sp;
{
	EX_PRIVATE *exp;
	TAG *tp;
	TAGF *tfp;

	/* Free up tag information. */
	exp = EXP(sp);
	while ((tp = exp->tagq.tqh_first) != NULL)
		FREETAG(tp);
	while ((tfp = exp->tagfq.tqh_first) != NULL)
		FREETAGF(tfp);
	if (exp->tlast != NULL)
		free(exp->tlast);
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
	TAG *ap, *tp;
	TAGF *atfp, *tfp;

	/* Copy tag stack. */
	oexp = EXP(orig);
	nexp = EXP(sp);
	for (ap = oexp->tagq.tqh_first; ap != NULL; ap = ap->q.tqe_next) {
		MALLOC(sp, tp, TAG *, sizeof(TAG));
		if (tp == NULL)
			goto nomem;
		*tp = *ap;
		if (ap->search != NULL &&
		    (tp->search = strdup(ap->search)) == NULL)
			goto nomem;
		TAILQ_INSERT_TAIL(&nexp->tagq, tp, q);
	}

	/* Copy list of tag files. */
	for (atfp = oexp->tagfq.tqh_first;
	    atfp != NULL; atfp = atfp->q.tqe_next) {
		MALLOC(sp, tfp, TAGF *, sizeof(TAGF));
		if (tfp == NULL)
			goto nomem;
		*tfp = *atfp;
		if ((tfp->name = strdup(atfp->name)) == NULL)
			goto nomem;
		TAILQ_INSERT_TAIL(&nexp->tagfq, tfp, q);
	}

	/* Copy the last tag. */
	if (oexp->tlast != NULL &&
	    (nexp->tlast = strdup(oexp->tlast)) == NULL) {
nomem:		msgq(sp, M_SYSERR, NULL);
		return (1);
	}
	return (0);
}

/*
 * tag_get --
 *	Get a tag from the tags files.
 */
static int
tag_get(sp, tag, tagp, filep, searchp)
	SCR *sp;
	char *tag, **tagp, **filep, **searchp;
{
	struct stat sb;
	EX_PRIVATE *exp;
	TAGF *tfp;
	size_t plen, slen, tlen;
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
		p = msg_print(sp, tag, &nf1);
		msgq(sp, M_ERR, "162|%s: tag not found", p);
		if (nf1)
			FREE_SPACE(sp, p, 0);
		if (echk)
			for (tfp = exp->tagfq.tqh_first;
			    tfp != NULL; tfp = tfp->q.tqe_next)
				if (F_ISSET(tfp, TAGF_ERR) &&
				    !F_ISSET(tfp, TAGF_ERR_WARN)) {
					p = msg_print(sp, tfp->name, &nf1);
					errno = tfp->errno;
					msgq(sp, M_SYSERR, "%s", p);
					if (nf1)
						FREE_SPACE(sp, p, 0);
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
	for (*p++ = '\0'; isblank(*p); ++p);
	for (*filep = p; *p && !isblank(*p); ++p);
	if (*p == '\0')
		goto malformed;
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
		plen = snprintf(pbuf, sizeof(pbuf), "%s/%s", tfp->name, *filep);
		*p = '/';
		if (stat(pbuf, &sb) == 0) {
			slen = strlen(*searchp);
			tlen = strlen(*tagp);
			MALLOC(sp, p, char *, plen + slen + tlen + 5);
			if (p != NULL) {
				memmove(p, *tagp, tlen);
				free(*tagp);
				*tagp = p;
				*(p += tlen) = '\0';
				memmove(++p, pbuf, plen);
				*filep = p;
				*(p += plen) = '\0';
				memmove(++p, *searchp, slen);
				*searchp = p;
				*(p += slen) = '\0';
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
	 * We'd like to test if the file is too big to mmap.  Since we don't
	 * know what size or type off_t's or size_t's are, what the largest
	 * unsigned integral type is, or what random insanity the local C
	 * compiler will perpetrate, doing the comparison in a portable way
	 * is flatly impossible.  Hope that malloc fails if the file is too
	 * large.
	 */
	if (fstat(fd, &sb) || (map = mmap(NULL, (size_t)sb.st_size,
	    PROT_READ, MAP_PRIVATE, fd, (off_t)0)) == (caddr_t)-1) {
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
	int nf;
	char *p;

	nf = 0;
	switch (msg) {
	case TAG_BADLNO:
		p = msg_print(sp, tag, &nf);
		msgq(sp, M_ERR, "164|%s: the tag line doesn't exist", p);
		break;
	case TAG_EMPTY:
		msgq(sp, M_INFO, "165|The tags stack is empty");
		break;
	case TAG_SEARCH:
		p = msg_print(sp, tag, &nf);
		msgq(sp, M_ERR, "166|%s: search pattern not found", p);
		break;
	default:
		abort();
	}
	if (nf)
		FREE_SPACE(sp, p, 0);
}
