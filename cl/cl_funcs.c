/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_funcs.c,v 8.9 1995/04/13 17:19:47 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:19:47 $";
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
#include "../vi/vi.h"
#include "cl.h"

/*
 * cl_addnstr --
 *	Add len bytes from the string at the cursor.
 */
int
cl_addnstr(sp, str, len)
	SCR *sp;
	const char *str;
	size_t len;
{
	EX_ABORT;
	VI_INIT;

	if (addnstr(str, len) == ERR) {
		msgq(sp, M_ERR, "Error: addnstr: %.*s", (int)len, str);
		return (1);
	}
	return (0);
}
/*
 * cl_addstr --
 *	Add the string at the current cursor.
 */
int
cl_addstr(sp, str)
	SCR *sp;
	const char *str;
{
	EX_ABORT;
	VI_INIT;

	if (addstr(str) == ERR) {
		msgq(sp, M_ERR, "Error: addstr: %s", str);
		return (1);
	}
	return (0);
}

/*
 * cl_bell --
 *	Ring the bell.
 */
int
cl_bell(sp)
	SCR *sp;
{
	F_CLR(sp, S_BELLSCHED);

	if (F_ISSET(sp, S_EX)) {
		(void)write(STDOUT_FILENO, "\07", 1);		/* \a */
		return (0);
	}

	VI_INIT;

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
 */
int
cl_busy(sp, msg, on)
	SCR *sp;
	const char *msg;
	int on;
{
	CL_PRIVATE *clp;
	size_t notused;

	if (F_ISSET(sp, S_EX))
		return (0);

	VI_INIT;

	clp = CLP(sp);
	if (on) {
		clp->busy_state = 0;
		getyx(stdscr, clp->busy_y, clp->busy_x);
		move(RLNO(sp, INFOLINE(sp)), 0);
		getyx(stdscr, notused, clp->busy_fx);

		/*
		 * If there's no message, just rest the cursor without
		 * setting the flag so the main loop doesn't update it.
		 */
		if (msg == NULL)
			refresh();
		else {
			(void)addstr(msg);
			(void)clrtoeol();
			getyx(stdscr, notused, clp->busy_fx);
			F_SET(clp, CL_BUSY);

			cl_busy_update(sp);
		}
	} else {
		F_CLR(clp, CL_BUSY);
		move(clp->busy_y, clp->busy_x);
		refresh();
	}
	return (0);
}

/*
 * cl_busy_update --
 *	Update the busy message.
 */
void
cl_busy_update(sp)
	SCR *sp;
{
	static const char flagc[] = "|/-|-\\";
	struct timeval tv;
	CL_PRIVATE *clp;

	clp = CLP(sp);

	/* Update every 1/4 of a second. */
	(void)gettimeofday(&tv, NULL);
	if (((tv.tv_sec - clp->busy_tv.tv_sec) * 1000000 +
	    (tv.tv_usec - clp->busy_tv.tv_usec)) < 4000)
		return;

	if (clp->busy_state == sizeof(flagc))
		clp->busy_state = 0;
	move(RLNO(sp, INFOLINE(sp)), clp->busy_fx);
	(void)addnstr(flagc + clp->busy_state++, 1);
	move(RLNO(sp, INFOLINE(sp)), clp->busy_fx);
	refresh();
}

/*
 * cl_canon --
 *	Enter/leave tty canonical mode.
 */
int
cl_canon(sp, enter)
	SCR *sp;
	int enter;
{
	EX_NOOP;

	VI_INIT;
	return (enter ? cl_ex_tinit(sp) : cl_ex_tend(sp));
}

/*
 * cl_clear --
 *	Clear the screen.
 */
int
cl_clear(sp)
	SCR *sp;
{
	EX_NOOP;

	VI_INIT;
	return (clear() == ERR);
}

/*
 * cl_clrtoeol --
 *	Clear from the current cursor to the end of the line.
 */
int
cl_clrtoeol(sp)
	SCR *sp;
{
	EX_NOOP;

	VI_INIT;
	return (clrtoeol() == ERR);
}

/*
 * cl_clrtoeos --
 *	Clear to the end of the screen.
 */
int
cl_clrtoeos(sp)
	SCR *sp;
{
	EX_NOOP;

	VI_INIT;
	if (clrtobot() == ERR)
		return (1);
	/*
	 * XXX
	 * This is used when temporarily exiting the editor to an
	 * outside application, so we refresh here.
	 */
	refresh();
	return (0);
}

/*
 * cl_confirm --
 *	Get confirmation from the user.
 */
conf_t
cl_confirm(sp)
	SCR *sp;
{
	CHAR_T ch;
	EVENT *evp;
	size_t oldy, oldx;

	if (F_ISSET(sp, S_EX)) {
		/* __TK__ */
		return (CONF_QUIT);
	}

	VI_INIT;

	getyx(stdscr, oldy, oldx);
	(void)move(RLNO(sp, INFOLINE(sp)), 0);
	(void)clrtoeol();
	(void)addnstr(STR_CONFIRM, sizeof(STR_CONFIRM) - 1);
	(void)move(oldy, oldx);
	(void)refresh();

	if (v_event_pull(sp, &evp))
		ch = evp->e_c;
	else
		if (cl_getkey(sp, &ch))
			return (CONF_QUIT);
	if (ch == CH_YES)
		return (CONF_YES);
	if (ch == CH_QUIT)
		return (CONF_QUIT);
	return (CONF_NO);
}

/*
 * cl_cursor --
 *	Return the current cursor position.
 */
int
cl_cursor(sp, yp, xp)
	SCR *sp;
	size_t *yp, *xp;
{
	size_t oldy;

	EX_ABORT;

	VI_INIT;
	getyx(stdscr, oldy, *xp);

	/*
	 * Adjust to be relative to the current screen -- this allows vi to
	 * call relative to the current screen.
	 */
	*yp = oldy - sp->woff;
	return (0);
}

/*
 * cl_deleteln --
 *	Delete the current line, scrolling all lines below it.  The bottom
 *	line becomes blank.
 */
int
cl_deleteln(sp)
	SCR *sp;
{
	EX_ABORT;

	VI_INIT;
	return (deleteln() == ERR);
}

/* 
 * cl_exadjust --
 *	Adjust the screen for ex.  All special purpose, all special case.
 */
int
cl_exadjust(sp, action)
	SCR *sp;
	exadj_t action;
{
	CL_PRIVATE *clp;
	int cnt;

	VI_ABORT;

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
 * cl_insertln --
 *	Push down the current line, discarding the bottom line.  The
 *	current line becomes blank.
 */
int
cl_insertln(sp)
	SCR *sp;
{
	EX_ABORT;

	VI_INIT;
	return (insertln() == ERR);
}

/*
 * cl_inverse --
 *	Turn inverse video on or off.
 */
int
cl_inverse(sp, on)
	SCR *sp;
	int on;
{
	CL_PRIVATE *clp;

	if (F_ISSET(sp, S_EX)) {
		EX_INIT;

		clp = CLP(sp);
		if (clp->SO == NULL)
			return (1);
		if (on)
			(void)tputs(clp->SO, 1, cl_putchar);
		else
			(void)tputs(clp->SE, 1, cl_putchar);
		return (0);
	}

	VI_INIT;

	if (on)
		return (standout() == ERR);
	else
		return (standend() == ERR);
	/* NOTREACHED */
}

/*
 * cl_move --
 *	Move the cursor.
 */
int
cl_move(sp, lno, cno)
	SCR *sp;
	size_t lno, cno;
{
	EX_ABORT;

	VI_INIT;
	if (move(RLNO(sp, lno), cno) == ERR) {
		msgq(sp, M_ERR, "Error: move: l(%u) c(%u) o(%u)",
		    lno, cno, sp->woff);
		return (1);
	}
	return (0);
}

/*
 * cl_refresh --
 *	Refresh the screen.
 */
int
cl_refresh(sp)
	SCR *sp;
{
	EX_NOOP;

	VI_INIT;
	return (refresh() == ERR);
}

/*
 * cl_repaint --
 *	Repaint the screen as of its last appearance.
 */
int
cl_repaint(sp)
	SCR *sp;
{
	EX_NOOP;

	VI_INIT;
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
