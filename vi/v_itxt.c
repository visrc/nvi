/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_itxt.c,v 5.42 1993/05/08 09:54:50 bostic Exp $ (Berkeley) $Date: 1993/05/08 09:54:50 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

#define	SET_TXT_STD(sp, f) {						\
	LF_INIT((f) | TXT_BEAUTIFY | TXT_CNTRLT | TXT_ESCAPE |		\
	    TXT_MAPINPUT | TXT_RECORD | TXT_RESOLVE);			\
	if (O_ISSET(sp, O_AUTOINDENT))					\
		LF_SET(TXT_AUTOINDENT);					\
}

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
	u_int flags;
	char *p;

	SET_TXT_STD(sp, TXT_APPENDEOL);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	for (cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
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
		if (v_ntext(sp, ep,
		    &sp->txthdr, NULL, p, len, rp, 0, OOBLNO, flags))
			return (1);

		SET_TXT_STD(sp, TXT_APPENDEOL | TXT_REPLAY);
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
	u_int flags;
	size_t len;
	char *p;

	SET_TXT_STD(sp, 0);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	for (cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		/*
		 * Move the cursor one column to the right and
		 * repaint the screen.
		 */
		if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			if (file_lline(sp, ep) != 0) {
				GETLINE_ERR(sp, fm->lno);
				return (1);
			}
			LF_SET(TXT_APPENDEOL);
			len = 0;
		} else if (len) {
			if (len == sp->cno + 1) {
				LF_SET(TXT_APPENDEOL);
				sp->cno = len;
			} else
				++sp->cno;
		} else
			LF_SET(TXT_APPENDEOL);
		if (v_ntext(sp, ep,
		    &sp->txthdr, NULL, p, len, rp, 0, OOBLNO, flags))
			return (1);

		SET_TXT_STD(sp, TXT_REPLAY);
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
	u_int flags;
	char *p;

	SET_TXT_STD(sp, 0);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	for (cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
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
		if (len == 0)
			LF_SET(TXT_APPENDEOL);
		if (v_ntext(sp, ep,
		    &sp->txthdr, NULL, p, len, rp, 0, OOBLNO, flags))
			return (1);

		SET_TXT_STD(sp, TXT_REPLAY);
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
	u_int flags;
	char *p;

	SET_TXT_STD(sp, 0);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	for (cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			if (file_lline(sp, ep) != 0) {
				GETLINE_ERR(sp, fm->lno);
				return (1);
			}
			len = 0;
		}
		if (len == 0)
			LF_SET(TXT_APPENDEOL);
		if (v_ntext(sp, ep,
		    &sp->txthdr, NULL, p, len, rp, 0, OOBLNO, flags))
			return (1);

		SET_TXT_STD(sp, TXT_REPLAY);
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
	recno_t ai_line;
	size_t len;
	u_long cnt;
	u_int flags;
	char *p;

	SET_TXT_STD(sp, TXT_APPENDEOL);
	if (F_ISSET(vp, VC_ISDOT))
		LF_SET(TXT_REPLAY);
	for (cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		if (fm->lno == 1 && file_lline(sp, ep) == 0) {
			p = NULL;
			len = 0;
			ai_line = OOBLNO;
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
			ai_line = sp->lno + 1;
		}
		if (v_ntext(sp, ep,
		    &sp->txthdr, NULL, p, len, rp, 0, ai_line, flags))
			return (1);

		SET_TXT_STD(sp, TXT_APPENDEOL | TXT_REPLAY);
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
	recno_t ai_line;
	size_t len;
	u_long cnt;
	u_int flags;
	char *p;

	SET_TXT_STD(sp, TXT_APPENDEOL);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	for (cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		if (sp->lno == 1 && file_lline(sp, ep) == 0) {
			p = NULL;
			len = 0;
			ai_line = OOBLNO;
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
			ai_line = sp->lno - 1;
		}
		if (v_ntext(sp, ep,
		    &sp->txthdr, NULL, p, len, rp, 0, ai_line, flags))
			return (1);

		SET_TXT_STD(sp, TXT_APPENDEOL | TXT_REPLAY);
		if (O_ISSET(sp, O_AUTOINDENT))
			LF_SET(TXT_AUTOINDENT);
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
	u_int flags;
	char *p;

	SET_TXT_STD(sp, 0);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);

	/*
	 * There are two cases -- if a count is supplied, we do a line
	 * mode change where we delete the lines and then insert text
	 * into a new line.  Otherwise, we replace the current line.
	 */
	tm->lno = fm->lno + (F_ISSET(vp, VC_C1SET) ? vp->count - 1 : 0);
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
		tm = NULL;
	} else { 
		/* The line may be empty, but that's okay. */
		if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			if (file_lline(sp, ep) != 0) {
				GETLINE_ERR(sp, tm->lno);
				return (1);
			}
			LF_SET(TXT_APPENDEOL);
			len = 0;
		} else {
			if (cut(sp, ep, VICB(vp), fm, tm, 1))
				return (1);
			tm->cno = len;
			LF_SET(TXT_EMARK | TXT_OVERWRITE);
		}
	}
	return (v_ntext(sp, ep,
	    &sp->txthdr, tm, p, len, rp, 0, OOBLNO, flags));
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
	u_int flags;
	int lmode;
	char *p;

	SET_TXT_STD(sp, 0);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	lmode = F_ISSET(vp, VC_LMODE);

	/* Cut the line. */
	if (cut(sp, ep, VICB(vp), fm, tm, lmode))
		return (1);

	/*
	 * If the movement is inside the line, edit the line.  Otherwise,
	 * delete the range, insert a new line and go into insert mode.
	 */
	if (fm->lno == tm->lno ||
	    fm->lno + 1 == tm->lno && tm->cno == 0) {
		if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			if (file_lline(sp, ep) != 0) {
				GETLINE_ERR(sp, fm->lno);
				return (1);
			}
			LF_SET(TXT_APPENDEOL);
			len = 0;
		} else
			LF_SET(TXT_EMARK | TXT_OVERWRITE);
	} else {

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

		tm = NULL;
	}
	return (v_ntext(sp, ep,
	    &sp->txthdr, tm, p, len, rp, 0, OOBLNO, flags));
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
	u_int flags;
	char *p;

	SET_TXT_STD(sp, 0);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);

	*rp = *fm;
	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	if ((p = file_gline(sp, ep, rp->lno, &len)) == NULL) {
		if (file_lline(sp, ep) != 0) {
			GETLINE_ERR(sp, rp->lno);
			return (1);
		}
		LF_SET(TXT_APPENDEOL);
		len = 0;
	} else
		LF_SET(TXT_OVERWRITE | TXT_REPLACE);
	tm->lno = rp->lno;
	tm->cno = len ? len : 0;
	if (v_ntext(sp, ep, &sp->txthdr, tm, p, len, rp, 0, OOBLNO, flags))
		return (1);

	/*
	 * Special case.  The historic vi handled [count]R badly, in that R
	 * would replace some number of characters, and then the count would
	 * append count-1 copies of the replacing chars to the replaced space.
	 * This seems wrong, so this version counts R commands.  There is some
	 * trickiness in moving back to where the user stopped replacing after
	 * each R command.  Basically, if the user ended with a newline, we
	 * want to use rp->cno (which will be 0).  Otherwise, use the column
	 * after the returned cursor, unless it would be past the end of the
	 * line, in which case we append to the line.
	 */
	while (--cnt) {
		if ((p = file_gline(sp, ep, rp->lno, &len)) == NULL)
			GETLINE_ERR(sp, rp->lno);
		SET_TXT_STD(sp, TXT_REPLAY);

		sp->lno = rp->lno;

		if (len == 0 || rp->cno == len - 1) {
			sp->cno = len;
			LF_SET(TXT_APPENDEOL);
		} else {
			sp->cno = rp->cno;
			if (rp->cno != 0)
				++sp->cno;
			LF_SET(TXT_OVERWRITE | TXT_REPLACE);
		}

		if (v_ntext(sp, ep,
		    &sp->txthdr, tm, p, len, rp, 0, OOBLNO, flags))
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
	u_int flags;
	char *p;

	SET_TXT_STD(sp, 0);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep) != 0) {
			GETLINE_ERR(sp, fm->lno);
			return (1);
		}
		len = 0;
		LF_SET(TXT_APPENDEOL);
	} else
		LF_SET(TXT_EMARK | TXT_OVERWRITE);

	tm->lno = fm->lno;
	tm->cno = fm->cno + (F_ISSET(vp, VC_C1SET) ? vp->count : 1);
	if (tm->cno > len)
		tm->cno = len;

	if (p != NULL && cut(sp, ep, VICB(vp), fm, tm, 0))
		return (1);

	return (v_ntext(sp, ep,
	    &sp->txthdr, tm, p, len, rp, 0, OOBLNO, flags));
}
