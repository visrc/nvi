/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vi.c,v 5.14 1992/05/23 08:50:20 bostic Exp $ (Berkeley) $Date: 1992/05/23 08:50:20 $";
#endif /* not lint */

#include <sys/types.h>
#include <termios.h>
#include <errno.h>
#include <limits.h>
#include <curses.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "vi.h"
#include "vcmd.h"
#include "cut.h"
#include "options.h"
#include "extern.h"

static int getcmd __P((VICMDARG *, VICMDARG *));
static int getcount __P((int, u_long *, int *));
static int getkeyword __P((VICMDARG *, u_int));

static VICMDARG dot, dotmotion;

void
vi()
{
	register VICMDARG *mp, *vp;
	register int key;
	VICMDARG cmd, motion;
	MARK fm;
	u_long cnt, oldy, oldx;
	u_int vc_c1set, flags;
	int erase;

	scr_ref();
	wrefresh(curscr);
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
		 * Flush any messages and repaint the screen.
		 *
		 * XXX
		 * If key already waiting, and it's not a key requiring
		 * a motion component, should skip repaint.
		 */
err:		if (msgcnt) {
			erase = 1;
			msg_vflush();
		} else
			erase = 0;
		refresh();
		if (erase)
			scr_modeline();

		/*
		 * We get a command, which may or may not have an associated
		 * motion.  If it does, we get it too, calling its underlying
		 * function to get the resulting mark.  We then call the
		 * command setting the cursor to the resulting mark.
		 */
		vp = &cmd;
		mp = NULL;
		if (getcmd(vp, NULL))
			goto err;

		flags = vp->kp->flags;

		/* V/v commands require movement commands. */
		if (V_from && !(flags & V_MOVE)) {
			bell();
			msg("%s: not a movement command", vp->key);
			goto err;
		}

		/* If the command requires motion, get the motion command. */
		if (flags & V_MOTION) {
			if (getcmd(&motion, vp)) {
				bell();
				goto err;
			}
			mp = &motion;
		}

		/* Get any associated keyword. */
		if (flags & (V_KEYNUM|V_KEYW) && getkeyword(vp, flags))
			goto err;

		/*
	 	 * Get resulting motion mark.  Motion commands can be doubled
		 * to indicate the current line.  In this case, or if the
		 * command is a "line command", set the flags to indicate
		 * this.  If not a doubled command, run the function to get
		 * the resulting mark.  Count may be provided both to the
		 * command and to the motion, in which case the count is
		 * multiplicative.  For example, "3y4y" is the same as "12yy".
		 * This count is provided to the motion command, not to the
		 * regular function.  Make sure we restore the original values
		 * of the command structure so dot commands can change the
		 * count values, e.g. "2dw" "3." deletes a total of five words.
		 */
		if (mp != NULL) {
			cnt = mp->count = mp->flags & VC_C1SET ? mp->count : 1;
			if (vp->flags & VC_C1SET) {
				mp->count *= vp->count;
				mp->flags |= VC_C1SET;
				vp->flags &= ~VC_C1SET;
				vc_c1set = VC_C1SET;
			} else
				vc_c1set = 0;
			if (vp->key == mp->key) {
				vp->flags |= VC_LMODE;
				vp->motion.lno = cursor.lno + mp->count - 1;
				vp->motion.cno = 1;
				fm.lno = cursor.lno;
				fm.cno = 0;
				if (file_gline(curf,
				    vp->motion.lno, NULL) == NULL) {
					v_eof(&cursor);
					goto err;
				}
			} else {
				mp->flags |= VC_ISMOTION;
				if ((mp->kp->func)(mp, &cursor, &vp->motion))
					goto err;
				if (mp->kp->flags & V_LMODE)
					vp->flags |= VC_LMODE;
				fm = cursor;
			}
			mp->count = cnt;
		} else
			fm = cursor;

		/* If a non-relative movement, set the '' mark. */
		if (flags & V_ABS)
			SETABSMARK(&cursor);

		/* Call the function, get the resulting mark. */
		if ((vp->kp->func)(vp, &fm, &cursor))
			goto err;

		/*
		 * If that command took us out of vi mode, then exit
		 * the loop without further action.
		 */
		if (mode != MODE_VI) {
			move(LINES - 1, 0);
			clrtoeol();
			refresh();
			break;
		}

		/* Update the screen. */
		scr_cchange();

		/* Set the dot variables. */
		if (flags & V_DOT) {
			dot = cmd;
			dot.flags |= VC_ISDOT | vc_c1set;
			dotmotion = motion;
		}
	}
}

#define	KEY(k) {							\
	if (((k) = getkey(WHEN_VICMD)) < 0 || (k) > MAXVIKEY) {		\
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

	/* If '.' command, return the dot motion. */
	if (ismotion && ismotion->flags & VC_ISDOT) {
		*vp = dotmotion;
		return (0);
	}

	/* 'C' and 'D' imply motion to the end-of-line. */
	if (ismotion != NULL && (ismotion->key == 'C' || ismotion->key == 'D'))
		key = '$';
	else {
		/* Pick up optional buffer. */
		KEY(key);
		if (key == '"') {
			KEY(key);
			if (!isalnum(key))
				goto ebuf;
			vp->buffer = key;
			KEY(key);
		} else
			vp->buffer = OOBCB;
	}

	/*
	 * Pick up optional count.  Special case, a leading 0 is not a count,
	 * it's a command.
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
			if (!isalnum(vp->buffer)) {
ebuf:				bell();
				msg("Buffer names are [0-9A-Za-z].");
				return (1);
			}
			vp->buffer = key;
		}

		/*
		 * Special case: '[' and ']' commands.  Doesn't the fact
		 * that the *single* character doesn't mean anything but
		 * the *doubled* character does just frost your shorts?
		 */
		if (vp->key == '[' || vp->key == ']') {
			KEY(key);
			if (vp->key != key) {
usage:				bell();
				msg("Usage: %s", ismotion != NULL ?
				    vikeys[ismotion->key].usage : kp->usage);
				return (1);
			}
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
	} else {
		/*
		 * Commands that have motion components can be doubled to
		 * imply the current line.
		 */
		if (ismotion->key != key && !(flags & V_MOVE))
			goto usage;

		/*
		 * Special case: c[wW] converted to c[eE].
		 *
		 * Wouldn't work right at the end of a word unless backspace
		 * one character before doing the move.  This will fix most
		 * cases.
		 * XXX
		 * But not all.
		 */
		if (ismotion->key == 'c' && (key == 'W' || key == 'w')) {
			kp = vp->kp = &vikeys[vp->key = key == 'W' ? 'E' : 'e'];
			if (cursor.cno && (key == 'e' || key == 'E'))
				--cursor.cno;
		}
	}

	/* Required character. */
	if (flags & V_CHAR)
		KEY(vp->character);
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
	char *p;

	p = file_gline(curf, cursor.lno, &len);
	beg = cursor.cno;

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
		for (end = cursor.cno; ++end < len && inword(p[end]););
		--end;
	} else {
		for (end = cursor.cno; ++end < len && innum(p[end]););

		/* Just a sign isn't a number. */
		if (end == beg && (p[beg] == '+' || p[beg] == '-'))
			goto noword;
		--end;
	}

	/*
	 * Getting a keyword implies moving the cursor to its beginning.
	 * Refresh now.
	 */
	if (beg != cursor.cno) {
		cursor.cno = beg;
		scr_cchange();
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
