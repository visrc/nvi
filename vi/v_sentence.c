/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_sentence.c,v 8.3 1993/06/28 14:22:11 bostic Exp $ (Berkeley) $Date: 1993/06/28 14:22:11 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>

#include "vi.h"
#include "vcmd.h"

/*
 * In historic vi, a sentence was delimited by a '.', '?' or '!' character
 * followed by TWO spaces, an empty line or a newline.  One or more empty
 * lines was also treated as a separate sentence.  Once again, historical vi
 * didn't do sentence movements associated with counts consistently, mostly
 * in the presence of lines containing only white-space characters.
 *
 * This implementation also permits a single tab to delimit sentences, and
 * treats lines containing only white-space characters as empty lines.
 * And, tabs are eaten (along with spaces) when skipping to the start of the
 * text follow a "sentence".
 */
#define	ISSPACE								\
	(cs.cs_flags == CS_EOL || isspace(cs.cs_ch))

/*
 * v_sentencef -- [count])
 *	Move forward count sentences.
 */
int
v_sentencef(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	enum { BLANK, NONE, PERIOD } state;
	VCS cs;
	u_long cnt;

	cs.cs_lno = fm->lno;
	cs.cs_cno = fm->cno;
	if (cs_init(sp, ep, &cs)) 
		return (1);

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;

	/*
	 * If in white-space, the next start of sentence counts as one.
	 * This may not handle "  .  " correctly, but it's real unclear
	 * what correctly means in that case.
	 */
	if (cs.cs_flags == CS_EMP || cs.cs_flags == 0 && ISSPACE) {
		if (cs_fblank(sp, ep, &cs))
			return (1);
		if (--cnt == 0) {
			if (fm->lno != cs.cs_lno || fm->cno != cs.cs_cno) {
				rp->lno = cs.cs_lno;
				rp->cno = cs.cs_cno;
				return (0);
			}
			v_eof(sp, ep, NULL);
			return (1);
		}
	}
	for (state = NONE;;) {
		if (cs_next(sp, ep, &cs))
			return (1);
		if (cs.cs_flags == CS_EOF)
			break;
		if (cs.cs_flags == CS_EOL) {
			if ((state == PERIOD || state == BLANK) && --cnt == 0) {
				if (cs_next(sp, ep, &cs))
					return (1);
				if (cs.cs_flags == 0 && ISSPACE &&
				    cs_fblank(sp, ep, &cs))
					return (1);
				goto ret;
			}
			state = NONE;
			continue;
		}
		if (cs.cs_flags == CS_EMP) {	/* An EMP is two sentences. */
			if (--cnt == 0)
				goto ret;
			if (cs_fblank(sp, ep, &cs))
				return (1);
			if (--cnt == 0)
				goto ret;
			state = NONE;
			continue;
		}
		switch (cs.cs_ch) {
		case '.':
		case '?':
		case '!':
			state = PERIOD;
			break;
		case '\t':
			if (state == PERIOD)
				state = BLANK;
			/* FALLTHROUGH */
		case ' ':
			if (state == PERIOD) {
				state = BLANK;
				break;
			}
			if (state == BLANK && --cnt == 0) {
				if (cs_fblank(sp, ep, &cs))
					return (1);
ret:				rp->lno = cs.cs_lno;
				rp->cno = cs.cs_cno;
				return (0);
			}
			/* FALLTHROUGH */
		default:
			state = NONE;
			break;
		}
	}

	/* EOF is a movement sink. */
	if (fm->lno != cs.cs_lno || fm->cno != cs.cs_cno) {
		rp->lno = cs.cs_lno;
		rp->cno = cs.cs_cno;
		return (0);
	}
	v_eof(sp, ep, NULL);
	return (1);
}

/*
 * v_sentenceb -- [count](
 *	Move backward count sentences.
 */
int
v_sentenceb(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	VCS cs;
	recno_t slno;
	size_t scno;
	u_long cnt;
	int last1, last2;

	if (fm->lno == 1 && fm->cno == 0) {
		v_sof(sp, NULL);
		return (1);
	}

	cs.cs_lno = fm->lno;
	cs.cs_cno = fm->cno;
	if (cs_init(sp, ep, &cs))
		return (1);

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;

	/*
	 * If on an empty line, skip to the first non-white-space
	 * character.
	 */
	if (cs.cs_flags == CS_EMP) {
		if (cs_bblank(sp, ep, &cs))
			return (1);
		if (cs_next(sp, ep, &cs))
			return (1);
	}

	for (last1 = last2 = 0;;) {
		if (cs_prev(sp, ep, &cs))
			return (1);
		if (cs.cs_flags == CS_SOF)
			break;
		if (cs.cs_flags == CS_EOL) {
			last2 = last1;
			last1 = 1;
			continue;
		}
		if (cs.cs_flags == CS_EMP) {
			if (--cnt == 0)
				goto ret;
			if (cs_bblank(sp, ep, &cs))
				return (1);
			last1 = last2 = 0;
			continue;
		}
		if (last1 && last2 &&
		    (cs.cs_ch == '.' || cs.cs_ch == '?' || cs.cs_ch == '!')) {
			if (--cnt != 0) {
				last1 = last2 = 0;
				continue;
			}

ret:			slno = cs.cs_lno;
			scno = cs.cs_cno;

			/* Move to the start of the sentence. */
			if (cs_bblank(sp, ep, &cs))
				return (1);
			/*
			 * If it was ".  xyz", with the cursor on the 'x', or
			 * "end.  ", with the cursor in the spaces, or the
			 * beginning of a sentence preceded by an empty line,
			 * we can end up where we started.  Fix it.
			 */
			if (fm->lno != cs.cs_lno || fm->cno != cs.cs_cno) {
				rp->lno = cs.cs_lno;
				rp->cno = cs.cs_cno;
				return (0);
			}
			/*
			 * Well, if an empty line preceded possible blanks
			 * and the sentence, it could be a real sentence.
			 */
			for (;;) {
				if (cs_prev(sp, ep, &cs))
					return (1);
				if (cs.cs_flags == CS_EOL)
					continue;
				if (cs.cs_flags == 0 && ISSPACE)
					continue;
				break;
			}
			if (cs.cs_flags == CS_EMP) {
				rp->lno = cs.cs_lno;
				rp->cno = cs.cs_cno;
				return (0);
			}
			/* But it wasn't; try again. */
			cnt = 1;
			cs.cs_lno = slno;
			cs.cs_cno = scno;
			last2 = last1 = 0;
		} else if (cs.cs_ch == '\t') {
			last1 = last2 = 1;
		} else {
			last2 = last1;
			last1 = ISSPACE ? 1 : 0;
		}
	}

	/* SOF is a movement sink. */
	rp->lno = cs.cs_lno;
	return (0);
}
