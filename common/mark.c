/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: mark.c,v 8.6 1993/11/03 17:11:19 bostic Exp $ (Berkeley) $Date: 1993/11/03 17:11:19 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"

static MARK *mark_find __P((SCR *, EXF *, ARG_CHAR_T));

/*
 * Marks are maintained in a key sorted doubly linked list.  We can't
 * use arrays because we have no idea how big an index key could be.
 * The underlying assumption is that users don't have more than, say,
 * 10 marks at any one time, so this will be is fast enough.
 *
 * Marks are fixed, and modifications to the line don't update the mark's
 * position in the line.  This can be hard.  If you add text to the line,
 * place a mark in that text, undo the addition and use ` to move to the
 * mark, the location will have disappeared.  It's tempting to try to adjust
 * the mark with the changes in the line, but this is hard to do, especially
 * if we've given the line to v_ntext.c:v_ntext() for editing.  Historic vi
 * would move to the first non-blank on the line when the mark location was
 * past the end of the line.  This can be complicated by deleting to a mark
 * that has disappeared using the ` command.  Historic vi vi treated this as
 * a line-mode motion and deleted the line.  This implementation complains to
 * the user.
 *
 * In historic vi, marks returned if the operation was undone, unless the
 * mark had been subsequently reset.  Tricky.  This is hard to start with,
 * but in the presence of repeated undo it gets nasty.  When a line is
 * deleted, we delete (and log) any marks on that line.  An undo will create
 * the mark.  Any mark creations are noted as to whether the user created
 * it or if it was created by an undo.  The former cannot be reset by another
 * undo, but the latter may. 
 *
 * All of these routines translate ABSMARK2 to ABSMARK1.  Setting either of
 * the absolute mark locations sets both, so that "m'" and "m`" work like
 * they, ah, for lack of a better word, "should".
 */

/*
 * mark_init --
 *	Set up the marks.
 */
int
mark_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	MARK *mp;

	/*
	 * Make sure the marks have been set up.  If they
	 * haven't, do so, and create the absolute mark.
	 */
	if ((mp = malloc(sizeof(MARK))) == NULL) {
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		return (1);
	}
	mp->lno = 1;
	mp->cno = 0;
	mp->name = ABSMARK1;
	mp->flags = 0;
	list_enter_head(&ep->marks, mp, MARK *, q);
	return (0);
}

/*
 * mark_end --
 *	Free up the marks.
 */
int
mark_end(sp, ep)
	SCR *sp;
	EXF *ep;
{
	MARK *mp;

	while ((mp = ep->marks.le_next) != NULL) {
		list_remove(mp, MARK *, q);
		FREE(mp, sizeof(MARK));
	}
	return (0);
}

/*
 * mark_get --
 *	Get the location referenced by a mark.
 */
MARK *
mark_get(sp, ep, key)
	SCR *sp;
	EXF *ep;
	ARG_CHAR_T key;
{
	MARK *mp;
	size_t len;
	char *p;

	if (key == ABSMARK2)
		key = ABSMARK1;

	if ((mp = mark_find(sp, ep, key)) == NULL)
		return (NULL);
	if (mp->name != key) {
		msgq(sp, M_BERR, "Mark %s: not set.", sp->cname[key].name);
                return (NULL);
	}
	if (F_ISSET(mp, MARK_DELETED)) {
		msgq(sp, M_BERR,
		    "Mark %s: the line was deleted.", sp->cname[key].name);
                return (NULL);
	}
	if ((p = file_gline(sp, ep, mp->lno, &len)) == NULL ||
	    mp->cno > len || mp->cno == len && len != 0) {
		msgq(sp, M_BERR, "Mark %s: cursor position no longer exists.",
		    sp->cname[key].name);
		return (NULL);
	}
	return (mp);
}

/*
 * mark_set --
 *	Set the location referenced by a mark.
 */
int
mark_set(sp, ep, key, value, userset)
	SCR *sp;
	EXF *ep;
	ARG_CHAR_T key;
	MARK *value;
	int userset;
{
	MARK *mp, *mt;

	if (key == ABSMARK2)
		key = ABSMARK1;

	if ((mp = mark_find(sp, ep, key)) == NULL)
		return (1);
	/*
	 * The rules are simple.  If the user is setting a mark (if it's a
	 * new mark this is always true), it always happens.  If not, it's
	 * an undo, and we set it if it's not already set or if it was set
	 * by a previous undo.
	 */
	if (mp->name != key) {
		if ((mt = malloc(sizeof(MARK))) == NULL) {
			msgq(sp, M_ERR, "Error: %s", strerror(errno));
			return (1);
		}
		list_insert_after(&mp->q, mt, MARK *, q);
		mp = mt;
	} else if (!userset &&
	    !F_ISSET(mp, MARK_DELETED) && F_ISSET(mp, MARK_USERSET))
		return (0);

	mp->lno = value->lno;
	mp->cno = value->cno;
	mp->name = key;
	mp->flags = userset ? MARK_USERSET : 0;
	return (0);
}

/*
 * mark_find --
 *	Find the requested mark, or, the slot immediately before
 *	where it would go.
 */
static MARK *
mark_find(sp, ep, key)
	SCR *sp;
	EXF *ep;
	ARG_CHAR_T key;
{
	MARK *mp, *lastmp;

	/*
	 * Return the requested mark or the slot immediately before
	 * where it should go.
	 */
	for (lastmp = NULL, mp = ep->marks.le_next;
	    mp != NULL; lastmp = mp, mp = mp->q.qe_next)
		if (mp->name >= key)
			return (mp->name == key ? mp : lastmp);
	return (lastmp);
}

/*
 * mark_delete --
 *	Update the marks based on a deletion.
 */
void
mark_delete(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	MARK *mp;

	for (mp = ep->marks.le_next; mp != NULL; mp = mp->q.qe_next)
		if (mp->lno >= lno)
			if (mp->lno == lno) {
				F_SET(mp, MARK_DELETED);
				(void)log_mark(sp, ep, mp);
			} else
				--mp->lno;
}

/*
 * mark_insert --
 *	Update the marks based on an insertion.
 */
void
mark_insert(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	MARK *mp;

	for (mp = ep->marks.le_next; mp != NULL; mp = mp->q.qe_next)
		if (mp->lno >= lno)
			++mp->lno;
}
