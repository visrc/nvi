/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_funcs.c,v 8.10 1995/05/26 09:09:37 bostic Exp $ (Berkeley) $Date: 1995/05/26 09:09:37 $";
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

/*
 * cl_addnstr --
 *	Add len bytes from the string at the cursor.
 *
 * PUBLIC: int cl_addnstr __P((SCR *, const char *, size_t));
 */
int
cl_addnstr(sp, str, len)
	SCR *sp;
	const char *str;
	size_t len;
{
	EX_ABORT(sp);
	VI_INIT_IGNORE(sp);

	if (addnstr(str, len) == ERR) {
		msgq(sp, M_ERR, "Error: addnstr: %.*s", (int)len, str);
		return (1);
	}
	return (0);
}
/*
 * cl_addstr --
 *	Add the string at the current cursor.
 *
 * PUBLIC: int cl_addstr __P((SCR *, const char *));
 */
int
cl_addstr(sp, str)
	SCR *sp;
	const char *str;
{
	EX_ABORT(sp);
	VI_INIT_IGNORE(sp);

	if (addstr(str) == ERR) {
		msgq(sp, M_ERR, "Error: addstr: %s", str);
		return (1);
	}
	return (0);
}

/*
 * cl_attr --
 *	Toggle a screen attribute.
 *
 * PUBLIC: int cl_attr __P((SCR *, attr_t, int));
 */
int
cl_attr(sp, attribute, on)
	SCR *sp;
	attr_t attribute;
	int on;
{
	CL_PRIVATE *clp;

	EX_INIT_IGNORE(sp);
	VI_INIT_IGNORE(sp);

	clp = CLP(sp);
	switch (attribute) {
	case SA_INVERSE:
		if (F_ISSET(sp, S_EX)) {
			if (clp->SO == NULL)
				return (1);
			if (on)
				(void)tputs(clp->SO, 1, cl_putchar);
			else
				(void)tputs(clp->SE, 1, cl_putchar);
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
 *	Ring the bell.
 *
 * PUBLIC: int cl_bell __P((SCR *));
 */
int
cl_bell(sp)
	SCR *sp;
{
	if (F_ISSET(sp, S_EX)) {
		(void)write(STDOUT_FILENO, "\07", 1);		/* \a */
		return (0);
	}

	VI_INIT_IGNORE(sp);

#ifdef SYSV_CURSES
	if (O_ISSET(sp, O_FLASH))
		flash();
	else
		beep();
#else
	if (O_ISSET(sp, O_FLASH) && CLP(sp)->VB != NULL) {
		(void)tputs(CLP(sp)->VB, 1, cl_putchar);
		(void)fflush(stdout);
	} else
		(void)write(STDOUT_FILENO, "\07", 1);		/* \a */
#endif
	return (0);
}

/*
 * cl_busy --
 *	Put up a busy message.
 *
 * PUBLIC: int cl_busy __P((SCR *, const char *, int));
 */
int
cl_busy(sp, msg, on)
	SCR *sp;
	const char *msg;
	int on;
{
	CL_PRIVATE *clp;
	size_t notused;

	if (F_ISSET(sp, S_EX | S_EX_CANON)) {
		if (on)
			(void)write(STDOUT_FILENO, msg, strlen(msg));
		else
			(void)write(STDOUT_FILENO, "\n", 1);
		return (0);
	}

	VI_INIT_IGNORE(sp);

	clp = CLP(sp);
	if (on) {
		clp->busy_state = 0;
		getyx(stdscr, clp->busy_y, clp->busy_x);
		(void)move(RLNO(sp, INFOLINE(sp)), 0);
		getyx(stdscr, notused, clp->busy_fx);

		/*
		 * If there's no message, just rest the cursor without
		 * setting the flag so the main loop doesn't update it.
		 */
		if (msg == NULL)
			(void)refresh();
		else {
			(void)addstr(msg);
			(void)clrtoeol();
			getyx(stdscr, notused, clp->busy_fx);
			F_SET(clp, CL_BUSY);
			(void)cl_busy_update(sp, 1);
		}
	} else
		if (F_ISSET(clp, CL_BUSY)) {
			F_CLR(clp, CL_BUSY);
			(void)move(clp->busy_y, clp->busy_x);
			(void)refresh();
		}
	return (0);
}

/*
 * cl_busy_update --
 *	Update the busy message.
 *
 * PUBLIC: int cl_busy_update __P((SCR *, int));
 */
int
cl_busy_update(sp, init)
	SCR *sp;
	int init;
{
	static const char flagc[] = "|/-|-\\";
	struct timeval tv;
	CL_PRIVATE *clp;

	EX_NOOP(sp);
	VI_INIT_IGNORE(sp);

	clp = CLP(sp);

	/* Update no more than every 1/4 of a second. */
	if (!init) {
		(void)gettimeofday(&tv, NULL);
		if (((tv.tv_sec - clp->busy_tv.tv_sec) * 1000000 +
		    (tv.tv_usec - clp->busy_tv.tv_usec)) < 4000)
			return (0);
	}

	(void)move(RLNO(sp, INFOLINE(sp)), clp->busy_fx);
	if (init) {
		(void)addnstr("+", 1);
		clp->busy_state = 0;
		(void)gettimeofday(&clp->busy_tv, NULL);
	} else {
		if (clp->busy_state == sizeof(flagc))
			clp->busy_state = 0;
		(void)addnstr(flagc + clp->busy_state++, 1);
	}
	(void)move(RLNO(sp, INFOLINE(sp)), clp->busy_fx);
	(void)refresh();
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
	EX_NOOP(sp);
	VI_INIT_IGNORE(sp);

	return (enter ? cl_ex_tinit(sp) : cl_ex_tend(sp));
}

/*
 * cl_clear --
 *	Clear the screen.
 *
 * PUBLIC: int cl_clear __P((SCR *));
 */
int
cl_clear(sp)
	SCR *sp;
{
	EX_NOOP(sp);
	VI_INIT_IGNORE(sp);

	return (clear() == ERR);
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
	EX_NOOP(sp);
	VI_INIT_IGNORE(sp);

	return (clrtoeol() == ERR);
}

/*
 * cl_clrtoeos --
 *	Clear from the current line to the end of the screen.
 *
 * PUBLIC: int cl_clrtoeos __P((SCR *));
 */
int
cl_clrtoeos(sp)
	SCR *sp;
{
	EX_NOOP(sp);
	VI_INIT_IGNORE(sp);

	return (clrtobot() == ERR);
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
	size_t oldy;

	EX_ABORT(sp);
	VI_INIT_IGNORE(sp);

	/*
	 * Adjust to be relative to the current screen -- this allows vi to
	 * call relative to the current screen.
	 */
	getyx(stdscr, oldy, *xp);
	*yp = oldy - sp->woff;
	return (0);
}

/*
 * cl_deleteln --
 *	Delete the current line, scrolling all lines below it.  The bottom
 *	line becomes blank.
 *
 * PUBLIC: int cl_deleteln __P((SCR *));
 */
int
cl_deleteln(sp)
	SCR *sp;
{
	EX_ABORT(sp);
	VI_INIT_IGNORE(sp);

	return (deleteln() == ERR);
}

/* 
 * cl_exadjust --
 *	Adjust the screen for ex.  All special purpose, all special case.
 *
 * PUBLIC: int cl_exadjust __P((SCR *, exadj_t));
 */
int
cl_exadjust(sp, action)
	SCR *sp;
	exadj_t action;
{
	CL_PRIVATE *clp;
	int cnt;

	VI_ABORT(sp);

	clp = CLP(sp);
	switch (action) {
	case EX_TERM_SCROLL:
		/* Move the cursor up one line if that's possible. */
		if (clp->UP != NULL)
			(void)tputs(clp->UP, 1, cl_putchar);
		else if (clp->CM != NULL)
			(void)tputs(tgoto(clp->CM,
			    0, O_VAL(sp, O_LINES) - 2), 1, cl_putchar);
		else
			return (0);
		/* FALLTHROUGH */
	case EX_TERM_CE:
		if (clp->CE != NULL) {
			(void)putchar('\r');
			(void)tputs(clp->CE, 1, cl_putchar);
		} else {
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
 *	Get a single key from the terminal.
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
		F_SET(sp->gp, G_INTERRUPTED);
		break;
	case INP_EOF:
	case INP_ERR:
		break;
	case INP_TIMEOUT:
		abort();
	}
	return (1);
}

/*
 * cl_insertln --
 *	Push down the current line, discarding the bottom line.  The current
 *	line becomes blank.
 *
 * PUBLIC: int cl_insertln __P((SCR *));
 */
int
cl_insertln(sp)
	SCR *sp;
{
	EX_ABORT(sp);
	VI_INIT_IGNORE(sp);

	return (insertln() == ERR);
}

/*
 * cl_lattr --
 *	Toggle a screen attribute for an entire line.
 *
 * PUBLIC: int cl_lattr __P((SCR *, size_t, attr_t, int));
 */
int
cl_lattr(sp, lno, attribute, on)
	SCR *sp;
	size_t lno;
	attr_t attribute;
	int on;
{
	size_t spcnt, col;
	char ch;

	EX_ABORT(sp);
	VI_INIT_IGNORE(sp);

	switch (attribute) {
	case SA_INVERSE:
		/*
		 * Walk through the line, retrieving each character and writing
		 * it back out in inverse video.  Since curses doesn't have an
		 * EOL marker, keep track of strings of spaces, and only put
		 * out trailing spaces if find another character.  The steps
		 * that get us here are as follows:
		 *
		 * + The user splits the screen and edits in any other than the
		 *   bottom screen.
		 * + The user enters ex commands, but not ex mode, so the ex
		 *   output is piped through vi/curses.
		 * + The ex output is only a single line.
		 * + When ex returns into vi, vi has already output the line,
		 *   and it needs to be in reverse video to show the separation
		 *   between the screens.
		 *
		 * XXX
		 * This is a major kluge -- it would be nice if curses had an
		 * interface that let us change attributes on a per line basis.
		 */
		lno = RLNO(sp, lno);
		(void)move(lno, 0);
		if (on)
			(void)standout();
		for (spcnt = col = 0;;) {
			ch = winch(stdscr);
			if (isspace(ch)) {
				++spcnt;
				if (++col >= sp->cols)
					break;
				(void)move(lno, col);
			} else {
				if (spcnt) {
					(void)move(lno, col - spcnt);
					for (; spcnt > 0; --spcnt)
						(void)addnstr(" ", 1);
				}
				(void)addnstr(&ch, 1);
				if (++col >= sp->cols)
					break;
			}
		}
		if (on)
			(void)standend();
		break;
	default:
		abort();
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
	VI_INIT_IGNORE(sp);

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
 * PUBLIC: int cl_refresh __P((SCR *));
 */
int
cl_refresh(sp)
	SCR *sp;
{
	EX_NOOP(sp);
	VI_INIT_IGNORE(sp);

	return (refresh() == ERR);
}

/*
 * cl_repaint --
 *	Repaint the screen as of its last appearance.
 *
 * PUBLIC: int cl_repaint __P((SCR *));
 */
int
cl_repaint(sp)
	SCR *sp;
{
	EX_NOOP(sp);
	VI_INIT_IGNORE(sp);

	return (wrefresh(curscr) == ERR);
}

#ifdef DEBUG
/*
 * gdbrefresh --
 *	Stub routine so can flush out screen changes using gdb.
 */
int
gdbrefresh()
{
	refresh();
	return (0);		/* XXX Convince gdb to run it. */
}
#endif
