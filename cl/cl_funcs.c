/*-
 * Copyright (c) 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_funcs.c,v 8.1 1995/01/23 17:01:50 bostic Exp $ (Berkeley) $Date: 1995/01/23 17:01:50 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <curses.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "cl.h"

/*
 * search.c:f_search() is called from ex/ex_tag.c:ex_tagfirst(),
 * which runs before the screen really exists, and try to access
 * the screen.  Make sure we don't step on anything.
 *
 * If the terminal isn't initialized, there's nothing to do.
 */
#define	CINIT								\
	if (!F_ISSET(CLP(sp), CL_CURSES_INIT))				\
		return (0);

/*
 * cl_addnstr --
 *	Add len bytes from the string at the cursor.
 */
int
cl_addnstr(sp, str, len)
	SCR *sp;
	char *str;
	size_t len;
{
	CINIT;

	if (addnstr(str, len) == ERR) {
		msgq(sp, M_ERR, "Error: addnstr: %.*s\n", (int)len, str);
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
	char *str;
{
	CINIT;

	if (addstr(str) == ERR) {
		msgq(sp, M_ERR, "Error: addstr: %s\n", str);
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
	CINIT;

#ifdef SYSV_CURSES
	if (O_ISSET(sp, O_FLASH))
		flash();
	else
		beep();
#else
	if (O_ISSET(sp, O_FLASH) && CLP(sp)->VB != NULL) {
		(void)tputs(CLP(sp)->VB, 1, vi_putchar);
		(void)fflush(stdout);
	} else
		(void)write(STDOUT_FILENO, "\007", 1);	/* '\a' */
#endif
	F_CLR(sp, S_BELLSCHED);
	return (0);
}

/*
 * cl_clear --
 *	Clear the screen.
 */
int
cl_clear(sp)
	SCR *sp;
{
	CINIT;

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
	CINIT;

	return (clrtoeol() == ERR);
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
	CINIT;

	getyx(stdscr, *yp, *xp);
	return (0);
}

/*
 * cl_deleteln --
 *	Delete the current line, scrolling all lines below it.  The
 *	bottom line becomes blank.
 */
int
cl_deleteln(sp)
	SCR *sp;
{
	CINIT;

	return (deleteln() == ERR);
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
	CINIT;

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
	CINIT;

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
	CINIT;

	if (move(lno, cno) == ERR) {
		msgq(sp, M_ERR, "Error: move: l(%u) c(%u) o(%u)\n",
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
	CINIT;

	return (refresh() == ERR);
}

/*
 * cl_restore --
 *	Restore the screen to its last appearance.
 */
int
cl_restore(sp)
	SCR *sp;
{
	CINIT;

	return (wrefresh(curscr) == ERR);
}

/*
 * cl_winch --
 *	Return the character at the current cursor.
 *
 * XXX
 * This is completely wrong, and should be deleted.
 */
int
cl_winch(sp, chp)
	SCR *sp;
	CHAR_T *chp;
{
	CINIT;

	*chp = winch(stdscr);
	return (0);
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
