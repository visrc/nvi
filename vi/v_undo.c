/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_undo.c,v 5.12 1992/11/08 10:41:34 bostic Exp $ (Berkeley) $Date: 1992/11/08 10:41:34 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"
#include "screen.h"
#include "extern.h"

/*
 * v_undo -- u
 *	Abort the last change.
 */
int
v_undo(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
#ifdef NOT_RIGHT_NOW
	if (undo()) {
		scr_ref(curf);
		refresh();
	}
#endif
	*rp = *fm;
	return (0);
}

static u_long s_lno;
static char *s_text;
static size_t s_textlen;

/*
 * v_undosave --
 *	Saves a line for a later undo.
 */
void
v_undosave(m)
	MARK *m;			/* Line to save. */
{
	static size_t s_tsize;
	size_t len;
	u_char *p;

	if (s_lno == m->lno)		/* Don't resave. */
		return;

	p = file_gline(curf, m->lno, &len);
	if (s_tsize < len && (s_text = realloc(s_text, len + 2)) == NULL) {
		msg("Error: %s", strerror(errno));
		return;
	}
	bcopy(p, s_text, len);
	s_text[len++] = '\n';		/* XXX */
	s_text[len] = '\0';
	s_textlen = len;
	s_lno = m->lno;
}

/*
 * v_undoline -- U
 *	Undoes changes to a single line, if possible.
 */
MARK *
v_undoline(m)
	MARK *m;			/* Line to "undo". */
{
	MARK t;

	/* Make sure we have the right line in the buffer. */
	if (s_lno != m->lno)
		return (NULL);

	/* Replace with the old contents. */
	t.lno = m->lno + 1;
	t.cno = 0;
	delete(curf, m, &t, 1);
	add(m, s_text, s_textlen);
	curf->rptlabel = "changed";

	/* Return, with the cursor at the front of the line. */
	m->cno = 0;
	return (m);
}
