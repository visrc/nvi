/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_funcs.c,v 10.11 1995/07/08 09:50:44 bostic Exp $ (Berkeley) $Date: 1995/07/08 09:50:44 $";
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

static int cl_lline_copy __P((SCR *, size_t *, CHAR_T **, size_t *));

static int user_request;

/*
 * cl_addstr --
 *	Add len bytes from the string at the cursor, advancing the cursor.
 *
 * PUBLIC: int cl_addstr __P((SCR *, const char *, size_t));
 */
int
cl_addstr(sp, str, len)
	SCR *sp;
	const char *str;
	size_t len;
{
	CL_PRIVATE *clp;
	size_t oldy, oldx;
	int iv, rval;

	EX_ABORT(sp);
	VI_INIT_IGNORE(sp);

	/*
	 * The curses screen overlaps ex and error message output with the
	 * normal user screen.  For that reason, we have to be able to tell
	 * if a request to change the screen is from the editor or from the
	 * screen code.  The former is ignored if the screen code is using
	 * those lines for its own purposes.
	 *
	 * This routine can be discarded if the screen has a separate area
	 * for presenting ex and error message output.
	 */
	clp = CLP(sp);
	getyx(stdscr, oldy, oldx);
	if (clp->totalcount != 0 && !F_ISSET(clp, CL_PRIVWRITE) &&
	    oldy > RLNO(sp, INFOLINE(sp)) - clp->totalcount)
		return (0);

	/*
	 * If it's the last line of the screen:
	 *	If there's a busy message already there, discard the busy
	 *	message.
	 *
	 *	If a split screen, use inverse video.
	 *
	 * Screens that have other ways than inverse video to separate split
	 * screens should discard the test for split screens and not set the
	 * output mode to inverse video.
	 */
	iv = 0;
	getyx(stdscr, oldy, oldx);
	if (oldy == RLNO(sp, INFOLINE(sp))) {
		if (clp->busy_state == BUSY_ON)
			clp->busy_state = BUSY_SILENT;
		if (IS_SPLIT(sp)) {
			iv = 1;
			F_SET(clp, CL_LLINE_IV);
		}
	}

	if (iv)
		(void)standout();
	if (rval = (addnstr(str, len) == ERR))
		msgq(sp, M_ERR, "Error: addstr: %.*s", (int)len, str);
	if (iv)
		(void)standend();
	return (rval);
}

/*
 * cl_attr --
 *	Toggle a screen attribute on/off.
 *
 * PUBLIC: int cl_attr __P((SCR *, scr_attr_t, int));
 */
int
cl_attr(sp, attribute, on)
	SCR *sp;
	scr_attr_t attribute;
	int on;
{
	CL_PRIVATE *clp;

	/*
	 * Screens not supporting standalone ex mode should discard tests for
	 * S_EX, and use an EX_ABORT macro, replacing EX_INIT_IGNORE.
	 */
	EX_INIT_IGNORE(sp);
	VI_INIT_IGNORE(sp);

	switch (attribute) {
	case SA_INVERSE:
		if (F_ISSET(sp, S_EX)) {
			clp = CLP(sp);
			if (clp->smso == NULL)
				return (1);
			if (on)
				(void)tputs(clp->smso, 1, cl_putchar);
			else
				(void)tputs(clp->rmso, 1, cl_putchar);
		} else
			return (on ? standout() == ERR : standend() == ERR);
		break;
	default:
		abort();
	}
	return (0);
}

/*
 * cl_bell --
 *	Ring the bell/flash the screen.
 *
 * PUBLIC: int cl_bell __P((SCR *));
 */
int
cl_bell(sp)
	SCR *sp;
{
	VI_INIT_IGNORE(sp);

	/*
	 * Screens not supporting standalone ex mode should discard tests for
	 * S_EX, and use an EX_ABORT macro.
	 */
	if (F_ISSET(sp, S_EX)) {
		(void)write(STDOUT_FILENO, "\07", 1);		/* \a */
		return (0);
	}

	/*
	 * Vi has an edit option which determines if the terminal should be
	 * beeped or the screen flashed.
	 */
	if (O_ISSET(sp, O_FLASH))
		flash();
	else
		beep();
	return (0);
}

/*
 * cl_busy --
 *	Display, update or clear a busy message.
 *
 * PUBLIC: int cl_busy __P((SCR *, const char *, int));
 */
int
cl_busy(sp, msg, on)
	SCR *sp;
	const char *msg;
	int on;
{
	static const char flagc[] = "|/-|-\\";
	CL_PRIVATE *clp;
	struct timeval tv;
	size_t info, len, notused, oldy;
	const char *p;

	VI_INIT_IGNORE(sp);

	/*
	 * Check for ex modes.
	 *
	 * This test is correct for all screen types.  Vi can enter this code
	 * with the S_EX_CANON flag set, and the additional flag tests should
	 * not cause harm.
	 */
	if (F_ISSET(sp, S_EX | S_EX_CANON | S_EX_SILENT))
		return (0);

	/*
	 * See the comment in cl_addstr(); if the line is already in use,
	 * ignore the request.  Note, we're assuming that all calls to this
	 * function are from the editor, not from another part of the screen.
	 */
	clp = CLP(sp);
	info = RLNO(sp, INFOLINE(sp));
	if (!F_ISSET(clp, CL_PRIVWRITE) && clp->totalcount != 0) {
		getyx(stdscr, oldy, notused);
		if (oldy > info - clp->totalcount)
			return (0);
	}

	/*
	 * Most of this routine is to deal with the fact that the curses screen
	 * shares real estate between the editor text and the busy messages.  If
	 * the screen has a better way to display busy messages, most of this
	 * goes away.  Logically, all that's needed is something that puts up a
	 * message, periodically updates it, and then goes away.
	 */
	if (on)
		switch (clp->busy_state) {
		case BUSY_OFF:
			/*
			 * Save the current cursor position and move to the
			 * info line.
			 */
			getyx(stdscr, clp->busy_y, clp->busy_x);
			(void)move(info, 0);
			getyx(stdscr, notused, clp->busy_fx);

			/* If there's no message, just rest the cursor. */
			if (msg == NULL) {
				clp->busy_state = BUSY_SILENT;
				refresh();
				break;
			}

			/* Save a copy of whatever is currently there. */
			if (cl_lline_copy(sp,
			    &clp->lline_len, &clp->lline, &clp->lline_blen))
				return (1);

			/* Display the busy message. */
			p = msg_cat(sp, msg, &len);
			(void)addnstr(p, len);
			getyx(stdscr, notused, clp->busy_fx);
			(void)clrtoeol();
			(void)move(info, clp->busy_fx);

			/* Initialize state for updates. */
			clp->busy_ch = 0;
			(void)gettimeofday(&clp->busy_tv, NULL);

			/* Update the state. */
			clp->busy_state = BUSY_ON;
			refresh();
			break;
		case BUSY_ON:
			/* Update no more than every 1/4 of a second. */
			(void)gettimeofday(&tv, NULL);
			if (((tv.tv_sec - clp->busy_tv.tv_sec) * 1000000 +
			    (tv.tv_usec - clp->busy_tv.tv_usec)) < 4000)
				return (0);

			/* Display the update. */
			(void)move(info, clp->busy_fx);
			if (clp->busy_ch == sizeof(flagc))
				clp->busy_ch = 0;
			(void)addnstr(flagc + clp->busy_ch++, 1);
			(void)move(info, clp->busy_fx);

			refresh();
			break;
		case BUSY_SILENT:
			break;
		}
	else
		switch (clp->busy_state) {
		case BUSY_OFF:
			break;
		case BUSY_ON:
			/* Restore the contents of the line. */
			move(info, 0);
			if (clp->lline_len == 0)
				clrtoeol();
			else {
				if (F_ISSET(clp, CL_LLINE_IV))
					(void)standout();
				(void)addnstr(clp->lline, clp->lline_len);
				if (F_ISSET(clp, CL_LLINE_IV))
					(void)standend();
				clp->lline_len = 0;
			}
			/* FALLTHROUGH */
		case BUSY_SILENT:
			clp->busy_state = BUSY_OFF;
			(void)move(clp->busy_y, clp->busy_x);
			(void)refresh();
		}
	return (0);
}

/*
 * cl_canon --
 *	Enter/leave tty canonical mode.
 *
 * PUBLIC: int cl_canon __P((SCR *, int));
 */
int
cl_canon(sp, enter)
	SCR *sp;
	int enter;
{
	/*
	 * This function isn't needed by any screen not supporting ex commands
	 * that require full terminal canonical mode (e.g. :shell).  Returning
	 * 1 for failure will cause the editor to reject the command.
	 */
	EX_NOOP(sp);
	VI_INIT_IGNORE(sp);

	if (enter) {
		/*
		 * All screens should move to the bottom of the screen, even
		 * in the presence of split screens.  This makes terminal
		 * scrolling happen naturally and without overwriting editor
		 * text.  Don't clear the info line, its contents may be valid,
		 * e.g. :file|append.
		 */
		(void)move(O_VAL(sp, O_LINES) - 1, 0);
		(void)refresh();
		return (cl_ex_tinit(sp));
	} else
		return (cl_ex_tend(sp));
}

/*
 * cl_clrtoeol --
 *	Clear from the current cursor to the end of the line.
 *
 * PUBLIC: int cl_clrtoeol __P((SCR *));
 */
int
cl_clrtoeol(sp)
	SCR *sp;
{
	CL_PRIVATE *clp;
	int oldy, oldx;

	EX_NOOP(sp);
	VI_INIT_IGNORE(sp);

	/*
	 * See the comment in cl_addstr(); if the line is already in use,
	 * ignore the request.  Note, we're assuming that all calls to this
	 * function are from the editor, not from another part of the screen.
	 */
	clp = CLP(sp);
	if (!F_ISSET(clp, CL_PRIVWRITE) && clp->totalcount != 0) {
		getyx(stdscr, oldy, oldx);
		if (oldy > RLNO(sp, INFOLINE(sp)) - clp->totalcount)
			return (0);
	}
	return (clrtoeol() == ERR);
}

/*
 * cl_cursor --
 *	Return the current cursor position.
 *
 * PUBLIC: int cl_cursor __P((SCR *, size_t *, size_t *));
 */
int
cl_cursor(sp, yp, xp)
	SCR *sp;
	size_t *yp, *xp;
{
	EX_ABORT(sp);
	VI_INIT_IGNORE(sp);

	/*
	 * The curses screen support splits a single underlying curses screen
	 * into multiple screens to support split screen semantics.  For this
	 * reason the returned value must be adjusted to be relative to the
	 * current screen, and not absolute.  Screens that implement the split
	 * using physically distinct screens won't need this hack.
	 */
	getyx(stdscr, *yp, *xp);
	*yp -= sp->woff;
	return (0);
}

/*
 * cl_move --
 *	Move the cursor.
 *
 * PUBLIC: int cl_move __P((SCR *, size_t, size_t));
 */
int
cl_move(sp, lno, cno)
	SCR *sp;
	size_t lno, cno;
{
	EX_ABORT(sp);
	VI_INIT_IGNORE(sp);

	/*
	 * See the comment in cl_cursor.  Screens implementing split screens
	 * using physically distinct screens won't need to adjust the cl_move
	 * parameters.
	 */
	if (move(RLNO(sp, lno), cno) != ERR)
		return (0);

	msgq(sp, M_ERR, "Error: move: l(%u) c(%u) o(%u)", lno, cno, sp->woff);
	return (1);
}

/*
 * cl_deleteln --
 *	Delete the current line, scrolling all lines below it.
 *
 * PUBLIC: int cl_deleteln __P((SCR *));
 */
int
cl_deleteln(sp)
	SCR *sp;
{
	CHAR_T *p;
	CL_PRIVATE *clp;
	size_t len, llen, oldy, oldx;

	EX_ABORT(sp);
	VI_INIT_IGNORE(sp);

	/*
	 * See the comment in cl_addstr(); if the line is already in use,
	 * ignore the request.  Note, we're assuming that all calls to this
	 * function are from the editor, not from another part of the screen.
	 * The additional difficulty here is that if we discard this request,
	 * we either have to preserve the change for later application, or
	 * force a full rewrite of the screen lines by the editor when the
	 * messages are cleared.  The latter is easier.
	 */
	clp = CLP(sp);
	if (!F_ISSET(clp, CL_PRIVWRITE) && clp->totalcount != 0) {
		F_SET(clp, CL_REPAINT);
		return (0);
	}

	/*
	 * This clause is required because the curses screen shares space
	 * between busy messages and other output.  If the screen does not
	 * do this, this code won't be necesssary.
	 *
	 * If the bottom line was in use for a busy message:
	 *
	 *	Get a copy of the busy message.
	 *	Replace it with whatever was there previously.
	 *	Scroll the screen.
	 *	Restore the busy message.
	 */ 
	if (clp->busy_state == BUSY_ON) {
		getyx(stdscr, oldy, oldx);
		p = NULL;
		len = llen = 0;
		if (cl_lline_copy(sp, &len, &p, &llen))
			return (1);
		if (clp->lline_len == 0)
			(void)clrtoeol();
		else {
			(void)move(RLNO(sp, INFOLINE(sp)), 0);
			(void)addnstr(clp->lline, clp->lline_len);
			clp->lline_len = 0;
		}
		if (deleteln() == ERR)
			return (1);
		if (len != 0) {
			(void)move(RLNO(sp, INFOLINE(sp)), 0);
			(void)addnstr(p, len);
		}
		(void)move(oldy, oldx);
		return (0);
	}

	/*
	 * This clause is required because the curses screen uses reverse
	 * video to delimit split screens.  If the screen does not do this,
	 * this code won't be necessary.
	 *
	 * If the bottom line was in reverse video, rewrite it in normal
	 * video before it's scrolled.
	 */
	if (F_ISSET(clp, CL_LLINE_IV)) {
		if (cl_lline_copy(sp,
		    &clp->lline_len, &clp->lline, &clp->lline_blen))
			return (1);

		if (clp->lline_len == 0)
			(void)clrtoeol();
		else {
			getyx(stdscr, oldy, oldx);
			(void)move(RLNO(sp, INFOLINE(sp)), 0);
			(void)addnstr(clp->lline, clp->lline_len);
			(void)move(oldy, oldx);
			clp->lline_len = 0;
#ifndef THIS_FIXES_A_BUG_IN_NCURSES
/*
 * This line fixes a bug in ncurses, where "file|file|file" shows
 * up with half the lines in reverse video.
 */
refresh();
#endif
		}
		F_CLR(clp, CL_LLINE_IV);
	}

	/*
	 * The bottom line is expected to be blank after this operation,
	 * and the screen must support this semantic.
	 */
	return (deleteln() == ERR);
}

/*
 * cl_insertln --
 *	Push down the current line, discarding the bottom line.
 *
 * PUBLIC: int cl_insertln __P((SCR *));
 */
int
cl_insertln(sp)
	SCR *sp;
{
	CL_PRIVATE *clp;

	EX_ABORT(sp);
	VI_INIT_IGNORE(sp);

	/*
	 * See the comment in cl_addstr(); if the line is already in use,
	 * ignore the request.  Note, we're assuming that all calls to this
	 * function are from the editor, not from another part of the screen.
	 * The additional difficulty here is that if we discard this request,
	 * we either have to preserve the change for later application, or
	 * force a full rewrite of the screen lines by the editor when the
	 * messages are cleared.  The latter is easier.
	 */
	clp = CLP(sp);
	if (!F_ISSET(clp, CL_PRIVWRITE) && clp->totalcount != 0) {
		F_SET(clp, CL_REPAINT);
		return (0);
	}

	/*
	 * The current line is expected to be blank after this operation,
	 * and the screen must support this semantic.
	 */
	return (insertln() == ERR);
}

/* 
 * cl_ex_adjust --
 *	Adjust the screen for ex.
 *
 * PUBLIC: int cl_ex_adjust __P((SCR *, exadj_t));
 */
int
cl_ex_adjust(sp, action)
	SCR *sp;
	exadj_t action;
{
	CL_PRIVATE *clp;
	int cnt;

	/*
	 * This routine is purely for standalone ex programs.  All special
	 * purpose, alll special case.  Screens not supporting a standalone
	 * ex mode should replace it with an EX_ABORT macro.
	 */
	VI_ABORT(sp);

	clp = CLP(sp);
	switch (action) {
	case EX_TERM_SCROLL:
		/* Move the cursor up one line if that's possible. */
		if (clp->cuu1 != NULL)
			(void)tputs(clp->cuu1, 1, cl_putchar);
		else if (clp->cup != NULL)
			(void)tputs(tgoto(clp->cup,
			    0, O_VAL(sp, O_LINES) - 2), 1, cl_putchar);
		else
			return (0);
		/* FALLTHROUGH */
	case EX_TERM_CE:
		/* Clear the line. */
		if (clp->el != NULL) {
			(void)putchar('\r');
			(void)tputs(clp->el, 1, cl_putchar);
		} else {
			/*
			 * Historically, ex didn't erase the line, so, if the
			 * displayed line was only a single glyph, and <eof>
			 * was more than one glyph, the output would not fully
			 * overwrite the user's input.  To fix this, output
			 * the maxiumum character number of spaces.  Note,
			 * this won't help if the user entered extra prompt
			 * or <blank> characters before the command character.
			 * We'd have to do a lot of work to make that work, and
			 * it's almost certainly not worth the effort.
			 */
			for (cnt = 0; cnt < MAX_CHARACTER_COLUMNS; ++cnt)
				(void)putchar('\b');
			for (cnt = 0; cnt < MAX_CHARACTER_COLUMNS; ++cnt)
				(void)putchar(' ');
			(void)putchar('\r');
			(void)fflush(stdout);
		}
		break;
	default:
		abort();
	}
	return (0);
}

/*
 * cl_getkey --
 *	Get a single terminal key (NOT event, terminal key).
 *
 * PUBLIC: int cl_getkey __P((SCR *, CHAR_T *));
 */
int
cl_getkey(sp, chp)
	SCR *sp;
	CHAR_T *chp;
{
	CL_PRIVATE *clp;
	int nr;

	/* 
	 * This routine is used by the ex s command with the c flag.  The
	 * area to be changed is displayed, then the user is prompted to
	 * confirm the change, and then this routine is called.  It's going
	 * to be difficult to make that event driven, but it will be done,
	 * at which point this routine will go away.  Logically, this code
	 * should be replaced by whatever code is needed to return the next
	 * user keystroke.
	 */
	clp = CLP(sp);
	switch (cl_read(sp, clp->ibuf, sizeof(clp->ibuf), &nr, NULL)) {
	case INP_OK:
		*chp = clp->ibuf[0];
		if (--nr) {
			memmove(clp->ibuf, clp->ibuf + 1, nr);
			clp->icnt = nr;
		}
		return (0);
	case INP_INTR:
	case INP_EOF:
	case INP_ERR:
		break;
	default:
		abort();
	}
	return (1);
}

/* 
 * cl_interrupt --
 *	Check for interrupts.
 *
 * PUBLIC: int cl_interrupt __P((SCR *));
 */
int
cl_interrupt(sp)
	SCR *sp;
{
	CL_PRIVATE *clp;

	/*
	 * XXX
	 * This is nasty.  If ex/vi asks about interrupts we can assume that
	 * the appropriate messages have been displayed and there's no need
	 * to post an interrupt event later.  Else, the screen code must post
	 * an interrupt event.
	 */
	clp = CLP(sp);
	if (F_ISSET(clp, CL_SIGINT)) {
		F_CLR(clp, CL_SIGINT);
		return (1);
	}
	return (0);
}

/*
 * cl_refresh --
 *	Refresh the screen.
 *
 * PUBLIC: int cl_refresh __P((SCR *, int));
 */
int
cl_refresh(sp, repaint)
	SCR *sp;
	int repaint;
{
	EX_NOOP(sp);
	VI_INIT_IGNORE(sp);

	/*
	 * If repaint is set, the editor is telling us that we don't know
	 * what's on the screen, so we have to repaint from scratch.
	 *
	 * In the curses library, doing wrefresh(curscr) is okay, but the
	 * screen flashes when we then apply the refresh() to bring it up
	 * to date.  So, use clearok().
	 */
	if (repaint)
		clearok(curscr, 1);
	return (refresh() == ERR);
}

/*
 * cl_discard --
 *	Discard a screen.
 *
 * PUBLIC: int cl_discard __P((SCR *, SCR **, dir_t *));
 */
int
cl_discard(sp, addp, dp)
	SCR *sp, **addp;
	dir_t *dp;
{
	SCR *nsp;

	EX_ABORT(sp);

	/*
	 * The cl_discard, cl_resize and cl_split routines are called when
	 * screens are exited, resized or split, respectively.  They will
	 * all have to change (and, in addition, the vi editor code may have
	 * to change) if there's a screen implementation that doesn't split
	 * screens the way that the curses screen does, i.e. one where split
	 * screens aren't created by splitting an existing screen in half.
	 *
	 * XXX
	 * This code is badly broken up between the editor and the screen
	 * code.  Once we have some idea what other screens will want, it
	 * should be reworked to provide a lot more information hiding.
	 *
	 * Discard screen sp.  If another screen got its real-estate, return
	 * return that screen and set if it was a screen immediately above or
	 * below it the discarded screen.  Otherwise, return NULL.
	 *
	 * In the curses screen, add into a previous screen and then into a
	 * subsequent screen, as they're the closest to the current screen.
	 * If that doesn't work, there was no screen to join.
	 */
	if ((nsp = sp->q.cqe_prev) != (void *)&sp->gp->dq) {
		nsp->rows += sp->rows;
		*addp = nsp;
		*dp = FORWARD;
	} else if ((nsp = sp->q.cqe_next) != (void *)&sp->gp->dq) {
		nsp->woff = sp->woff;
		nsp->rows += sp->rows;
		*addp = nsp;
		*dp = BACKWARD;
	} else {
		*addp = NULL;
		*dp = NOTSET;
	}
	return (0);
}

/*
 * cl_resize --
 *	Resize a screen.
 *
 * PUBLIC: int cl_resize __P((SCR *, long, long, SCR *, long, long));
 */
int
cl_resize(a, a_sz, a_off, b, b_sz, b_off)
	SCR *a, *b;
	long a_sz, a_off, b_sz, b_off;
{
	EX_ABORT(a);

	/*
	 * See the comment in cl_discard().
	 *
	 * X_sz is the signed, change in the total size of the split screen,
	 * X_off is the signed change in the offset of the split screen in the
	 * curses screen.
	 */
	a->rows += a_sz;
	a->woff += a_off;
	b->rows += b_sz;
	b->woff += b_off;
	return (0);
}

/*
 * cl_split --
 *	Split a screen.
 *
 * PUBLIC: int cl_split __P((SCR *, SCR *, int));
 */
int
cl_split(old, new, to_up)
	SCR *old, *new;
	int to_up;
{
	size_t half;

	EX_ABORT(old);

	/*
	 * See the comment in cl_discard.
	 *
	 * Split the screen in half, and update the shared information.
	 */
	half = old->rows / 2;
	if (to_up) {				/* Old is bottom half. */
		new->rows = old->rows - half;	/* New. */
		new->woff = old->woff;
		old->rows = half;		/* Old. */
		old->woff += new->rows;
	} else {				/* Old is top half. */
		new->rows = old->rows - half;	/* New. */
		new->woff = old->woff + half;
		old->rows = half;		/* Old. */
	}
	return (0);
}

/*
 * cl_suspend --
 *	Suspend a screen.
 *
 *
 * PUBLIC: int cl_suspend __P((SCR *));
 */
int
cl_suspend(sp)
	SCR *sp;
{
	struct termios t;
	GS *gp;
	int rval;

	rval = 0;
	switch (F_ISSET(sp, S_EX | S_VI)) {
	case S_EX:
		/*
		 * This part of the function isn't needed by any screen not
		 * supporting ex commands that require full terminal canonical
		 * mode (e.g. :suspend).  Returning 1 for failure will cause
		 * the editor to reject the command.
		 *
		 * Save the terminal settings, and restore the original ones.
		 */
		gp = sp->gp;
		if (F_ISSET(gp, G_STDIN_TTY)) {
			if (tcgetattr(STDIN_FILENO, &t)) {
				msgq(sp, M_SYSERR, "suspend: tcgetattr");
				return (1);
			}
			if (F_ISSET(gp, G_TERMIOS_SET) &&
			    tcsetattr(STDIN_FILENO,
			    TCSASOFT | TCSADRAIN, &gp->original_termios)) {
				msgq(sp, M_SYSERR,
				    "suspend: tcsetattr original");
				return (1);
			}
		}

		/* Stop the process group. */
		if (rval = kill(0, SIGTSTP))
			msgq(sp, M_SYSERR, "suspend: kill");

		/* Time passes ... */

		/* Restore terminal settings. */
		if (F_ISSET(gp, G_STDIN_TTY) &&
		    tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &t)) {
			msgq(sp, M_SYSERR, "suspend: tcsetattr current");
			rval = 1;
		}
		break;
	case S_VI:
		/*
		 * This part of the function isn't needed by any screen not
		 * supporting vi process suspension, i.e. any screen that isn't
		 * backed by a UNIX shell.  Returning 1 for failure will cause
		 * the editor to reject the command.
		 *
		 * Temporarily end the screen.
		 */
		(void)endwin();

		/* Stop the process group. */
		if (rval = kill(0, SIGTSTP))
			msgq(sp, M_SYSERR, "suspend: kill");

		/* Time passes ... */

		/* Refresh the screen. */
		clearok(curscr, 1);
		refresh();

		/* If the screen changed size, set the SIGWINCH bit. */
		if (!cl_ssize(sp, 1, NULL, NULL))
			F_SET(CLP(sp), CL_SIGWINCH);
		break;
	default:
		abort();
	}
	return (rval);
}

/*
 * cl_lline_copy --
 *	Get a copy of the current last line.
 */
static int
cl_lline_copy(sp, lenp, bufpp, blenp)
	SCR *sp;
	size_t *lenp, *blenp;
	CHAR_T **bufpp;
{
	CHAR_T *p, ch;
	size_t col, lno,oldx, oldy, spcnt;

	/*
	 * XXX
	 * We could save a copy each time the last line is written, but it
	 * would be a lot more work and I don't expect to do this very often.
	 *
	 * Allocate enough memory to hold the line.
	 */
	BINC_RET(sp, *bufpp, *blenp, O_VAL(sp, O_COLUMNS));

	/*
	 * Walk through the line, retrieving each character.  Since curses
	 * has no EOL marker, keep track of strings of spaces, and copy the
	 * trailing spaces only if there's another non-space character.
	 */
	getyx(stdscr, oldy, oldx);
	lno = RLNO(sp, INFOLINE(sp));
	for (p = *bufpp, col = spcnt = 0;;) {
		(void)move(lno, col);
		ch = winch(stdscr);
		if (isblank(ch))
			++spcnt;
		else {
			for (; spcnt > 0; --spcnt)
				*p++ = ' ';
			*p++ = ch;
		}
		if (++col >= sp->cols)
			break;
	}
	(void)move(oldy, oldx);

	*lenp = p - *bufpp;
	return (0);
}

#ifdef DEBUG
/*
 * gdbrefresh --
 *	Stub routine so can flush out curses screen changes using gdb.
 */
int
gdbrefresh()
{
	refresh();
	return (0);		/* XXX Convince gdb to run it. */
}
#endif
