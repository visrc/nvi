/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_itxt.c,v 5.36 1993/04/05 07:49:04 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:49:04 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"
#include "text.h"

/*
 * v_iA -- [count]A
 *	Append text to the end of the line.
 */
int
v_iA(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		/* Move the cursor to the end of the line. */
		if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			if (file_lline(sp, ep) != 0) {
				GETLINE_ERR(sp, fm->lno);
				return (1);
			}
			len = 0;
		} else 
			sp->cno = len;

		/* Set flag to put an extra space at the end of the line. */
		if (v_ntext(sp, ep, vp, NULL, p, len, rp, OOBLNO, N_APPENDEOL))
			return (1);

		vp->flags |= VC_ISDOT;
	}
	return (0);
}

/*
 * v_ia -- [count]a
 *	Append text to the cursor position.
 */
int
v_ia(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	int flags;
	size_t len;
	char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		/*
		 * Move the cursor one column to the right and
		 * repaint the screen.
		 */
		flags = 0;
		if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			if (file_lline(sp, ep) != 0) {
				GETLINE_ERR(sp, fm->lno);
				return (1);
			}
			flags = N_APPENDEOL;
			len = 0;
		} else if (len) {
			if (len == sp->cno + 1) {
				flags = N_APPENDEOL;
				sp->cno = len;
			} else
				++sp->cno;
		} else
			flags = N_APPENDEOL;
		if (v_ntext(sp, ep, vp, NULL, p, len, rp, OOBLNO, flags))
			return (1);

		vp->flags |= VC_ISDOT;
	}
	return (0);
}

/*
 * v_iI -- [count]I
 *	Insert text at the front of the line.
 */
int
v_iI(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		/*
		 * Move the cursor to the start of the line and repaint
		 * the screen.
		 */
		if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			if (file_lline(sp, ep) != 0) {
				GETLINE_ERR(sp, fm->lno);
				return (1);
			}
			len = 0;
		} else if (sp->cno != 0)
			sp->cno = 0;
		if (v_ntext(sp, ep, vp,
		    NULL, p, len, rp, OOBLNO, len == 0 ? N_APPENDEOL : 0))
			return (1);

		vp->flags |= VC_ISDOT;
	}
	return (0);
}

/*
 * v_ii -- [count]i
 *	Insert text at the cursor position.
 */
int
v_ii(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			if (file_lline(sp, ep) != 0) {
				GETLINE_ERR(sp, fm->lno);
				return (1);
			}
			len = 0;
		}
		if (v_ntext(sp, ep, vp,
		    NULL, p, len, rp, OOBLNO, len == 0 ? N_APPENDEOL : 0))
			return (1);

		vp->flags |= VC_ISDOT;
	}
	return (0);
}

/*
 * v_iO -- [count]O
 *	Insert text above this line.
 */
int
v_iO(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		if (fm->lno == 1 && file_lline(sp, ep) == 0) {
			p = NULL;
			len = 0;
		} else {
			p = "";
			len = 0;
			if (file_iline(sp, ep, sp->lno, p, len))
				return (1);
			if ((p = file_gline(sp, ep, sp->lno, &len)) == NULL) {
				GETLINE_ERR(sp, sp->lno);
				return (1);
			}
			sp->cno = 0;
		}
		if (v_ntext(sp, ep, vp, NULL,
		    p, len, rp, sp->lno + 1, N_APPENDEOL | N_AUTOINDENT))
			return (1);

		vp->flags |= VC_ISDOT;
	}
	return (0);
}

/*
 * v_io -- [count]o
 *	Insert text after this line.
 */
int
v_io(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		if (sp->lno == 1 && file_lline(sp, ep) == 0) {
			p = NULL;
			len = 0;
		} else {
			p = "";
			len = 0;
			if (file_aline(sp, ep, sp->lno, p, len))
				return (1);
			if ((p = file_gline(sp, ep, ++sp->lno, &len)) == NULL) {
				GETLINE_ERR(sp, sp->lno);
				return (1);
			}
			sp->cno = 0;
		}
		if (v_ntext(sp, ep, vp, NULL,
		    p, len, rp, sp->lno - 1, N_APPENDEOL | N_AUTOINDENT))
			return (1);

		vp->flags |= VC_ISDOT;
	}
	return (0);
}

/*
 * v_Change -- [buffer][count]C
 *	Change line command.
 */
int
v_Change(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	int flags;
	char *p;

	/*
	 * There are two cases -- if a count is supplied, we do a line
	 * mode change where we delete the lines and then insert text
	 * into a new line.  Otherwise, we replace the current line.
	 */
	tm->lno = fm->lno + (vp->flags & VC_C1SET ? vp->count - 1 : 0);
	if (fm->lno != tm->lno) {
		/* Make sure that the to line is real. */
		if (file_gline(sp, ep, tm->lno, NULL) == NULL) {
			GETLINE_ERR(sp, tm->lno);
			return (1);
		}

		/* Cut the line. */
		if (cut(sp, ep, VICB(vp), fm, tm, 1))
			return (1);

		/* Insert a line while we still can... */
		if (file_iline(sp, ep, fm->lno, "", 0))
			return (1);
		++fm->lno;
		++tm->lno;
		if (delete(sp, ep, fm, tm, 1))
			return (1);
		if ((p = file_gline(sp, ep, --fm->lno, &len)) == NULL) {
			GETLINE_ERR(sp, fm->lno);
			return (1);
		}
		sp->lno = fm->lno;
		sp->cno = 0;
		return (v_ntext(sp, ep, vp, NULL, p, len, rp, OOBLNO, 0));
	}

	/* The line may be empty, but that's okay. */
	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep) != 0) {
			GETLINE_ERR(sp, tm->lno);
			return (1);
		}
		flags = N_APPENDEOL;
		len = 0;
	} else {
		if (cut(sp, ep, VICB(vp), fm, tm, 1))
			return (1);
		tm->cno = len;
		flags = N_EMARK | N_OVERWRITE;
	}
	return (v_ntext(sp, ep, vp, fm, p, len, rp, OOBLNO, flags));
}

/*
 * v_change -- [buffer][count]c[count]motion
 *	Change command.
 */
int
v_change(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	int flags, lmode;
	char *p;

	lmode = vp->flags & VC_LMODE;

	/*
	 * If the movement is off the line, delete the range, insert a new
	 * line and go into insert mode.
	 */
	if (fm->lno != tm->lno) {
		/* Cut the line. */
		if (cut(sp, ep, VICB(vp), fm, tm, lmode))
			return (1);

		/* Insert a line while we still can... */
		if (file_iline(sp, ep, fm->lno, "", 0))
			return (1);
		++fm->lno;
		++tm->lno;
		if (delete(sp, ep, fm, tm, lmode))
			return (1);
		if ((p = file_gline(sp, ep, --fm->lno, &len)) == NULL) {
			GETLINE_ERR(sp, fm->lno);
			return (1);
		}
		sp->lno = fm->lno;
		sp->cno = 0;
		return (v_ntext(sp, ep, vp, NULL, p, len, rp, OOBLNO, 0));
	}

	/* Otherwise, do replacement. */
	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep) != 0) {
			GETLINE_ERR(sp, fm->lno);
			return (1);
		}
		flags = N_APPENDEOL;
		len = 0;
	} else {
		/* Cut the line. */
		if (cut(sp, ep, VICB(vp), fm, tm, lmode))
			return (1);
		flags = N_EMARK | N_OVERWRITE;
	}
	return (v_ntext(sp, ep, vp, tm, p, len, rp, OOBLNO, flags));
}

/*
 * v_Replace -- [count]R
 *	Overwrite multiple characters.
 */
int
v_Replace(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	int flags, notfirst;
	char *p;

	*rp = *fm;
	notfirst = 0;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;) {
		if ((p = file_gline(sp, ep, rp->lno, &len)) == NULL) {
			if (file_lline(sp, ep) != 0) {
				GETLINE_ERR(sp, fm->lno);
				return (1);
			}
			flags = N_APPENDEOL;
			len = 0;
		} else
			flags = N_OVERWRITE | N_REPLACE;
		/*
		 * Special case.  The historic vi handled [count]R badly, in
		 * that R would replace some number of characters, and then
		 * the count would append count-1 copies of the replacing chars
		 * to the replaced space.  This seems wrong, so this version
		 * counts R commands.  Move back to where the user stopped
		 * replacing after each R command.
		 */
		if (notfirst && len) {
			++rp->cno;
			sp->lno = rp->lno;
			sp->cno = rp->cno;
			vp->flags |= VC_ISDOT;
		}
		notfirst = 1;
		tm->lno = rp->lno;
		tm->cno = len ? len : 0;
		if (v_ntext(sp, ep, vp, tm, p, len, rp, OOBLNO, flags))
			return (1);
	}
	return (0);
}

/*
 * v_subst -- [buffer][count]s
 *	Substitute characters.
 */
int
v_subst(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	int flags;
	char *p;

	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep) != 0) {
			GETLINE_ERR(sp, fm->lno);
			return (1);
		}
		len = 0;
		flags = N_APPENDEOL;
	} else
		flags = N_EMARK | N_OVERWRITE;

	tm->lno = fm->lno;
	tm->cno = fm->cno + (vp->flags & VC_C1SET ? vp->count : 1);
	if (tm->cno > len)
		tm->cno = len;

	if (p != NULL && cut(sp, ep, VICB(vp), fm, tm, 0))
		return (1);

	return (v_ntext(sp, ep, vp, tm, p, len, rp, OOBLNO, flags));
}
