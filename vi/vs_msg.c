/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_msg.c,v 10.6 1995/07/08 10:24:04 bostic Exp $ (Berkeley) $Date: 1995/07/08 10:24:04 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <curses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "cl.h"
#include "vi.h"

static void	cl_divider __P((SCR *));
static int	cl_output __P((SCR *, mtype_t, const char *, int));
static void	cl_modeline __P((SCR *));
static void	cl_msgsave __P((SCR *, mtype_t, const char *, size_t));
static void	cl_scroll __P((SCR *, CHAR_T *, u_int));

/* 
 * cl_msg --
 *	Display ex output or error messages for the screen.
 *
 * This routine is the editor interface for all ex printing output, and all
 * ex and vi error/informational messages.  It implements the curses strategy
 * of stealing lines from the bottom of the vi text screen, which requires a
 * fair bit of work in the supporting functions in cl_funcs.c.  Screens with
 * an alternate method of displaying messages, i.e. a dialog box, can dispense
 * with almost this entire file.
 *
 * PUBLIC: int cl_msg __P((SCR *, mtype_t, const char *, size_t));
 */
int
cl_msg(sp, mtype, line, rlen)
	SCR *sp;
	mtype_t mtype;
	const char *line;
	size_t rlen;
{
	CL_PRIVATE *clp;
	size_t cols, len, oldy, oldx, padding;
	const char *e, *s, *t;

	/* If fmt is NULL, flush any buffered output. */
	if (line == NULL) {
		if (F_ISSET(sp, S_EX))
			(void)fflush(stdout);
		return (0);
	}
	
	/*
	 * Display ex output or error messages for the screen.  If ex
	 * running, or in ex canonical mode, let stdio(3) do the work.
	 */
	clp = CLP(sp);
	if (F_ISSET(clp, CL_INIT_EX) || F_ISSET(sp, S_EX | S_EX_CANON)) {
		F_SET(clp, CL_EX_WROTE);
		return (printf("%.*s", rlen, line));
	}

	/*
	 * If no screen yet, the user is doing output from a .exrc file or
	 * something similar.  Save the output for later.
	 */
	if (!F_ISSET(clp, CL_INIT_EX | CL_INIT_VI)) {
		cl_msgsave(sp, mtype, line, rlen);
		return (rlen);
	}

	/* Save the current cursor. */
	(void)cl_cursor(sp, &oldy, &oldx);

	/* Avoid overwrite checks by cl_funcs.c routines. */
	F_SET(clp, CL_PRIVWRITE);

	/*
	 * If the message type is changing, terminate any previous message
	 * and move to a new line.
	 */
	if (mtype != clp->mtype) {
		if (clp->lcontinue != 0) {
			if (clp->mtype == M_NONE)
				(void)cl_output(sp, clp->mtype, "\n", 1);
			else
				(void)cl_output(sp, clp->mtype, ".\n", 2);
			clp->lcontinue = 0;
		}
		clp->mtype = mtype;
	}

	/* If it an ex output message, just write it out. */
	if (mtype == M_NONE) {
		(void)cl_output(sp, mtype, line, rlen);
		goto ret;
	}

	/*
	 * Need up to three padding characters normally; a semi-colon and
	 * two separating spaces.  If only a single line on the screen, add
	 * some more for the trailing continuation message.
	 *
	 * XXX
	 * Assume that a semi-colon takes up a single column on the screen.
	 */
	if (IS_ONELINE(sp))
		(void)msg_cmsg(sp, CMSG_CONT_S, &padding);
	else
		padding = 0;
	padding += 3;

	/*
	 * If a message won't fit on a line, try to break it at a <blank>
	 * boundary.  (There are almost certainly pathological cases that
	 * will break this code.)  If subsequent message will fit on the
	 * same line, write a separator, and output it.  Otherwise, put out
	 * a newline.
	 */
	len = rlen;
	if (clp->lcontinue != 0)
		if (len + clp->lcontinue + padding >= sp->cols)
			(void)cl_output(sp, mtype, ".\n", 2);
		else  {
			(void)cl_output(sp, mtype, ";", 1);
			(void)cl_output(sp, M_NONE, "  ", 2);
		}
	for (cols = sp->cols - padding, s = line; len > 0; s = t) {
		for (; isblank(*s) && --len != 0; ++s);
		if (len == 0)
			break;
		if (len > cols) {
			for (e = s + cols; e > s && !isblank(*e); --e);
			if (e == s)
				 e = t = s + cols;
			else
				for (t = e; isblank(e[-1]); --e);
		} else {
			e = s + len;
			/*
			 * XXX:
			 * If t isn't initialized for "s = t", len will be
			 * equal to 0.  Keep the compiler quiet.
			 */
			t = s;
		}
		len -= e - s;
		if ((e - s) > 1 && s[(e - s) - 1] == '.')
			--e;
		(void)cl_output(sp, mtype, s, e - s);
		if (len != 0)
			(void)cl_output(sp, M_NONE, "\n", 1);
		if (INTERRUPTED(sp))
			break;
	}

ret:	/* Clear local update flag. */
	F_CLR(clp, CL_PRIVWRITE);

	/* Restore the cursor and refresh the screen. */
	(void)cl_move(sp, oldy, oldx);
	(void)cl_refresh(sp, 0);
	return (rlen);
}

/*
 * cl_output --
 *	Output the text to the screen.
 */
static int
cl_output(sp, mtype, line, llen)
	SCR *sp;
	mtype_t mtype;
	const char *line;
	int llen;
{
	CL_PRIVATE *clp;
	CHAR_T ch;
	size_t notused;
	int len, rlen, tlen;
	const char *p, *t;

	clp = CLP(sp);
	for (p = line, rlen = llen; llen > 0;) {
		/* Get the next physical line. */
		if ((p = memchr(line, '\n', llen)) == NULL)
			len = llen;
		else
			len = p - line;

		/*
		 * The max is sp->cols characters, and we may have already
		 * written part of the line.
		 */
		if (len + clp->lcontinue > sp->cols)
			len = sp->cols - clp->lcontinue;

		/*
		 * If the first line output, do nothing.  If the second line
		 * output, draw the divider line.  If drew a full screen, we
		 * remove the divider line.  If it's a continuation line, move
		 * to the continuation point, else, move the screen up.
		 */
		if (clp->lcontinue == 0) {
			if (!IS_ONELINE(sp)) {
				if (clp->totalcount == 1) {
					(void)cl_move(sp, INFOLINE(sp) - 1, 0);
					(void)clrtoeol();
					(void)cl_divider(sp);
					F_SET(clp, CL_DIVIDER);
					++clp->totalcount;
					++clp->linecount;
				}
				if (clp->totalcount == sp->t_maxrows &&
				    F_ISSET(clp, CL_DIVIDER)) {
					--clp->totalcount;
					--clp->linecount;
					F_CLR(clp, CL_DIVIDER);
				}
			}
			if (clp->totalcount != 0)
				cl_scroll(sp, &ch, SCROLL_QUIT);

			(void)cl_move(sp, INFOLINE(sp), 0);
			++clp->totalcount;
			++clp->linecount;

			if (INTERRUPTED(sp)) {
				clp->lcontinue =
				    clp->linecount = clp->totalcount = 0;
				break;
			}
		} else
			(void)cl_move(sp, INFOLINE(sp), clp->lcontinue);

		/* Display the line, doing character translation. */
		if (mtype == M_ERR)
			(void)cl_attr(sp, SA_INVERSE, 1);
		for (t = line, tlen = len; tlen--; ++t)
			(void)cl_addstr(sp, KEY_NAME(sp, *t), KEY_LEN(sp, *t));
		if (mtype == M_ERR)
			(void)cl_attr(sp, SA_INVERSE, 0);

		/* Clear the rest of the line. */
		(void)clrtoeol();

		/* If we loop, it's a new line. */
		clp->lcontinue = 0;

		/* Reset for the next line. */
		line += len;
		llen -= len;
		if (p != NULL) {
			++line;
			--llen;
		}
	}

	/* Set up next continuation line. */
	if (p == NULL)
		cl_cursor(sp, &notused, &clp->lcontinue);
	return (rlen);
}

/*
 * cl_resolve --
 *	Deal with message output.
 *
 * This routine is called from the curses screen read loop to periodically
 * make sure that the user has seen any messages that have been displayed.
 * Screens with alternate methods of displaying messages can discard this
 * routine.
 *
 * PUBLIC: int cl_resolve __P((SCR *, int));
 */
int
cl_resolve(sp, fakecomplete)
	SCR *sp;
	int fakecomplete;
{
	CHAR_T ch;
	CL_PRIVATE *clp;
	EVENT ev;
	size_t oldy, oldx;
	u_int32_t notused;

	/* Ring the bell if it's scheduled. */
	if (F_ISSET(sp->gp, G_BELLSCHED)) {
		F_CLR(sp->gp, G_BELLSCHED);
		(void)cl_bell(sp);
	}

	/* That's all that needs to be done for ex. */
	if (F_ISSET(sp, S_EX))
		return (0);

	getyx(stdscr, oldy, oldx);

	/*
	 * If a command hasn't just completed:
	 *	If 0 message lines are in use, we're in vi mode, we're not
	 *	running an ex command in canonical mode, and we're not in
	 *	input mode on the colon command line, update the mode line.
	 */
	clp = CLP(sp);
	if (fakecomplete)
		F_SET(sp, S_COMPLETE);
	if (!F_ISSET(sp, S_COMPLETE)) {
		if (clp->totalcount == 0 && F_ISSET(sp, S_VI) &&
		    !F_ISSET(sp, S_EX_CANON | S_INPUT_INFO)) {
			cl_modeline(sp);
			(void)move(oldy, oldx);
			refresh();
		}
		return (0);
	}

	/*
	 * Each time a command completes:
	 *	Display change reporting, status line.
	 *	If we entered full ex canonical mode and wrote something,
	 *	or >1 message line in use:
	 *		Put up return-to-continue message.
	 *		If ended-ex command flag set and we get a colon
	 *		continue character, return.
	 *		else
	 *			Repaint overwritten screen lines.
	 *	If in canonical mode, exit canonical mode.
	 */
	F_SET(clp, CL_PRIVWRITE);
	msg_rpt(sp);
	if (F_ISSET(sp, S_STATUS)) {
		F_CLR(sp, S_STATUS);
		msg_status(sp, sp->lno, 0);
	}
	if (clp->totalcount > 1 ||
	    F_ISSET(sp, S_EX_CANON) && F_ISSET(clp, CL_EX_WROTE)) {
		cl_scroll(sp, &ch, SCROLL_WAIT);
		if (F_ISSET(sp, S_COMPLETE_EX) && ch == ':') {
			F_CLR(clp, CL_PRIVWRITE);
			F_CLR(sp, S_COMPLETE | S_COMPLETE_EX);
			return (1);
		}
		if (F_ISSET(clp, CL_REPAINT) ||
		    F_ISSET(sp, S_EX_CANON) && F_ISSET(clp, CL_EX_WROTE)) {
			clearok(curscr, 1);
			ev.e_event = E_REPAINT;
			ev.e_flno = 1;
		} else {
			ev.e_event = E_REPAINT;
			ev.e_flno = clp->totalcount >= sp->rows ? 1 :
			    (sp->rows - clp->totalcount) + 1;
		}
		/*
		 * !!!
		 * Clear the message count variables, so that the screen
		 * code lets the user write the message area.
		 */
		clp->linecount = clp->lcontinue = clp->totalcount = 0;

		/* Do the repaint. */
		ev.e_tlno = sp->rows;
		(void)v_event_handler(sp, &ev, &notused);

		/* The repaint event also reset the cursor for us. */
		getyx(stdscr, oldy, oldx);
	}

	/*
	 * Clear the repaint and wrote-in-ex-canonical mode bit, they
	 * don't apply across commands.
	 */
	F_CLR(clp, CL_EX_WROTE | CL_PRIVWRITE | CL_REPAINT);

	/* Leave canonical mode. */
	if (F_ISSET(sp, S_EX_CANON))
		(void)cl_canon(sp, 0);

	/*
	 * If 0 message lines in use (which can be caused by the user
	 * answering return-to-continue, put up the modeline.
	 */
	if (clp->totalcount == 0 && F_ISSET(sp, S_VI))
		cl_modeline(sp);

	/* Reset the count of overwriting lines. */
	clp->linecount = clp->lcontinue = clp->totalcount = 0;

	/* Restore the user's cursor. */
	(void)move(oldy, oldx);
	refresh();

	/* Clear the command-completed flags. */
	F_CLR(sp, S_COMPLETE | S_COMPLETE_EX);
	return (0);
}
			
/*
 * cl_modeline --
 *	Update the mode line.
 *
 * This routine is called from the curses screen read loop to update the
 * screen modeline display.  Screens with alternate methods of displaying
 * the modeline (e.g. ones that don't require shared screen real-estate)
 * can discard this routine.
 */
static void
cl_modeline(sp)
	SCR *sp;
{
	static const char *modes[] = {
		"215|Append",			/* SM_APPEND */
		"216|Change",			/* SM_CHANGE */
		"217|Command",			/* SM_COMMAND */
		"218|Insert",			/* SM_INSERT */
		"219|Replace",			/* SM_REPLACE */
	};
	size_t cols, curlen, endpoint, len, midpoint;
	const char *t;
	char *p, buf[20];

	/*
	 * We put down the file name, the ruler, the mode and the dirty flag.
	 * If there's not enough room, there's not enough room, we don't play
	 * any special games.  We try to put the ruler in the middle and the
	 * mode and dirty flag at the end.
	 *
	 * !!!
	 * Leave the last character blank, in case it's a really dumb terminal
	 * with hardware scroll.  Second, don't paint the last character in the
	 * screen, SunOS 4.1.1 and Ultrix 4.2 curses won't let you.
	 */
	cols = sp->cols - 1;

	curlen = 0;
	for (p = sp->frp->name; *p != '\0'; ++p);
	while (--p > sp->frp->name) {
		if (*p == '/') {
			++p;
			break;
		}
		if ((curlen += KEY_LEN(sp, *p)) > cols) {
			curlen -= KEY_LEN(sp, *p);
			++p;
			break;
		}
	}

	/* Clear the mode line. */
	(void)cl_move(sp, INFOLINE(sp), 0);
	(void)clrtoeol();

	/* Display the file name. */
	for (; *p != '\0'; ++p)
		(void)cl_addstr(sp, KEY_NAME(sp, *p), KEY_LEN(sp, *p));

	/*
	 * Display the ruler.  If we're not at the midpoint yet, move there.
	 * Otherwise, add in two extra spaces.
	 *
	 * XXX
	 * Assume that numbers, commas, and spaces only take up a single
	 * column on the screen.
	 */
	if (O_ISSET(sp, O_RULER)) {
		len = snprintf(buf, sizeof(buf), "%lu,%lu", sp->lno,
		    (VIP(sp)->sc_smap->off - 1) * sp->cols + VIP(sp)->sc_col -
		    (O_ISSET(sp, O_NUMBER) ? O_NUMBER_LENGTH : 0) + 1);
		midpoint = (cols - ((len + 1) / 2)) / 2;
		if (curlen < midpoint) {
			(void)cl_move(sp, INFOLINE(sp), midpoint);
			curlen += len;
		} else if (curlen + 2 + len < cols) {
			(void)cl_addstr(sp, "  ", 2);
			curlen += 2 + len;
		}
		(void)cl_addstr(sp, buf, len);
	}

	/*
	 * Display the mode and the modified flag, as close to the end of the
	 * line as possible, but guaranteeing at least two spaces between the
	 * ruler and the modified flag.
	 */
#define	MODESIZE	9
	endpoint = cols;
	if (O_ISSET(sp, O_SHOWMODE)) {
		if (F_ISSET(sp->ep, F_MODIFIED))
			--endpoint;
		t = msg_cat(sp, modes[sp->showmode], &len);
		endpoint -= len;
	}

	if (endpoint > curlen + 2) {
		(void)cl_move(sp, INFOLINE(sp), endpoint);
		if (O_ISSET(sp, O_SHOWMODE)) {
			if (F_ISSET(sp->ep, F_MODIFIED))
				(void)cl_addstr(sp,
				    KEY_NAME(sp, '*'), KEY_LEN(sp, '*'));
			(void)cl_addstr(sp, t, len);
		}
	}
}

/*
 * cl_scroll --
 *	Scroll the screen for output.
 */
static void
cl_scroll(sp, chp, flags)
	SCR *sp;
	CHAR_T *chp;
	u_int flags;
{
	CHAR_T ch;
	CL_PRIVATE *clp;
	size_t len;
	const char *p;

	clp = CLP(sp);
	if (!IS_ONELINE(sp)) {
		/*
		 * Scroll the screen.  Instead of scrolling the entire screen,
		 * delete the line above the first line output so preserve the
		 * maximum amount of the screen.
		 */
		(void)cl_move(sp, clp->totalcount <
		    sp->rows ? INFOLINE(sp) - clp->totalcount : 0, 0);
		(void)cl_deleteln(sp);

		/* If there are screens below us, push them back into place. */
		if (sp->q.cqe_next != (void *)&sp->gp->dq) {
			(void)cl_move(sp, INFOLINE(sp), 0);
			(void)cl_insertln(sp);
		}
	}

	/* If just displayed a full screen, wait. */
	if (LF_ISSET(SCROLL_WAIT) || clp->linecount == sp->t_maxrows) {
		if (IS_ONELINE(sp))
			p = msg_cmsg(sp, CMSG_CONT_S, &len);
		else {
			(void)cl_move(sp, INFOLINE(sp), 0);
			p = msg_cmsg(sp,
			    LF_ISSET(SCROLL_QUIT) ? CMSG_CONT_Q : CMSG_CONT,
			    &len);
		}
		(void)cl_addstr(sp, p, len);

		++clp->totalcount;
		clp->linecount = 0;

		(void)clrtoeol();
		(void)cl_refresh(sp, 0);

		/*
		 * Get a single character from the terminal.  Any errors are
		 * ignored for now, simply pretend the user interrupted us.
		 */
		switch (read(STDIN_FILENO, &ch, 1)) {
		case 0:
		case -1:
			if (chp != NULL)
				*chp = CH_QUIT;
			F_SET(sp, S_INTERRUPTED);
			break;
		case 1:
			if (chp != NULL)
				*chp = ch;
			if (LF_ISSET(SCROLL_QUIT) && ch == CH_QUIT)
				F_SET(sp, S_INTERRUPTED);
		}
	}
}

/*
 * cl_divider --
 *	Draw a dividing line between the screen and the output.
 */
static void
cl_divider(sp)
	SCR *sp;
{
	size_t len;

#define	DIVIDESTR	"+=+=+=+=+=+=+=+"
	len =
	    sizeof(DIVIDESTR) - 1 > sp->cols ? sp->cols : sizeof(DIVIDESTR) - 1;
	(void)cl_attr(sp, SA_INVERSE, 1);
	(void)cl_addstr(sp, DIVIDESTR, len);
	(void)cl_attr(sp, SA_INVERSE, 0);
}

/*
 * cl_msgsave --
 *	Save a message for later display.
 */
static void
cl_msgsave(sp, mt, p, len)
	SCR *sp;
	mtype_t mt;
	const char *p;
	size_t len;
{
	GS *gp;
	MSG *mp_c, *mp_n;

	/*
	 * We have to handle messages before we have any place to put them.
	 * If there's no screen support yet, allocate a msg structure, copy
	 * in the message, and queue it on the global structure.  If we can't
	 * allocate memory here, we're genuinely screwed, dump the message
	 * to stderr in the (probably) vain hope that someone will see it.
	 */
	CALLOC_NOMSG(sp, mp_n, MSG *, 1, sizeof(MSG));
	if (mp_n == NULL)
		goto nomem;
	MALLOC_NOMSG(sp, mp_n->buf, char *, len);
	if (mp_n->buf == NULL) {
		free(mp_n);
nomem:		(void)fprintf(stderr, "%.*s\n", (int)len, p);
		return;
	}

	memmove(mp_n->buf, p, len);
	mp_n->len = len;
	mp_n->mtype = mt;

	gp = sp->gp;
	if ((mp_c = gp->msgq.lh_first) == NULL) {
		LIST_INSERT_HEAD(&gp->msgq, mp_n, q);
	} else {
		for (; mp_c->q.le_next != NULL; mp_c = mp_c->q.le_next);
		LIST_INSERT_AFTER(mp_c, mp_n, q);
	}
}
