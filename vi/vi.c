/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vi.c,v 8.31 1993/11/12 16:43:26 bostic Exp $ (Berkeley) $Date: 1993/11/12 16:43:26 $";
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
	   getcount __P((SCR *, ARG_CHAR_T, u_long *));
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
	u_int flags;
	int comcount, eval;

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
		if (!TERM_MORE(sp->gp->key) && log_cursor(sp, ep))
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

		/* Get any associated keyword. */
		flags = vp->kp->flags;
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

		if (!TERM_MORE(sp->gp->key)) {
			(void)msg_rpt(sp, 1);

			if (0)
err:				TERM_FLUSH(sp->gp->key);
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

#define	KEY(key, map) {							\
	if (getkey(sp, &key, map))					\
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

	/* Refresh the command structure. */
	memset(&vp->vp_startzero, 0,
	    (char *)&vp->vp_endzero - (char *)&vp->vp_startzero);

	if (getkey(sp, &key, TXT_MAPCOMMAND))
		return (1);

	/* An escape bells the user only if already in command mode. */
	if (sp->special[key] == K_ESCAPE) {
		if (ismotion == NULL)
			msgq(sp, M_BERR, "Already in command mode");
		return (1);
	}
	if (key > MAXVIKEY) {
		msgq(sp, M_BERR, "%s isn't a vi command", charname(sp, key));
		return (1);
	}

	/* Pick up optional buffer. */
	if (key == '"') {
		KEY(vp->buffer, 0);
		F_SET(vp, VC_BUFFER);
		KEY(key, TXT_MAPCOMMAND);
	}

	/*
	 * Pick up optional count, where a leading 0 is not a count,
	 * it's a command.
	 */
	if (isdigit(key) && key != '0') {
		if (getcount(sp, key, &vp->count))
			return (1);
		F_SET(vp, VC_C1SET);
		*comcountp = 1;
		KEY(key, TXT_MAPCOMMAND);
	} else
		*comcountp = 0;

	/* Pick up optional buffer. */
	if (key == '"') {
		if (F_ISSET(vp, VC_BUFFER)) {
			msgq(sp, M_ERR, "Only one buffer can be specified.");
			return (1);
		}
		KEY(vp->buffer, 0);
		F_SET(vp, VC_BUFFER);
		KEY(key, TXT_MAPCOMMAND);
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
		if (LF_ISSET(V_RBUF))
			KEY(vp->buffer, 0);

		/*
		 * Special case: '[', ']' and 'Z' commands.  Doesn't the
		 * fact that the *single* characters don't mean anything
		 * but the *doubled* characters do just frost your shorts?
		 */
		if (vp->key == '[' || vp->key == ']' || vp->key == 'Z') {
			KEY(key, TXT_MAPCOMMAND);
			if (vp->key != key)
				goto usage;
		}
		/* Special case: 'z' command. */
		if (vp->key == 'z') {
			KEY(vp->character, 0);
			if (isdigit(vp->character)) {
				if (getcount(sp, key, &vp->count2))
					return (1);
				F_SET(vp, VC_C2SET);
			}
			KEY(vp->character, 0);
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
		KEY(vp->character, 0);

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
		 * historic vi permitted a "cc" or "!!" command to insert
		 * text.
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
		 * and have special cursor suck semantics when they are used
		 * as standalone commands, to ignore column positioning.
		 */
		fm->lno = tm->lno = sp->lno;
		fm->cno = tm->cno = sp->cno;
		if ((motion.kp->func)(sp, ep, &motion, fm, NULL, tm))
			return (1);

		/*
		 * If the underlying motion was a line motion, set the flag
		 * in the command structure.  Underlying commands can also
		 * flag the movement as a line motion (see v_sentence).
		 */
		if (F_ISSET(motion.kp, V_LMODE) || F_ISSET(&motion, VC_LMODE))
			F_SET(vp, VC_LMODE);

		/*
		 * If the motion is in the reverse direction, switch the from
		 * and to MARK's so that it's always in a forward direction.
		 * Because the motion is always from the from MARK to, but not
		 * including, the to MARK, the function may have modified the
		 * from MARK, so that it gets the one-past-the-place semantics
		 * we use; see v_match() for an example.
		 *
		 * !!!
		 * Historic vi changed the cursor as part of this, which made
		 * no sense.  For example, "yj" would move the cursor but "yk"
		 * would not.
		 */
		if (tm->lno < fm->lno ||
		    tm->lno == fm->lno && tm->cno < fm->cno) {
			m = *fm;
			*fm = *tm;
			*tm = m;
#ifdef HISTORIC_MOVE_TO_START_OF_BLOCK
			sp->lno = fm->lno;
			sp->cno = fm->cno;
#endif
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

/*
 * getkeyword --
 *	Get the "word" the cursor is on.
 */
static int
getkeyword(sp, ep, kp, flags)
	SCR *sp;
	EXF *ep;
	VICMDARG *kp;
	u_int flags;
{
	register size_t beg, end;
	recno_t lno;
	size_t len;
	char *p;

	if ((p = file_gline(sp, ep, sp->lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			v_eof(sp, ep, NULL);
		else
			GETLINE_ERR(sp, sp->lno);
		return (1);
	}
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
 * getcount --
 *	Return the next count.
 */
static inline int
getcount(sp, fkey, countp)
	SCR *sp;
	ARG_CHAR_T fkey;
	u_long *countp;
{
	u_long count, tc;
	CHAR_T key;

	key = fkey;
	count = tc = 0;
	do {
		/* Assume that overflow results in a smaller number. */
		tc = count * 10 + key - '0';
		if (count > tc) {
			/* Toss to the next non-digit. */
			do {
				if (getkey(sp, &key,
				    TXT_MAPCOMMAND | TXT_MAPNODIGIT))
					return (1);
			} while (isdigit(key));
			msgq(sp, M_ERR, "Number larger than %lu", ULONG_MAX);
			return (1);
		}
		count = tc;
		if (getkey(sp, &key, TXT_MAPCOMMAND | TXT_MAPNODIGIT))
			return (1);
	} while (isdigit(key));
	*countp = count;
	return (0);
}

/*
 * getkey --
 *	Return the next key.
 */
static inline int
getkey(sp, keyp, map)
	SCR *sp;
	CHAR_T *keyp;
	u_int map;
{
	switch (term_key(sp, keyp, map)) {
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
