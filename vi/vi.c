/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vi.c,v 5.7 1992/05/15 11:11:13 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:11:13 $";
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
#include "options.h"
#include "vcmd.h"
#include "extern.h"

static int getcmd __P((VICMDARG *, int));
static int getcount __P((int, u_long *, int *));
static int getkeyword __P((VICMDARG *, u_int));

void
vi()
{
	static VICMDARG dot, dotmotion;
	register VICMDARG *mp, *vp;
	register int key;
	VICMDARG cmd, motion;
	u_long oldy, oldx;
	u_int flags;
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
		if (msgcnt) {
			erase = 1;
			msg_vflush();
		} else
			erase = 0;
		refresh();
		if (erase)
			scr_modeline();

		/*
		 * We get a command, which may or may not have an associated
		 * motion.  If it does, we get it too calling its underlying
		 * function to get the resulting mark.  We then call the
		 * command, and reset the cursor to the resulting mark.
		 */
#ifdef DEBUG
		bzero(&cmd, sizeof(cmd));
#endif
		vp = &cmd;
		if (getcmd(vp, 0))
			continue;

		mp = NULL;
		flags = vp->kp->flags;

		/* If dot, set the cmd and motion structures. */
		if (vp->key == '.') {
			if (dot.key == '\0') {
				bell();
				msg("No commands executed.");
				continue;
			}
			vp = &dot;
			mp = &dotmotion;
		}

		/* V/v commands require movement commands. */
		if (V_from && !(flags & V_MOVE)) {
			bell();
			msg("%s: not a movement command", vp->key);
			continue;
		}

		/*
		 * If require motion get the motion command and get
		 * the resulting mark.
		 */
		if (flags & V_MOTION) {
			if (getcmd(&motion, vp->key)) {
				bell();
				continue;
			}
			mp = &motion;
		}

		/* Get resulting motion mark. */
		if (mp != NULL &&
		    (mp->kp->func)(mp, &cursor, &vp->motion))
			continue;

		/* Get any associated keyword. */
		if (flags & V_KEYW && getkeyword(vp, flags))
			continue;

		/* Set the cut buffer. */
		if (vp->buffer)
			cutname(vp->buffer);

		/* If a non-relative movement, set the '' mark. */
		if (flags & V_ABS)
			mark_def(&cursor);

		/* Call the function, get the resulting mark. */
		if ((vp->kp->func)(vp, &cursor, &cursor))
			continue;

		/* Update the screen. */
		scr_cchange();

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

		/* Set the dot variables, if necessary . */
		if (flags & V_DOT) {
			dot = cmd;
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
	int ismotion;		/* Previous key if getting motion component. */
{
	register VIKEYS *kp;
	register u_long count;
	register u_int flags;
	u_long hold;
	int key;

	bzero(vp, sizeof(vp));

	if (ismotion) {
		/* 'C' and 'D' imply motion to the end-of-line. */
		if (ismotion == 'C' || ismotion == 'D')
			key = '$';
	} else {
		/* Pick up optional buffer. */
		KEY(key);
		if (key == '"') {
			KEY(key);
			if (!isalnum(key))
				goto ebuf;
			vp->buffer = key;
			KEY(key);
		}
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
	if (ismotion) {
		/*
		 * Commands that have motion components can be doubled to
		 * imply the current line, i.e. * "dd" is the same as "d_".
		 */
		if (ismotion == key)
			kp = vp->kp = &vikeys[vp->key = '_'];

		/*
		 * Special case: c[wW] converted to c[eE].
		 *
		 * Wouldn't work right at the end of a word unless backspace
		 * one character before doing the move.  This will fix most
		 * cases.
		 * XXX
		 * But not all.
		 */
		if (ismotion == 'c' && key == 'W' || key == 'w') {
			kp = vp->kp = &vikeys[vp->key = key == 'W' ? 'E' : 'e'];
			if (cursor.cno && (key == 'e' || key == 'E'))
				--cursor.cno;
		}
	}

	if (kp->func == NULL) {
		bell();
		msg("%s isn't a command.", charname(key));
		return (1);
	}

	flags = kp->flags;

	/* Check for illegal count. */
	if (vp->flags & VC_C1SET && !flags & V_CNT)
		goto usage;

	/* Illegal motion command. */
	if (ismotion)
		if (!(flags & V_MOVE))
			goto usage;
	else {
		/* Illegal buffer. */
		if (!(flags & V_OBUF) && vp->buffer)
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
				msg("Usage: %s", ismotion ?
				    vikeys[ismotion].usage : kp->usage);
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
	}

	/* Required character. */
	if (flags & V_CHAR)
		KEY(vp->character);

	return (0);
}

static int
getkeyword(kp, flags)
	VICMDARG *kp;
	u_int flags;
{
	register size_t beg, end, key;
	size_t len;
	char *p;

	p = file_gline(curf, cursor.lno, &len);

	/* May not be a keyword at all. */
	if (!len || !inword(p[cursor.cno])) {
		bell();
		if (ISSET(O_VERBOSE))
			msg("Cursor not in a word.");
		return (1);
	}

	/* Find the beginning/end of the keyword. */
	if (beg = cursor.cno) {
		do {
			--beg;
		} while (inword(p[beg]) && beg > 0);
		++beg;
	}
	for (end = cursor.cno; ++end < len && inword(p[end]););
	--end;
	
	/*
	 * Getting a keyword implies moving the cursor to its beginning.
	 * Refresh now, even though not likely to be necessary.
	 */
	if (beg != cursor.cno) {
		cursor.cno = beg;
		scr_cchange();
		refresh();
	}

	/* Note where the keyword starts and stops, so we can replace it. */
	kp->kcstart = beg;
	kp->kcstop = end;

	len = end - beg + 1;
	if (kp->klen <= len) {
		kp->klen += len + 50;
		if ((kp->keyword = realloc(kp->keyword, kp->klen)) == NULL) {
			bell();
			msg("Keyword too large: %s", strerror(errno));
			return (1);
		}
	}
	bcopy(p + beg, kp->keyword, len);
	kp->keyword[len] = '\0';			/* XXX */
	return (0);
}
