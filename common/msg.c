/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: msg.c,v 8.2 1994/05/18 18:41:43 bostic Exp $ (Berkeley) $Date: 1994/05/18 18:41:43 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"

/*
 * msgq --
 *	Display a message.
 */
void
#ifdef __STDC__
msgq(SCR *sp, enum msgtype mt, const char *fmt, ...)
#else
msgq(sp, mt, fmt, va_alist)
	SCR *sp;
	enum msgtype mt;
        char *fmt;
        va_dcl
#endif
{
        va_list ap;
	size_t len;
	char msgbuf[1024];

#ifdef __STDC__
        va_start(ap, fmt);
#else
        va_start(ap);
#endif
	/*
	 * It's possible to enter msg when there's no screen to hold
	 * the message.  If sp is NULL, ignore the special cases and
	 * just build the message, using __global_list.
	 */
	if (sp == NULL)
		goto nullsp;

	switch (mt) {
	case M_BERR:
		if (!F_ISSET(sp, S_EXSILENT) &&
		    F_ISSET(sp->gp, G_STDIN_TTY) && !O_ISSET(sp, O_VERBOSE)) {
			F_SET(sp, S_BELLSCHED);
			return;
		}
		mt = M_ERR;
		break;
	case M_VINFO:
		if (!O_ISSET(sp, O_VERBOSE))
			return;
		mt = M_INFO;
		/* FALLTHROUGH */
	case M_INFO:
		if (F_ISSET(sp, S_EXSILENT))
			return;
		break;
	case M_ERR:
	case M_SYSERR:
		break;
	default:
		abort();
	}

nullsp:	len = 0;

#define	EPREFIX	"Error: "
	if (mt == M_SYSERR) {
		memmove(msgbuf, EPREFIX, sizeof(EPREFIX) - 1);
		len += sizeof(EPREFIX) - 1;
	}

	if (sp != NULL && sp->if_name != NULL) {
		len += snprintf(msgbuf + len, sizeof(msgbuf) - len,
		    "%s, %d: ", sp->if_name, sp->if_lno);
		if (len >= sizeof(msgbuf))
			goto err;
	}

	if (fmt != NULL) {
		len += vsnprintf(msgbuf + len, sizeof(msgbuf) - len, fmt, ap);
		if (len >= sizeof(msgbuf))
			goto err;
	}

	if (mt == M_SYSERR) {
		len += snprintf(msgbuf + len,
		    sizeof(msgbuf) - len, ": %s", strerror(errno));
		if (len >= sizeof(msgbuf))
			goto err;
	}

	/*
	 * If len >= the size, some characters were discarded.
	 * Ignore trailing nul.
	 */
err:	if (len >= sizeof(msgbuf))
		len = sizeof(msgbuf) - 1;

#ifdef DEBUG
	if (sp != NULL)
		TRACE(sp, "%.*s\n", len, msgbuf);
#endif
	msg_app(__global_list, sp, mt == M_ERR ? 1 : 0, msgbuf, len);
}

/*
 * msg_app --
 *	Append a message into the queue.  This can fail, but there's
 *	nothing we can do if it does.
 */
void
msg_app(gp, sp, inv_video, p, len)
	GS *gp;
	SCR *sp;
	int inv_video;
	char *p;
	size_t len;
{
	static int reenter;		/* STATIC: Re-entrancy check. */
	sigset_t set;
	MSG *mp, *nmp;

	/*
	 * It's possible to reenter msg when it allocates space.
	 * We're probably dead anyway, but no reason to drop core.
	 */
	if (reenter)
		return;
	reenter = 1;

	/*
	 * We can be entered as the result of a signal arriving, trying
	 * to sync the file and failing.  This shouldn't be a hot spot,
	 * block the signals.
	 */
	SIGBLOCK;

	/*
	 * Find an empty structure, or allocate a new one.  Use the
	 * screen structure if it exists, otherwise the global one.
	 */
	if (sp != NULL) {
		if ((mp = sp->msgq.lh_first) == NULL) {
			CALLOC(sp, mp, MSG *, 1, sizeof(MSG));
			if (mp == NULL)
				goto ret;
			LIST_INSERT_HEAD(&sp->msgq, mp, q);
			goto store;
		}
	} else if ((mp = gp->msgq.lh_first) == NULL) {
		CALLOC(sp, mp, MSG *, 1, sizeof(MSG));
		if (mp == NULL)
			goto ret;
		LIST_INSERT_HEAD(&gp->msgq, mp, q);
		goto store;
	}
	while (!F_ISSET(mp, M_EMPTY) && mp->q.le_next != NULL)
		mp = mp->q.le_next;
	if (!F_ISSET(mp, M_EMPTY)) {
		CALLOC(sp, nmp, MSG *, 1, sizeof(MSG));
		if (nmp == NULL)
			goto ret;
		LIST_INSERT_AFTER(mp, nmp, q);
		mp = nmp;
	}

	/* Get enough memory for the message. */
store:	if (len > mp->blen && binc(sp, &mp->mbuf, &mp->blen, len))
		goto ret;

	/* Store the message. */
	memmove(mp->mbuf, p, len);
	mp->len = len;
	mp->flags = inv_video ? M_INV_VIDEO : 0;

ret:	reenter = 0;
	SIGUNBLOCK;
}

/*
 * msgrpt --
 *	Report on the lines that changed.
 *
 * !!!
 * Historic vi documentation (USD:15-8) claimed that "The editor will also
 * always tell you when a change you make affects text which you cannot see."
 * This isn't true -- edit a large file and do "100d|1".  We don't implement
 * this semantic as it would require that we track each line that changes
 * during a command instead of just keeping count.
 *
 * Line counts weren't right in historic vi, either.  For example, given the
 * file:
 *	abc
 *	def
 * the command 2d}, from the 'b' would report that two lines were deleted,
 * not one.
 */
int
msg_rpt(sp, is_message)
	SCR *sp;
	int is_message;
{
	static char * const action[] = {
		"added", "changed", "copied", "deleted", "joined",
		"moved", "left shifted", "right shifted", "yanked",
		NULL,
	};
	recno_t total;
	u_long rval;
	int first, cnt;
	size_t blen, len;
	char * const *ap;
	char *bp, *p, number[40];

	if (F_ISSET(sp, S_EXSILENT))
		return (0);

	if ((rval = O_VAL(sp, O_REPORT)) == 0)
		goto norpt;

	GET_SPACE_RET(sp, bp, blen, 512);
	p = bp;

	total = 0;
	for (ap = action, cnt = 0, first = 1; *ap != NULL; ++ap, ++cnt)
		if (sp->rptlines[cnt] != 0) {
			total += sp->rptlines[cnt];
			len = snprintf(number, sizeof(number),
			    "%s%lu lines %s",
			    first ? "" : "; ", sp->rptlines[cnt], *ap);
			memmove(p, number, len);
			p += len;
			first = 0;
		}

	/*
	 * If nothing to report, return.  Note that the number of lines
	 * must be > than the user's value, not >=.  This is historic
	 * practice and means that users cannot report on single line
	 * changes.
	 */
	if (total > rval) {
		*p = '\0';
		if (is_message)
			msgq(sp, M_INFO, "%s", bp);
		else
			ex_printf(EXCOOKIE, "%s\n", bp);
	}

	FREE_SPACE(sp, bp, blen);

	/* Clear after each report. */
norpt:	sp->rptlchange = OOBLNO;
	memset(sp->rptlines, 0, sizeof(sp->rptlines));
	return (0);
}

#ifdef MSG_CATALOG
/*
 * get_msg --
 *	Return a format based on a message number.
 */
char *
get_msg(sp, msgno)
	SCR *sp;
	char *s_msgno;
{
	DBT data, key;
	GS *gp;
	recno_t msgno;
	char *msg, *p;

	gp = sp == NULL ? __global_list : sp->gp;
	if (gp->msgdb == NULL) {
		p = sp == NULL ? _PATH_MSGDEF : O_STR(sp, O_CATALOG);
		if ((gp->msgdb = dbopen(p,
		    O_NONBLOCK | O_RDONLY, 444, DB_RECNO, NULL)) == NULL) {
			if ((fmt = malloc(256)) == NULL)
				return ("");
			(void)snprintf(fmt,
			    "unable to open %s: %s", p, strerror(errno));
			return (fmt);
		}
	}
	msgno = atoi(s_msgno);
	key.data = &msgno;
	key.size = sizeof(recno_t);
	switch (gp->msgdb->get(gp->msgdb, &key, &data, 0)) {
	case 0:
		return (data.data);
	case 1:
		p = "no catalog record %ls";
		break;
	case -1:
		p = "catalog record %s: %s";
		break;
	}
	if ((fmt = malloc(256)) == NULL)
		return ("");
	(void)snprintf(fmt, p, msgno, strerror(errno));
	return (fmt);
}
#endif
