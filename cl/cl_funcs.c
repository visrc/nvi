/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_funcs.c,v 10.25 1995/11/06 09:58:12 bostic Exp $ (Berkeley) $Date: 1995/11/06 09:58:12 $";
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

#include "../common/common.h"
#include "../vi/vi.h"
#include "cl.h"

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
	int iv;

	EX_ABORT(sp);
	VI_INIT_ABORT(sp);

	/*
	 * If it's the last line of the screen and it's a split screen,
	 * use inverse video.
	 */
	iv = 0;
	getyx(stdscr, oldy, oldx);
	if (oldy == RLNO(sp, LASTLINE(sp))) {
		clp = CLP(sp);
		if (IS_SPLIT(sp)) {
			iv = 1;
			(void)standout();
			F_SET(clp, CL_LLINE_IV);
		} else
			F_CLR(clp, CL_LLINE_IV);
	}

	if (addnstr(str, len) == ERR)
		return (1);

	if (iv)
		(void)standend();
	return (0);
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

	EX_INIT_ABORT(sp);
	VI_INIT_ABORT(sp);

	switch (attribute) {
	case SA_INVERSE:
		if (F_ISSET(sp, S_EX | S_EX_CANON)) {
			clp = CLP(sp);
			if (clp->smso == NULL)
				return (1);
			if (on)
				(void)tputs(clp->smso, 1, cl_putchar);
			else
				(void)tputs(clp->rmso, 1, cl_putchar);
			(void)fflush(stdout);
		} else {
			if (on)
				(void)standout();
			else
				(void)standend();
		}
		break;
	default:
		abort();
	}
	return (0);
}

/*
 * cl_baud --
 *	Return the baud rate.
 *
 * PUBLIC: int cl_baud __P((SCR *, u_long *));
 */
int
cl_baud(sp, ratep)
	SCR *sp;
	u_long *ratep;
{
	CL_PRIVATE *clp;

	/*
	 * XXX
	 * There's no portable way to get a "baud rate" -- cfgetospeed(3)
	 * returns the value associated with some #define, which we may
	 * never have heard of, or which may be a purely local speed.  Vi
	 * only cares if it's SLOW (w300), slow (w1200) or fast (w9600).
	 * Try and detect the slow ones, and default to fast.
	 */
	clp = CLP(sp);
	switch (cfgetospeed(&clp->orig)) {
	case B50:
	case B75:
	case B110:
	case B134:
	case B150:
	case B200:
	case B300:
	case B600:
		*ratep = 600;
		break;
	case B1200:
		*ratep = 1200;
		break;
	default:
		*ratep = 9600;
		break;
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
	EX_INIT_ABORT(sp);
	VI_INIT_ABORT(sp);

	if (F_ISSET(sp, S_EX | S_EX_CANON))
		(void)write(STDOUT_FILENO, "\07", 1);		/* \a */
	else {
		/*
		 * Vi has an edit option which determines if the terminal
		 * should be beeped or the screen flashed.
		 */
		if (O_ISSET(sp, O_FLASH))
			(void)flash();
		else
			(void)beep();
	}
	return (0);
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
	EX_ABORT(sp);
	VI_INIT_ABORT(sp);

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
	VI_INIT_ABORT(sp);

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
 * cl_deleteln --
 *	Delete the current line, scrolling all lines below it.
 *
 * PUBLIC: int cl_deleteln __P((SCR *));
 */
int
cl_deleteln(sp)
	SCR *sp;
{
	CHAR_T ch;
	CL_PRIVATE *clp;
	size_t col, lno, spcnt, oldy, oldx;

	EX_ABORT(sp);
	VI_INIT_ABORT(sp);

	clp = CLP(sp);

	/*
	 * This clause is required because the curses screen uses reverse
	 * video to delimit split screens.  If the screen does not do this,
	 * this code won't be necessary.
	 *
	 * If the bottom line was in reverse video, rewrite it in normal
	 * video before it's scrolled.
	 *
	 * Check for the existence of a chgat function; XSI requires it, but
	 * historic implementations of System V curses don't.   If it's not
	 * a #define, we'll fall back to doing it by hand, which is slow but
	 * acceptable.
	 *
	 * By hand means walking through the line, retrieving and rewriting
	 * each character.  Curses has no EOL marker, so track strings of
	 * spaces, and copy the trailing spaces only if there's a non-space
	 * character following.
	 */
	if (F_ISSET(clp, CL_LLINE_IV)) {
		getyx(stdscr, oldy, oldx);
#ifdef mvchgat
		mvchgat(RLNO(sp, LASTLINE(sp)), 0, -1, A_NORMAL, 0, NULL);
#else
		for (lno = RLNO(sp, LASTLINE(sp)), col = spcnt = 0;;) {
			(void)move(lno, col);
			ch = winch(stdscr);
			if (isblank(ch))
				++spcnt;
			else {
				for (; spcnt > 0; --spcnt)
					(void)addch(' ');
				(void)addch(ch);
			}
			if (++col >= sp->cols)
				break;
		}
		F_CLR(clp, CL_LLINE_IV);
#endif
		(void)move(oldy, oldx);
	}

	/*
	 * The bottom line is expected to be blank after this operation,
	 * and other screens must support that semantic.
	 */
	return (deleteln() == ERR);
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
	 * purpose, all special case.  Screens not supporting a standalone
	 * ex mode should replace it with an EX_ABORT macro.
	 */
	VI_ABORT(sp);
	EX_INIT_ABORT(sp);

	clp = CLP(sp);
	switch (action) {
	case EX_TERM_SCROLL:
		/* Move the cursor up one line if that's possible. */
		if (clp->cuu1 != NULL)
			(void)tputs(clp->cuu1, 1, cl_putchar);
		else if (clp->cup != NULL)
			(void)tputs(tgoto(clp->cup,
			    0, LINES - 2), 1, cl_putchar);
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
 * cl_insertln --
 *	Push down the current line, discarding the bottom line.
 *
 * PUBLIC: int cl_insertln __P((SCR *));
 */
int
cl_insertln(sp)
	SCR *sp;
{
	EX_ABORT(sp);
	VI_INIT_ABORT(sp);

	/*
	 * The current line is expected to be blank after this operation,
	 * and the screen must support that semantic.
	 */
	return (insertln() == ERR);
}

/*
 * cl_keyval --
 *	Return the value for a special key.
 *
 * PUBLIC: int cl_keyval __P((SCR *, scr_keyval_t, CHAR_T *, int *));
 */
int
cl_keyval(sp, val, chp, dnep)
	SCR *sp;
	scr_keyval_t val;
	CHAR_T *chp;
	int *dnep;
{
	CL_PRIVATE *clp;

	/*
	 * VEOF, VERASE and VKILL are required by POSIX 1003.1-1990,
	 * VWERASE is a 4BSD extension.
	 */
	clp = CLP(sp);
	switch (val) {
	case KEY_VEOF:
		*dnep = (*chp = clp->orig.c_cc[VEOF]) == _POSIX_VDISABLE;
		break;
	case KEY_VERASE:
		*dnep = (*chp = clp->orig.c_cc[VERASE]) == _POSIX_VDISABLE;
		break;
	case KEY_VKILL:
		*dnep = (*chp = clp->orig.c_cc[VKILL]) == _POSIX_VDISABLE;
		break;
#ifdef VWERASE
	case KEY_VWERASE:
		*dnep = (*chp = clp->orig.c_cc[VWERASE]) == _POSIX_VDISABLE;
		break;
#endif
	default:
		*dnep = 1;
		break;
	}
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
	VI_INIT_ABORT(sp);

	/* See the comment in cl_cursor. */
	if (move(RLNO(sp, lno), cno) == ERR) {
		msgq(sp, M_ERR,
		    "Error: move: l(%u) c(%u) o(%u)", lno, cno, sp->woff);
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
	EX_ABORT(sp);
	VI_INIT_ABORT(sp);

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
 * cl_rename --
 *	Rename the file.
 *
 * PUBLIC: int cl_rename __P((SCR *));
 */
int
cl_rename(sp)
	SCR *sp;
{
	return (0);			/* Curses doesn't care. */
}

/*
 * cl_suspend --
 *	Suspend a screen.
 *
 * PUBLIC: int cl_suspend __P((SCR *, int *));
 */
int
cl_suspend(sp, allowedp)
	SCR *sp;
	int *allowedp;
{
	struct termios t;
	CL_PRIVATE *clp;
	GS *gp;
	size_t oldy, oldx;
	int changed;

	EX_INIT_ABORT(sp);
	VI_INIT_ABORT(sp);

	*allowedp = 1;

	/*
	 * XXX
	 * All of this is done completely behind curses back.  You can't
	 * portably stop and restart curses screens, and even hope to get
	 * it right.  End of story.
	 *
	 * Save the terminal settings, and restore the original ones.
	 */
	gp = sp->gp;
	if (F_ISSET(gp, G_STDIN_TTY)) {
		if (tcgetattr(STDIN_FILENO, &t)) {
			msgq(sp, M_SYSERR, "suspend: tcgetattr");
			return (1);
		}
		clp = CLP(sp);
		if (tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &clp->orig)) {
			msgq(sp, M_SYSERR, "suspend: tcsetattr original");
			return (1);
		}
	}

	/* Move to the lower left-hand corner of the screen. */
	if (F_ISSET(sp, S_VI)) {
		getyx(stdscr, oldy, oldx);
		(void)move(LINES - 1, 0);
		(void)refresh();
	}

	/* Stop the process group. */
	if (kill(0, SIGTSTP)) {
		msgq(sp, M_SYSERR, "suspend: kill");
		return (1);
	}

	/* Time passes ... */

	/* Restore terminal settings. */
	if (F_ISSET(gp, G_STDIN_TTY) &&
	    tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &t)) {
		msgq(sp, M_SYSERR, "suspend: tcsetattr current");
		return (1);
	}

	/* Refresh the screen; if it changed size, set the SIGWINCH bit. */
	if (F_ISSET(sp, S_VI)) {
		(void)move(oldy, oldx);
		(void)cl_refresh(sp, 1);

		if (cl_ssize(sp, 1, NULL, NULL, &changed))
			return (1);
		if (changed)
			F_SET(CLP(sp), CL_SIGWINCH);
	}
	return (0);
}

/*
 * cl_usage --
 *	Print out the curses usage messages.
 * 
 * PUBLIC: void cl_usage __P((void));
 */
void
cl_usage()
{
#define	USAGE "\
usage: ex [-eFRrsv] [-c command] [-t tag] [-w size] [file ...]\n\
usage: vi [-eFlRrv] [-c command] [-t tag] [-w size] [file ...]\n"
	(void)fprintf(stderr, "%s", USAGE);
#undef	USAGE
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
