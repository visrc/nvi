/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vi.c,v 8.27 1993/11/04 16:17:31 bostic Exp $ (Berkeley) $Date: 1993/11/04 16:17:31 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

static int getcmd __P((SCR *, EXF *,
		VICMDARG *, VICMDARG *, VICMDARG *, int *));
static inline int
	   getcount __P((SCR *, CHAR_T *, u_long *));
static inline int
	   getkey __P((SCR *, CHAR_T *, u_int));
static int getkeyword __P((SCR *, EXF *, VICMDARG *, u_int));
static int getmotion __P((SCR *, EXF *,
		VICMDARG *, VICMDARG *, MARK *, MARK *));

/*
 * Side-effect:
 *	The dot structure can be set by the underlying vi functions,
 *	see v_Put() and v_put().
 */
#define	DOT		(&VP(sp)->sdot)
#define	DOTMOTION	(&VP(sp)->sdotmotion)

/*
 * vi --
 * 	Main vi command loop.
 */
int
vi(sp, ep)
	SCR *sp;
	EXF *ep;
{
	MARK abs, fm, tm, m;
	VICMDARG cmd, *vp;
	int comcount, eval;
	u_int flags;

	/* Start vi. */
	if (v_init(sp, ep))
		return (1);

	/* Paint the screen. */
	if (sp->s_refresh(sp, ep))
		return (v_end(sp));

	/* Command initialization. */
	memset(&cmd, 0, sizeof(VICMDARG));

	/* Edited as it can be. */
	F_SET(sp->frp, FR_EDITED);

	for (eval = 0, vp = &cmd;;) {
		if (!TERM_KEY_MORE(sp) && log_cursor(sp, ep))
			goto err;

		/*
		 * We get a command, which may or may not have an associated
		 * motion.  If it does, we get it too, calling its underlying
		 * function to get the resulting mark.  We then call the
		 * command setting the cursor to the resulting mark.
		 */
		if (getcmd(sp, ep, DOT, vp, NULL, &comcount))
			goto err;

		/*
		 * Historical practice: if a dot command gets a new count,
		 * any motion component goes away, i.e. "d3w2." deletes a
		 * total of 5 words.
		 */
		if (F_ISSET(vp, VC_ISDOT) && comcount)
			DOTMOTION->count = 1;

		flags = vp->kp->flags;

		/* Get any associated keyword. */
		if (LF_ISSET(V_KEYNUM | V_KEYW) &&
		    getkeyword(sp, ep, vp, flags))
			goto err;

		/* If a non-relative movement, copy the future absolute mark. */
		if (LF_ISSET(V_ABS)) {
			abs.lno = sp->lno;
			abs.cno = sp->cno;
		}

		/*
		 * Do any required motion; getmotion sets the from MARK
		 * and the line mode flag.
		 */
		if (LF_ISSET(V_MOTION)) {
			if (getmotion(sp, ep, DOTMOTION, vp, &fm, &tm))
				goto err;
		} else {
			/*
			 * Set everything to the current cursor position.
			 * Line commands (ex: Y) default to the current line.
			 */
			tm.lno = fm.lno = sp->lno;
			tm.cno = fm.cno = sp->cno;

			/*
			 * Set line mode flag, for example, "yy".
			 *
			 * If a count is set, we set the to MARK here relative
			 * to the cursor/from MARK.  This is done for commands
			 * that take both counts and motions, i.e. "4yy" and
			 * "y%" -- there's no way the command can known which
			 * the user did, so we have to do it here.  There are
			 * other commands that are line mode commands and take
			 * counts ("#G", "#H") and for which this calculation
			 * is either meaningless or wrong.  Each command must
			 * do its own validity checking of the value.
			 */
			if (F_ISSET(vp->kp, V_LMODE)) {
				F_SET(vp, VC_LMODE);
				if (F_ISSET(vp, VC_C1SET)) {
					tm.lno = sp->lno + vp->count - 1;
					tm.cno = sp->cno;
				}
			}
		}

		/* Increment the command count. */
		++sp->ccnt;

		/*
		 * Call the function.  Set the return cursor to the current
		 * cursor position first -- the underlying routines don't
		 * bother to do the work if it doesn't move.
		 */
		m.lno = sp->lno;
		m.cno = sp->cno;
		if ((vp->kp->func)(sp, ep, vp, &fm, &tm, &m))
			goto err;
#ifdef DEBUG
		/* Make sure no function left the temporary space locked. */
		if (F_ISSET(sp->gp, G_TMP_INUSE)) {
			msgq(sp, M_ERR,
			    "Error: vi: temporary buffer not released.");
			return (1);
		}
#endif
		/*
		 * If that command took us out of vi or changed the screen,
		 * then exit the loop without further action.
		 */
		if (!F_ISSET(sp, S_MODE_VI) || F_ISSET(sp, S_MAJOR_CHANGE))
			break;
		
		/* Set the absolute mark. */
		if (LF_ISSET(V_ABS) && mark_set(sp, ep, ABSMARK1, &abs, 1))
			goto err;

		/* Set the dot command structure. */
		if (LF_ISSET(V_DOT)) {
			*DOT = cmd;
			F_SET(DOT, VC_ISDOT);
			/*
			 * If a count was supplied for both the command and
			 * its motion, the count was used only for the motion.
			 * Turn the count back on for the dot structure.
			 */
			if (F_ISSET(vp, VC_C1RESET))
				F_SET(DOT, VC_C1SET);
		}

		/*
		 * Some vi row movements are "attracted" to the last position
		 * set, i.e. the V_RCM commands are moths to the V_RCM_SET
		 * commands' candle.  It's broken into two parts.  Here we deal
		 * with the command flags.  In sp->relative(), we deal with the
		 * screen flags.  If the movement is to the EOL the vi command
		 * handles it.  If it's to the beginning, we handle it here.
		 *
		 * Does this totally violate the screen and editor layering?
		 * You betcha.  As they say, if you think you understand it,
		 * you don't.
		 */
		switch (LF_ISSET(V_RCM | V_RCM_SETFNB |
		    V_RCM_SETLAST | V_RCM_SETLFNB | V_RCM_SETNNB)) {
		case 0:
			break;
		case V_RCM:
			m.cno = sp->s_relative(sp, ep, m.lno);
			break;
		case V_RCM_SETLAST:
			sp->rcmflags = RCM_LAST;
			break;
		case V_RCM_SETLFNB:
			if (fm.lno != m.lno) {
				if (nonblank(sp, ep, m.lno, &m.cno))
					goto err;
				sp->rcmflags = RCM_FNB;
			}
			break;
		case V_RCM_SETFNB:
			m.cno = 0;
			/* FALLTHROUGH */
		case V_RCM_SETNNB:
			if (nonblank(sp, ep, m.lno, &m.cno))
				goto err;
			sp->rcmflags = RCM_FNB;
			break;
		default:
			abort();
		}
			
		/* Update the cursor. */
		sp->lno = m.lno;
		sp->cno = m.cno;

		if (!TERM_KEY_MORE(sp)) {
			(void)msg_rpt(sp, 1);

			if (0)
err:				TERM_KEY_FLUSH(sp);
		}

		/* Refresh the screen. */
		if (sp->s_refresh(sp, ep)) {
			eval = 1;
			break;
		}

		/* Set the new favorite position. */
		if (LF_ISSET(V_RCM_SET)) {
			sp->rcmflags = 0;
			sp->rcm = sp->sc_col;
		}
	}

	return (v_end(sp) || eval);
}

#define	KEY(key) {							\
	if (getkey(sp, &key, 0))					\
		return (1);						\
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
getcmd(sp, ep, dp, vp, ismotion, comcountp)
	SCR *sp;
	EXF *ep;
	VICMDARG *dp, *vp;
	VICMDARG *ismotion;	/* Previous key if getting motion component. */
	int *comcountp;
{
	VIKEYS const *kp;
	u_int flags;
	CHAR_T key;

	/* Clean up the command structure. */
	memset(&vp->vp_startzero, 0,
	    (char *)&vp->vp_endzero - (char *)&vp->vp_startzero);

	/* An escape bells the user only if already in command mode. */
	if (ismotion == NULL) {
		if (getkey(sp, &key, TXT_MAPCOMMAND)) {
			if (sp->special[key] == K_ESCAPE)
				msgq(sp, M_BERR, "Already in command mode");
			return (1);
		}
	} else
		KEY(key);
	if (key > MAXVIKEY) {
		msgq(sp, M_BERR, "%s isn't a vi command", charname(sp, key));
		return (1);
	}

	/* Pick up optional buffer. */
	if (key == '"') {
		KEY(key);
		if (!isalnum(key))
			goto ebuf;
		vp->buffer = key;
		F_SET(vp, VC_BUFFER);
		KEY(key);
	}

	/*
	 * Pick up optional count, where a leading 0 is not a count,
	 * it's a command.
	 */
	if (isdigit(key) && key != '0') {
		if (getcount(sp, &key, &vp->count))
			return (1);
		F_SET(vp, VC_C1SET);
		*comcountp = 1;
	} else
		*comcountp = 0;

	/* Pick up optional buffer. */
	if (key == '"') {
		if (F_ISSET(vp, VC_BUFFER)) {
			msgq(sp, M_ERR,
			    "Only one buffer can be specified.");
			return (1);
		}
		KEY(key);
		if (!isalnum(key))
			goto ebuf;
		vp->buffer = key;
		F_SET(vp, VC_BUFFER);
		KEY(key);
	}

	/*
	 * Find the command.  The only legal command with no underlying
	 * function is dot.
	 */
	kp = vp->kp = &vikeys[vp->key = key];
	if (kp->func == NULL) {
		if (key != '.') {
			msgq(sp, M_ERR,
			    "%s isn't a command", charname(sp, key));
			return (1);
		}

		/* If called for a motion command, stop now. */
		if (dp == NULL)
			goto usage;

		/* A repeatable command must have been executed. */
		if (!F_ISSET(dp, VC_ISDOT)) {
			msgq(sp, M_ERR, "No command to repeat.");
			return (1);
		}

		/* Set new count/buffer, if any, and return. */
		if (F_ISSET(vp, VC_C1SET)) {
			F_SET(dp, VC_C1SET);
			dp->count = vp->count;
		}
		if (F_ISSET(vp, VC_BUFFER))
			dp->buffer = vp->buffer;
		*vp = *dp;
		return (0);
	}

	flags = kp->flags;

	/* Check for illegal count. */
	if (F_ISSET(vp, VC_C1SET) && !LF_ISSET(V_CNT))
		goto usage;

	/* Illegal motion command. */
	if (ismotion == NULL) {
		/* Illegal buffer. */
		if (!LF_ISSET(V_OBUF) && F_ISSET(vp, VC_BUFFER))
			goto usage;

		/* Required buffer. */
		if (LF_ISSET(V_RBUF)) {
			KEY(key);
			if (key > UCHAR_MAX) {
ebuf:				msgq(sp, M_ERR, "Invalid buffer name.");
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
			KEY(key);
			if (vp->key != key)
				goto usage;
		}
		/* Special case: 'Z' command. */
		if (vp->key == 'Z') {
			KEY(key);
			if (vp->key != key)
				goto usage;
		}
		/* Special case: 'z' command. */
		if (vp->key == 'z') {
			KEY(key);
			if (isdigit(key)) {
				if (getcount(sp, &key, &vp->count2))
					return (1);
				F_SET(vp, VC_C2SET);
			}
			vp->character = key;
		}
	}

	/*
	 * Commands that have motion components can be doubled to
	 * imply the current line.
	 */
	else if (ismotion->key != key && !LF_ISSET(V_MOVE)) {
usage:		msgq(sp, M_ERR, "Usage: %s", ismotion != NULL ?
		    vikeys[ismotion->key].usage : kp->usage);
		return (1);
	}

	/* Required character. */
	if (LF_ISSET(V_CHAR))
		KEY(vp->character);

	return (0);
}

/*
 * getmotion --
 *
 * Get resulting motion mark.
 */
static int
getmotion(sp, ep, dm, vp, fm, tm)
	SCR *sp;
	EXF *ep;
	VICMDARG *dm, *vp;
	MARK *fm, *tm;
{
	MARK m;
	VICMDARG motion;
	u_long cnt;
	int notused;

	/* If '.' command, use the dot motion, else get the motion command. */
	if (F_ISSET(vp, VC_ISDOT)) {
		motion = *dm;
		F_SET(&motion, VC_ISDOT);
	} else if (getcmd(sp, ep, NULL, &motion, vp, &notused))
		return (1);

	/*
	 * A count may be provided both to the command and to the motion, in
	 * which case the count is multiplicative.  For example, "3y4y" is the
	 * same as "12yy".  This count is provided to the motion command and
	 * not to the regular function. 
	 */
	cnt = motion.count = F_ISSET(&motion, VC_C1SET) ? motion.count : 1;
	if (F_ISSET(vp, VC_C1SET)) {
		motion.count *= vp->count;
		F_SET(&motion, VC_C1SET);

		/*
		 * Set flags to restore the original values of the command
		 * structure so dot commands can change the count values,
		 * e.g. "2dw" "3." deletes a total of five words.
		 */
		F_CLR(vp, VC_C1SET);
		F_SET(vp, VC_C1RESET);
	}

	/*
	 * Some commands can be repeated to indicate the current line.  In
	 * this case, or if the command is a "line command", set the flags
	 * appropriately.  If not a doubled command, run the function to get
	 * the resulting mark.
 	 */
	if (vp->key == motion.key) {
		F_SET(vp, VC_LMODE);

		/*
		 * Set the end of the command; the column is after the line.
		 *
		 * If the current line is missing, i.e. the file is empty,
		 * historic vi permitted a "cc" or "!!" command to change
		 * insert text.
		 */
		tm->lno = sp->lno + motion.count - 1;
		if (file_gline(sp, ep, tm->lno, &tm->cno) == NULL) {
			if (tm->lno != 1 || vp->key != 'c' && vp->key != '!') {
				m.lno = sp->lno;
				m.cno = sp->cno;
				v_eof(sp, ep, &m);
				return (1);
			}
			tm->cno = 0;
		}

		/* Set the origin of the command. */
		fm->lno = sp->lno;
		fm->cno = 0;
	} else {
		/*
		 * Motion commands change the underlying movement (*snarl*).
		 * For example, "l" is illegal at the end of a line, but "dl"
		 * is not.  Set flags so the function knows the situation.
		 */
		F_SET(&motion, vp->kp->flags & VC_COMMASK);

		/*
		 * Everything starts at the current position.  This permits
		 * commands like 'j' and 'k', that are line oriented motions
		 * and have special cursor suck semantics when are used as
		 * standalone commands, to ignore column positioning.
		 */
		fm->lno = tm->lno = m.lno = sp->lno;
		fm->cno = tm->cno = m.cno = sp->cno;
		if ((motion.kp->func)(sp, ep, &motion, &m, NULL, tm))
			return (1);

		/*
		 * If the underlying motion was a line motion, set the flag
		 * in the command structure.  Underlying commands can also
		 * flag the movement as a line motion (see v_sentence).
		 */
		if (F_ISSET(motion.kp, V_LMODE) || F_ISSET(&motion, VC_LMODE))
			F_SET(vp, VC_LMODE);

		/*
		 * If the motion is in a backward direction, switch the current
		 * location so that we're always moving in the same direction.
		 *
		 * This is also the reason for the fact that "yj" doesn't move
		 * the cursor but "yk" does -- when the from MARK is changed,
		 * it's the same as changing the underlying cursor position.
		 *
		 * XXX
		 * I'm not sure that using the cursor here is correct, it might
		 * be cleaner to use the from MARK.  Whichever is used, it has
		 * to be reevaluated.  Some routines (see v_match) modify the
		 * cursor as well as the return MARK, so that if a movement is
		 * in the reverse direction they can get the one-past-the-place
		 * semantics.
		 */
		if (tm->lno < sp->lno ||
		    tm->lno == sp->lno && tm->cno < sp->cno) {
			*fm = *tm;
			tm->lno = sp->lno;
			tm->cno = sp->cno;
		} else {
			fm->lno = sp->lno;
			fm->cno = sp->cno;
		}
	}

	/*
	 * If the command sets dot, save the motion structure.  The
	 * motion count was changed above and needs to be reset, that's
	 * why this is done here, and not in the calling routine.
	 */
	if (F_ISSET(vp->kp, V_DOT)) {
		*dm = motion;
		dm->count = cnt;
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
	char *p;

	p = file_gline(sp, ep, sp->lno, &len);
	beg = sp->cno;

	/* May not be a keyword at all. */
	if (p == NULL || len == 0 ||
	    LF_ISSET(V_KEYW) && !inword(p[beg]) ||
	    LF_ISSET(V_KEYNUM) && !innum(p[beg]) &&
	    p[beg] != '-' && p[beg] != '+') {
noword:		msgq(sp, M_BERR, "Cursor not in a %s",
		    LF_ISSET(V_KEYW) ? "word" : "number");
		return (1);
	}

	/* Find the beginning/end of the keyword. */
	if (beg != 0)
		if (LF_ISSET(V_KEYW)) {
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

	if (LF_ISSET(V_KEYW)) {
		for (end = sp->cno; ++end < len && inword(p[end]););
		--end;
	} else {
		for (end = sp->cno; ++end < len;) {
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
	if (beg != sp->cno) {
		sp->cno = beg;
		sp->s_refresh(sp, ep);
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

/*
 * getkey --
 *	Return the next key.
 */
static inline int
getkey(sp, keyp, flags)
	SCR *sp;
	CHAR_T *keyp;
	u_int flags;
{
	switch (term_key(sp, keyp, flags)) {
	case INP_OK:
		break;
	case INP_EOF:
		F_SET(sp, S_EXIT_FORCE);
		/* FALLTHROUGH */
	case INP_ERR:
		return (1);
	}
	return (sp->special[*keyp] == K_ESCAPE);
}

/*
 * getcount --
 *	Return the next count.
 *
 * !!!
 * Note that the TXT_MAPCOMMAND flag is set during this routine.  The reason
 * is that we're going to the next non-digit, and we want that character to
 * have been been mapped since we push it back on the stack.  The problem we
 * could have is if a user has mapped a digit into something.  Historical vi
 * was a bit odd in this area.  It permitted the mapping of 1-9 (the digit 0
 * was a special case as it didn't (and doesn't) indicate the start of a count)
 * but then ignored those mappings.  The result of all this, is that, in this
 * implementation we don't ignore the mappings.
 */
static inline int
getcount(sp, keyp, countp)
	SCR *sp;
	CHAR_T *keyp;
	u_long *countp;
{
	u_long count, tc;

	count = tc = 0;
	do {
		/* Assume that overflow results in a smaller number. */
		tc = count * 10 + *keyp - '0';
		if (count > tc) {
			/* Toss to the next non-digit. */
			do {
				if (getkey(sp, keyp, TXT_MAPCOMMAND))
					return (1);
			} while (isdigit(*keyp));
			(void)term_push(sp, sp->gp->key, keyp, 1);
			msgq(sp, M_ERR,
			    "Number larger than %lu", ULONG_MAX);
			return (1);
		}
		count = tc;
		if (getkey(sp, keyp, TXT_MAPCOMMAND))
			return (1);
	} while (isdigit(*keyp));
	*countp = count;
	return (0);
}
