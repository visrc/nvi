/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vi.c,v 5.20 1992/10/10 14:06:08 bostic Exp $ (Berkeley) $Date: 1992/10/10 14:06:08 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include "vi.h"
#include "vcmd.h"
#include "cut.h"
#include "options.h"
#include "screen.h"
#include "extern.h"

static int getcmd __P((VICMDARG *, VICMDARG *));
static int getcount __P((int, u_long *, int *));
static int getkeyword __P((VICMDARG *, u_int));
static int getmotion __P((VICMDARG *, MARK *, MARK *));

static VICMDARG cmd, dot, dotmotion;
int needexerase;

/*
 * vi --
 * 	Main vi command loop.
 */
void
vi()
{
	register VICMDARG *vp;
	MARK fm, tm, m;
	u_int flags;
	int erase;

	scr_init();
	scr_ref();
	refresh();
	for (;;) {
		/* Report any changes from the previous command. */
		if (rptlines) {
			if (LVAL(O_REPORT) && rptlines >= LVAL(O_REPORT)) {
				msg("%ld line%s %s", rptlines,
				    (rptlines == 1 ? "" : "s"), rptlabel);
			}
			rptlines = 0;
		}

		/*
		 * If ex left a message on the screen, leave the screen alone
		 * until the next keystroke.  If a vi message is waiting,
		 * display it.
		 *
		 * XXX
		 * If key already waiting, and it's not a key requiring
		 * a motion component, should skip repaint.
		 */
err:		if (msgcnt) {
			msg_vflush();
			refresh();
		} else if (!needexerase) {
			scr_modeline(curf);
			refresh();
		}

		/*
		 * We get a command, which may or may not have an associated
		 * motion.  If it does, we get it too, calling its underlying
		 * function to get the resulting mark.  We then call the
		 * command setting the cursor to the resulting mark.
		 */
		vp = &cmd;
		if (getcmd(vp, NULL))
			goto err;

		flags = vp->kp->flags;

		/* V/v commands require movement commands. */
		if (V_from && !(flags & V_MOVE)) {
			bell();
			msg("%s: not a movement command", vp->key);
			goto err;
		}

		/* Get any associated keyword. */
		if (flags & (V_KEYNUM|V_KEYW) && getkeyword(vp, flags))
			goto err;

		/* If a non-relative movement, set the '' mark. */
		if (flags & V_ABS) {
			m.lno = curf->lno;
			m.cno = curf->cno;
			SETABSMARK(&m);
		}

		/* Do any required motion. */
		if (flags & V_MOTION) {
			if (getmotion(vp, &fm, &tm)) {
				bell();
				goto err;
			}
		} else {
			fm.lno = curf->lno;
			fm.cno = curf->cno;
		}

		/* Call the function, update the cursor. */
		if ((vp->kp->func)(vp, &fm, &tm, &m))
			goto err;

		/*
		 * If that command took us out of vi mode, then exit
		 * the loop without further action.
		 */
		if (mode != MODE_VI)
			break;

		/*
		 * Some vi row movements are "attracted" to the last set
		 * position, i.e. the V_RCM commands are moths to the
		 * V_RCM_SET commands' candle.  Does this totally violate
		 * the screen and editor layering?  You betcha.
		 */
		if (flags & V_RCM)
			m.cno = scr_relative(curf, m.lno);
		else if (flags & V_RCM_SETFNB) {
			/* Hack -- do the movement here, too. */
			if (nonblank(m.lno, &m.cno))
				goto err;
			curf->rcmflags = RCM_FNB;
		}
		else if (flags & V_RCM_SETLAST)
			curf->rcmflags = RCM_LAST;
			
		curf->lno = m.lno;
		curf->cno = m.cno;

		/* Update the screen. */
		scr_cchange(curf);

		if (flags & V_RCM_SET) {
			curf->rcmflags = 0;
			curf->rcm = curf->scno;
		}

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
	}
	scr_end();
}

#define	KEY(k) {							\
	(k) = getkey(WHEN_VICMD);					\
	if (needexerase) {						\
		scr_modeline(curf);					\
		refresh();						\
	}								\
	if ((k) < 0 || (k) > MAXVIKEY) {				\
		bell();							\
		return (NULL);						\
	}								\
	if ((k) == ESCAPE) {						\
		if (!ismotion)						\
			bell();						\
		return (NULL);						\
	}								\
}

#define	GETCOUNT(count) {						\
	count = 0;							\
	do {								\
		hold = count * 10 + key - '0';				\
		if (count > hold) {					\
			msg("Number larger than %lu\n", ULONG_MAX);	\
			return (NULL);					\
		}							\
		count = hold;						\
		KEY(key);						\
	} while (isdigit(key));						\
}

/*
 * getcmd --
 *
 * The command structure for vi is less complex than ex (and don't think
 * I'm not grateful!)  The command syntax is:
 *
 *	[buffer] [count] key [[motion] | [buffer] [character]]
 *
 * and there are several special cases.  The motion value is itself a vi
 * command, with the syntax:
 *
 *	[count] key [character]
 */
static int
getcmd(vp, ismotion)
	VICMDARG *vp;
	VICMDARG *ismotion;	/* Previous key if getting motion component. */
{
	register VIKEYS *kp;
	register u_long count;
	register u_int flags;
	u_long hold;
	int key;

	bzero(&vp->vpstartzero,
	    (char *)&vp->vpendzero - (char *)&vp->vpstartzero);

	KEY(key);

	/* Pick up optional buffer. */
	if (key == '"') {
		KEY(key);
		if (!isalnum(key))
			goto ebuf;
		vp->buffer = key;
		KEY(key);
	} else
		vp->buffer = OOBCB;
	/*
	 * Pick up optional count.  Special case, a leading 0 is not
	 * a count, it's a command.
	 */
	if (isdigit(key) && key != '0') {
		GETCOUNT(vp->count);
		vp->flags |= VC_C1SET;
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
			msg("No commands which set dot executed yet.");
		} else
			msg("%s isn't a command.", charname(key));
		bell();
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
			KEY(key);
			if (key != '"')
				goto usage;
			KEY(key);
			if (vp->buffer > UCHAR_MAX) {
ebuf:				bell();
				msg("Invalid buffer name.");
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
				GETCOUNT(vp->count2);
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
usage:		bell();
		msg("Usage: %s", ismotion != NULL ?
		    vikeys[ismotion->key].usage : kp->usage);
		return (1);
	}

	/* Required character. */
	if (flags & V_CHAR)
		KEY(vp->character);

	return (0);
}

/*
 * getmotion --
 *
 * Get resulting motion mark.
 */
static int
getmotion(vp, fm, tm)
	VICMDARG *vp;
	MARK *fm, *tm;
{
	MARK m;
	VICMDARG motion;
	u_long cnt;

	/* If '.' command, use the dot motion, else get the motion command. */
	if (vp->flags & VC_ISDOT)
		motion = dotmotion;
	else if (getcmd(&motion, vp))
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
	 * Motion commands can be doubled to indicate the current line.  In
	 * this case, or if the command is a "line command", set the flags
	 * appropriately.  If not a doubled command, run the function to get
	 * the resulting mark.
 	 */
	if (vp->key == motion.key) {
		vp->flags |= VC_LMODE;

		/* Set the end of the command. */
		tm->lno = curf->lno + motion.count - 1;
		tm->cno = 1;
		if (file_gline(curf, tm->lno, NULL) == NULL) {
			m.lno = curf->lno;
			m.cno = curf->cno;
			v_eof(&m);
			return (1);
		}

		/* Set the origin of the command. */
		fm->lno = curf->lno;
		fm->cno = 0;
	} else {
		/*
		 * Motion commands change the underlying movement (*snarl*).
		 * For example, "l" is illegal at the end of a line, but "dl"
		 * is not.  Set flags so the function knows the situation.
		 */
		motion.flags |= vp->kp->flags & VC_COMMASK;

		m.lno = curf->lno;
		m.cno = curf->cno;
		if ((motion.kp->func)(&motion, &m, NULL, tm))
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
		if (tm->lno < curf->lno ||
		    tm->lno == curf->lno && tm->cno < curf->cno) {
			*fm = *tm;
			tm->lno = curf->lno;
			tm->cno = curf->cno;
		} else {
			fm->lno = curf->lno;
			fm->cno = curf->cno;
		}
	}

	/*
	 * If a dot command save motion structure.  Note that the motion count
	 * was changed above and needs to be reset.
	 */
	if (vp->flags & V_DOT) {
		dotmotion = motion;
		dotmotion.count = cnt;
	}
	return (0);
}

#define	innum(c)	(isdigit(c) || index("abcdefABCDEF", c))

static int
getkeyword(kp, flags)
	VICMDARG *kp;
	u_int flags;
{
	register size_t beg, end, key;
	size_t len;
	u_char *p;

	p = file_gline(curf, curf->lno, &len);
	beg = curf->cno;

	/* May not be a keyword at all. */
	if (!len ||
	    flags & V_KEYW && !inword(p[beg]) ||
	    flags & V_KEYNUM && !innum(p[beg])) {
noword:		bell();
		if (ISSET(O_VERBOSE))
			msg("Cursor not in a %s.",
			    flags & V_KEYW ? "word" : "number");
		return (1);
	}

	/* Find the beginning/end of the keyword. */
	if (beg != 0) {
		if (flags & V_KEYW) {
			do {
				--beg;
			} while (inword(p[beg]) && beg > 0);
		} else {
			do {
				--beg;
			} while (innum(p[beg]) && beg > 0);

			/* Skip possible leading sign. */
			if (beg != 0 && p[beg] == '+' || p[beg] == '-')
				--beg;
		}
		if (beg != 0)
			++beg;
	}

	if (flags & V_KEYW) {
		for (end = curf->cno; ++end < len && inword(p[end]););
		--end;
	} else {
		for (end = curf->cno; ++end < len && innum(p[end]););

		/* Just a sign isn't a number. */
		if (end == beg && (p[beg] == '+' || p[beg] == '-'))
			goto noword;
		--end;
	}

	/*
	 * Getting a keyword implies moving the cursor to its beginning.
	 * Refresh now.
	 */
	if (beg != curf->cno) {
		curf->cno = beg;
		scr_cchange(curf);
		refresh();
	}

	/*
	 * XXX
	 * 8-bit clean problem.  Numeric keywords are handled using strtol(3)
	 * and friends.  This would have to be fixed in v_increment and here
	 * to not depend on a trailing NULL.
	 */
	len = (end - beg) + 2;				/* XXX */
	kp->klen = (end - beg) + 1;
	if (kp->kbuflen <= len) {
		kp->kbuflen += len + 50;
		if ((kp->keyword =
		    realloc(kp->keyword, kp->kbuflen)) == NULL) {
			bell();
			msg("Keyword too large: %s", strerror(errno));
			return (1);
		}
	}
	bcopy(p + beg, kp->keyword, kp->klen);
	kp->keyword[len - 1] = '\0';			/* XXX */
	return (0);
}
