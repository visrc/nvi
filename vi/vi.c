/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vi.c,v 5.54 1993/03/28 19:05:54 bostic Exp $ (Berkeley) $Date: 1993/03/28 19:05:54 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

static int getcmd __P((SCR *, EXF *, VICMDARG *, VICMDARG *));
static int getkeyword __P((SCR *, EXF *, VICMDARG *, u_int));
static int getmotion __P((SCR *, EXF *, VICMDARG *, MARK *, MARK *));

static VICMDARG cmd, dot, dotmotion;

/*
 * vi --
 * 	Main vi command loop.
 */
int
vi(sp, ep)
	SCR *sp;
	EXF *ep;
{
	MARK fm, tm, m;
	VICMDARG *vp;
	int eval;
	u_int flags;

	if (v_init(sp, ep))
		return (1);
	status(sp, ep, ep->lno);

	for (eval = 0;;) {
err:		if (!F_ISSET(sp, S_SCREENWAIT) && sp->refresh(sp, ep)) {
			eval = 1;
			break;
		}

		/*
		 * We get a command, which may or may not have an associated
		 * motion.  If it does, we get it too, calling its underlying
		 * function to get the resulting mark.  We then call the
		 * command setting the cursor to the resulting mark.
		 */
		vp = &cmd;
		if (getcmd(sp, ep, vp, NULL))
			goto err;

		flags = vp->kp->flags;

		/* Get any associated keyword. */
		if (flags & (V_KEYNUM|V_KEYW) && getkeyword(sp, ep, vp, flags))
			goto err;

		/* If a non-relative movement, set the '' mark. */
		if (flags & V_ABS) {
			m.lno = ep->lno;
			m.cno = ep->cno;
			SETABSMARK(sp, ep, &m);
		}

		/*
		 * Do any required motion.  If no motion specified, and it's
		 * a line-oriented command, set the to MARK anyway, it's used
		 * by some commands.  If count specified, then the to MARK is
		 * set to that many lines, counting the current one.
		 */
		if (flags & V_MOTION) {
			if (getmotion(sp, ep, vp, &fm, &tm))
				goto err;
		} else {
			fm.lno = ep->lno;
			fm.cno = ep->cno;
			if (vp->kp->flags & V_LMODE && vp->flags & VC_C1SET) {
				tm.lno = ep->lno + vp->count - 1;
				tm.cno = ep->cno;
			} else
				tm = fm;
		}

		/* Log the start of an action. */
		(void)log_cursor(sp, ep);

		/* Call the function, update the cursor. */
		if ((vp->kp->func)(sp, ep, vp, &fm, &tm, &m))
			goto err;
		
		/*
		 * If that command took us out of vi, then exit
		 * the loop without further action.
		 */
		if (!F_ISSET(sp, S_MODE_VI) || F_ISSET(sp, S_FILE_CHANGED))
			break;

		/* Set the dot command structure. */
		if (flags & V_DOT) {
			dot = cmd;
			dot.flags |= VC_ISDOT;
			/*
			 * If a count supplied for both the motion and the
			 * command, the count applies only to the motion.
			 * Reset the command count in the dot structure.
			 */
			if (vp->flags & VC_C1RESET)
				dot.flags |= VC_C1SET;
		}

		/*
		 * Some vi row movements are "attracted" to the last position
		 * set, i.e. the V_RCM commands are moths to the V_RCM_SET
		 * commands' candle.  It's broken into two parts.  Here we deal
		 * with the command flags.  In sp->relative(), we deal with the
		 * screen flags.  If the movement is to the EOL, the vi command
		 * handles it.  If it's to the beginning, we handle it here.
		 *
		 * Does this totally violate the screen and editor layering?
		 * You betcha.  As they say, if you think you understand it,
		 * you don't.
		 */
		if (flags & V_RCM)
			m.cno = sp->relative(sp, ep, m.lno);
		else if (flags & V_RCM_SETFNB) {
			if (nonblank(sp, ep, m.lno, &m.cno))
				goto err;
			sp->rcmflags = RCM_FNB;
		}
		else if (flags & V_RCM_SETLAST)
			sp->rcmflags = RCM_LAST;
			
		/* Update the cursor. */
		ep->lno = m.lno;
		ep->cno = m.cno;

		/* Report on the changes from the command. */
		if (sp->rptlines) {
			if (LVAL(O_REPORT) && sp->rptlines >= LVAL(O_REPORT)) {
				msgq(sp, M_DISPLAY,
				    "%ld line%s %s", sp->rptlines,
				    sp->rptlines == 1 ? "" : "s", sp->rptlabel);
			}
			sp->rptlines = 0;
		}

		/* Set the new favorite position. */
		if (flags & V_RCM_SET) {
			sp->rcmflags = 0;
			sp->rcm = sp->scno;
		}
	}
	return (v_end(sp) || eval);
}

#define	KEY(sp, k, flags) {						\
	(k) = getkey(sp, flags);					\
	if (F_ISSET(sp, S_SCHED_UPDATE)) {				\
		F_CLR(sp, S_SCHED_UPDATE);				\
		sp->refresh(sp, ep);					\
	}								\
	if (sp->special[(k)] == K_VLNEXT)				\
		(k) = getkey(sp, 0);					\
	else if (sp->special[(k)] == K_ESCAPE)				\
		return (1);						\
}

#define	GETCOUNT(sp, count) {						\
	count = 0;							\
	do {								\
		hold = count * 10 + key - '0';				\
		if (count > hold) {					\
			msgq(sp, M_ERROR,				\
			    "Number larger than %lu", ULONG_MAX);	\
			return (NULL);					\
		}							\
		count = hold;						\
		KEY(sp, key, 0);					\
	} while (isdigit(key));						\
}

/*
 * getcmd --
 *
 * The command structure for vi is less complex than ex (and don't think
 * I'm not grateful!)  The command syntax is:
 *
 *	[count] [buffer] [count] key [[motion] | [buffer] [character]]
 *
 * and there are several special cases.  The motion value is itself a vi
 * command, with the syntax:
 *
 *	[count] key [character]
 */
static int
getcmd(sp, ep, vp, ismotion)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	VICMDARG *ismotion;	/* Previous key if getting motion component. */
{
	register VIKEYS *kp;
	register u_int flags;
	u_long hold;
	int key;

	memset(&vp->vpstartzero, 0,
	    (char *)&vp->vpendzero - (char *)&vp->vpstartzero);

	KEY(sp, key, GB_MAPCOMMAND);
	if (key < 0 || key > MAXVIKEY) {
		msgq(sp, M_BELL, "%s isn't a command", charname(sp, key));
		return (1);
	}

	/* Pick up optional buffer. */
	if (key == '"') {
		KEY(sp, key, 0);
		if (!isalnum(key))
			goto ebuf;
		vp->buffer = key;
		KEY(sp, key, 0);
	} else
		vp->buffer = OOBCB;

	/*
	 * Pick up optional count.  Special case, a leading 0 is not
	 * a count, it's a command.
	 */
	if (isdigit(key) && key != '0') {
		GETCOUNT(sp, vp->count);
		vp->flags |= VC_C1SET;
	}

	/* Pick up optional buffer. */
	if (key == '"') {
		if (vp->buffer != OOBCB) {
			msgq(sp, M_ERROR,
			    "Only one buffer can be specified.");
			return (1);
		}
		KEY(sp, key, 0);
		if (!isalnum(key))
			goto ebuf;
		vp->buffer = key;
		KEY(sp, key, 0);
	}

	/* Find the command. */
	kp = vp->kp = &vikeys[vp->key = key];
	if (kp->func == NULL) {
		/* If dot, set new count/buffer, if any, and return. */
		if (key == '.') {
			if (dot.flags & VC_ISDOT) {
				if (vp->flags & VC_C1SET) {
					dot.flags |= VC_C1SET;
					dot.count = vp->count;
				}
				if (vp->buffer != OOBCB)
					dot.buffer = vp->buffer;
				*vp = dot;
				return (0);
			}
			msgq(sp, M_ERROR,
			    "No commands which set dot executed yet.");
		} else
			msgq(sp, M_ERROR,
			    "%s isn't a command", charname(sp, key));
		return (1);
	}

	flags = kp->flags;

	/* Check for illegal count. */
	if (vp->flags & VC_C1SET && !flags & V_CNT)
		goto usage;

	/* Illegal motion command. */
	if (ismotion == NULL) {
		/* Illegal buffer. */
		if (!(flags & V_OBUF) && vp->buffer != OOBCB)
			goto usage;

		/* Required buffer. */
		if (flags & V_RBUF) {
			KEY(sp, key, 0);
			if (key != '"')
				goto usage;
			KEY(sp, key, 0);
			if (key > UCHAR_MAX) {
ebuf:				msgq(sp, M_ERROR, "Invalid buffer name.");
				return (1);
			}
			vp->buffer = key;
		}

		/*
		 * Special case: '[' and ']' commands.  Doesn't the fact
		 * that the *single* characters don't mean anything but
		 * the *doubled* characters do just frost your shorts?
		 */
		if (vp->key == '[' || vp->key == ']') {
			KEY(sp, key, 0);
			if (vp->key != key)
				goto usage;
		}
		/* Special case: 'Z' command. */
		if (vp->key == 'Z') {
			KEY(sp, key, 0);
			if (vp->key != key)
				goto usage;
		}
		/* Special case: 'z' command. */
		if (vp->key == 'z') {
			KEY(sp, key, 0);
			if (isdigit(key)) {
				GETCOUNT(sp, vp->count2);
				vp->flags |= VC_C2SET;
			}
			vp->character = key;
		}
	}

	/*
	 * Commands that have motion components can be doubled to
	 * imply the current line.
	 */
	else if (ismotion->key != key && !(flags & V_MOVE)) {
usage:		msgq(sp, M_ERROR, "Usage: %s", ismotion != NULL ?
		    vikeys[ismotion->key].usage : kp->usage);
		return (1);
	}

	/* Required character. */
	if (flags & V_CHAR)
		KEY(sp, vp->character, 0);

	return (0);
}

/*
 * getmotion --
 *
 * Get resulting motion mark.
 */
static int
getmotion(sp, ep, vp, fm, tm)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm;
{
	MARK m;
	VICMDARG motion;
	u_long cnt;

	/* If '.' command, use the dot motion, else get the motion command. */
	if (vp->flags & VC_ISDOT)
		motion = dotmotion;
	else if (getcmd(sp, ep, &motion, vp))
		return (1);

	/*
	 * A count may be provided both to the command and to the motion, in
	 * which case the count is multiplicative.  For example, "3y4y" is the
	 * same as "12yy".  This count is provided to the motion command and
	 * not to the regular function. 
	 */
	cnt = motion.count = motion.flags & VC_C1SET ? motion.count : 1;
	if (vp->flags & VC_C1SET) {
		motion.count *= vp->count;
		motion.flags |= VC_C1SET;

		/*
		 * Set flags to restore the original values of the command
		 * structure so dot commands can change the count values,
		 * e.g. "2dw" "3." deletes a total of five words.
		 */
		vp->flags &= ~VC_C1SET;
		vp->flags |= VC_C1RESET;
	}

	/*
	 * Some commands can be repeated to indicate the current line.  In
	 * this case, or if the command is a "line command", set the flags
	 * appropriately.  If not a doubled command, run the function to get
	 * the resulting mark.
 	 */
	if (vp->key == motion.key) {
		vp->flags |= VC_LMODE;

		/* Set the end of the command. */
		tm->lno = ep->lno + motion.count - 1;
		tm->cno = 1;

		/*
		 * If the current line is missing, i.e. the file is empty,
		 * historic vi permitted a "cc" command to change it.
		 */
		if (!(vp->kp->flags & VC_C) &&
		    file_gline(sp, ep, tm->lno, NULL) == NULL) {
			m.lno = ep->lno;
			m.cno = ep->cno;
			v_eof(sp, ep, &m);
			return (1);
		}

		/* Set the origin of the command. */
		fm->lno = ep->lno;
		fm->cno = 0;
	} else {
		/*
		 * Motion commands change the underlying movement (*snarl*).
		 * For example, "l" is illegal at the end of a line, but "dl"
		 * is not.  Set flags so the function knows the situation.
		 */
		motion.flags |= vp->kp->flags & VC_COMMASK;

		m.lno = ep->lno;
		m.cno = ep->cno;
		if ((motion.kp->func)(sp, ep, &motion, &m, NULL, tm))
			return (1);

		/*
		 * If the underlying motion was a line motion, set the flag
		 * in the command structure.
		 */
		if (motion.kp->flags & V_LMODE)
			vp->flags |= VC_LMODE;

		/*
		 * If the motion is in a backward direction, switch the current
		 * location so that we're always moving in the same direction.
		 */
		if (tm->lno < ep->lno ||
		    tm->lno == ep->lno && tm->cno < ep->cno) {
			*fm = *tm;
			tm->lno = ep->lno;
			tm->cno = ep->cno;
		} else {
			fm->lno = ep->lno;
			fm->cno = ep->cno;
		}
	}

	/*
	 * If a dot command save motion structure.  Note that the motion count
	 * was changed above and needs to be reset.
	 */
	if (vp->kp->flags & V_DOT) {
		dotmotion = motion;
		dotmotion.count = cnt;
	}
	return (0);
}

#define	innum(c)	(isdigit(c) || strchr("abcdefABCDEF", c))

static int
getkeyword(sp, ep, kp, flags)
	SCR *sp;
	EXF *ep;
	VICMDARG *kp;
	u_int flags;
{
	register size_t beg, end;
	size_t len;
	u_char *p;

	p = file_gline(sp, ep, ep->lno, &len);
	beg = ep->cno;

	/* May not be a keyword at all. */
	if (!len ||
	    flags & V_KEYW && !inword(p[beg]) ||
	    flags & V_KEYNUM && !innum(p[beg])) {
noword:		msgq(sp, M_BELL, "Cursor not in a %s.",
		    flags & V_KEYW ? "word" : "number");
		return (1);
	}

	/* Find the beginning/end of the keyword. */
	if (beg != 0)
		if (flags & V_KEYW) {
			for (;;) {
				--beg;
				if (!inword(p[beg])) {
					++beg;
					break;
				}
				if (beg == 0)
					break;
			}
		} else {
			for (;;) {
				--beg;
				if (!innum(p[beg])) {
					if (beg > 0 && p[beg - 1] == '0' &&
					    (p[beg] == 'X' || p[beg] == 'x'))
						--beg;
					else
						++beg;
					break;
				}
				if (beg == 0)
					break;
			}

			/* Skip possible leading sign. */
			if (beg != 0 && p[beg] != '0' &&
			    (p[beg - 1] == '+' || p[beg - 1] == '-'))
				--beg;
		}

	if (flags & V_KEYW) {
		for (end = ep->cno; ++end < len && inword(p[end]););
		--end;
	} else {
		for (end = ep->cno; ++end < len;) {
			if (p[end] == 'X' || p[end] == 'x') {
				if (end != beg + 1 || p[beg] != '0')
					break;
				continue;
			}
			if (!innum(p[end]))
				break;
		}

		/* Just a sign isn't a number. */
		if (end == beg && (p[beg] == '+' || p[beg] == '-'))
			goto noword;
		--end;
	}

	/*
	 * Getting a keyword implies moving the cursor to its beginning.
	 * Refresh now.
	 */
	if (beg != ep->cno) {
		ep->cno = beg;
		sp->refresh(sp, ep);
	}

	/*
	 * XXX
	 * 8-bit clean problem.  Numeric keywords are handled using strtol(3)
	 * and friends.  This would have to be fixed in v_increment and here
	 * to not depend on a trailing NULL.
	 */
	len = (end - beg) + 2;				/* XXX */
	kp->klen = (end - beg) + 1;
	BINC(sp, kp->keyword, kp->kbuflen, len);
	memmove(kp->keyword, p + beg, kp->klen);
	kp->keyword[kp->klen] = '\0';			/* XXX */
	return (0);
}
