/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_sentence.c,v 5.13 1993/04/12 15:02:37 bostic Exp $ (Berkeley) $Date: 1993/04/12 15:02:37 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * Sentences are sequences of characters terminated by a period followed
 * by at least two spaces or a newline.
 * 
 * Historical vi mishandled lines with only white-space characters.  Forward
 * sentences treated them as part of the current sentence, backward sentences
 * treated them as different sentences.  This implementation treats lines with
 * only white-space characters and empty lines as sentence delimiters, not
 * sentences, in both directions.
 */

#define	EATBLANK(sp)							\
	while (getc_next(sp, ep, FORWARD, &ch) &&			\
	    (ch == EMPTYLINE || ch == ' ' || ch == '\t'))

#define	ISSPACE(ch)							\
	(ch == EMPTYLINE || ch == ' ' || ch == '\t')

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
	enum { NONE, PERIOD, BLANK } state;
	int ch;
	u_long cnt;

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;

	if (getc_init(sp, ep, fm, &ch))
		return (1);

	/*
	 * If in white-space, the next start of sentence counts as one.
	 * This may not handle "  .  " correctly, but it's real unclear
	 * what correctly means in that case.
	 */
	if (ISSPACE(ch)) {
		EATBLANK(sp);
		if (--cnt == 0) {
			getc_set(sp, ep, rp);
			if (fm->lno != rp->lno || fm->cno != rp->cno)
				return (0);
			v_eof(sp, ep, NULL);
			return (1);
		}
	}
	for (state = NONE; getc_next(sp, ep, FORWARD, &ch);)
		switch(ch) {
		case EMPTYLINE:
			if ((state == PERIOD || state == BLANK) && --cnt == 0) {
				EATBLANK(sp);
				getc_set(sp, ep, rp);
				return (0);
			}
			state = NONE;
			break;
		case '.':
		case '?':
		case '!':
			state = PERIOD;
			break;
		case ' ':
		case '\t':
			if (state == PERIOD) {
				state = BLANK;
				break;
			}
			if (state == BLANK && --cnt == 0) {
				EATBLANK(sp);
				getc_set(sp, ep, rp);
				return (0);
			}
			break;
		default:
			state = NONE;
			break;
		}

	/* EOF is a movement sink. */
	getc_set(sp, ep, rp);
	if (fm->lno != rp->lno || fm->cno != rp->cno)
		return (0);

	v_eof(sp, ep, NULL);
	return (1);
}

#undef	EATBLANK
#define	EATBLANK(sp)							\
	while (getc_next(sp, ep, BACKWARD, &ch) &&			\
	    (ch == EMPTYLINE || ch == ' ' || ch == '\t'))

/*
 * v_sentenceb -- [count])
 *	Move forward count sentences.
 */
int
v_sentenceb(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int ch, last1, last2;
	u_long cnt;

	if (fm->lno == 1 && fm->cno == 0) {
		v_sof(sp, NULL);
		return (1);
	}

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;

	if (getc_init(sp, ep, fm, &ch))
		return (1);

	/*
	 * Make ".  xxx" with the cursor on the 'x', and "xxx.  ", with the
	 * cursor in the spaces, work.
	 */
	if (getc_next(sp, ep, BACKWARD, &ch) && ISSPACE(ch))
		EATBLANK(sp);

	for (last1 = last2 = 'a'; getc_next(sp, ep, BACKWARD, &ch);) {
		if ((ch == '.' || ch == '?' || ch == '!') &&
		    ISSPACE(last1) && ISSPACE(last2) && --cnt == 0) {
			while (getc_next(sp, ep, FORWARD, &ch) && ISSPACE(ch));
			getc_set(sp, ep, rp);
			return (0);
		}
		last2 = last1;
		last1 = ch;
	}

	/* SOF is a movement sink. */
	getc_set(sp, ep, rp);
	return (0);
}
