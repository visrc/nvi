/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ip_funcs.c,v 8.12 1996/12/18 10:28:02 bostic Exp $ (Berkeley) $Date: 1996/12/18 10:28:02 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/common.h"
#include "../vi/vi.h"
#include "../ipc/ip.h"
#include "extern.h"

/*
 * ip_addstr --
 *	Add len bytes from the string at the cursor, advancing the cursor.
 *
 * PUBLIC: int ip_addstr __P((SCR *, const char *, size_t));
 */
int
ip_addstr(sp, str, len)
	SCR *sp;
	const char *str;
	size_t len;
{
	IP_BUF ipb;
	IP_PRIVATE *ipp;
	int iv, rval;

	ipp = IPP(sp);

	/*
	 * If ex isn't in control, it's the last line of the screen and
	 * it's a split screen, use inverse video.
	 */
	iv = 0;
	if (!F_ISSET(sp, SC_SCR_EXWROTE) &&
	    ipp->row == LASTLINE(sp) && IS_SPLIT(sp)) {
		iv = 1;
		ip_attr(sp, SA_INVERSE, 1);
	}
	ipb.code = SI_ADDSTR;
	ipb.len1 = len;
	ipb.str1 = str;
	rval = vi_send("a", &ipb);

	if (iv)
		ip_attr(sp, SA_INVERSE, 0);
	return (rval);
}

/*
 * ip_attr --
 *	Toggle a screen attribute on/off.
 *
 * PUBLIC: int ip_attr __P((SCR *, scr_attr_t, int));
 */
int
ip_attr(sp, attribute, on)
	SCR *sp;
	scr_attr_t attribute;
	int on;
{
	IP_BUF ipb;

	ipb.code = SI_ATTRIBUTE;
	ipb.val1 = attribute;
	ipb.val2 = on;

	return (vi_send("12", &ipb));
}

/*
 * ip_baud --
 *	Return the baud rate.
 *
 * PUBLIC: int ip_baud __P((SCR *, u_long *));
 */
int
ip_baud(sp, ratep)
	SCR *sp;
	u_long *ratep;
{
	*ratep = 9600;		/* XXX: Translation: fast. */
	return (0);
}

/*
 * ip_bell --
 *	Ring the bell/flash the screen.
 *
 * PUBLIC: int ip_bell __P((SCR *));
 */
int
ip_bell(sp)
	SCR *sp;
{
	IP_BUF ipb;

	ipb.code = SI_BELL;

	return (vi_send(NULL, &ipb));
}

/*
 * ip_busy --
 *	Display a busy message.
 *
 * PUBLIC: void ip_busy __P((SCR *, const char *, busy_t));
 */
void
ip_busy(sp, str, bval)
	SCR *sp;
	const char *str;
	busy_t bval;
{
	IP_BUF ipb;

	switch (bval) {
	case BUSY_ON:
		ipb.code = SI_BUSY_ON;
		ipb.len1 = strlen(str);
		ipb.str1 = str;
		(void)vi_send("a1", &ipb);
		break;
	case BUSY_OFF:
		ipb.code = SI_BUSY_OFF;
		(void)vi_send(NULL, &ipb);
		break;
	case BUSY_UPDATE:
		break;
	}
	return;
}

/*
 * ip_clrtoeol --
 *	Clear from the current cursor to the end of the line.
 *
 * PUBLIC: int ip_clrtoeol __P((SCR *));
 */
int
ip_clrtoeol(sp)
	SCR *sp;
{
	IP_BUF ipb;

	ipb.code = SI_CLRTOEOL;

	return (vi_send(NULL, &ipb));
}

/*
 * ip_cursor --
 *	Return the current cursor position.
 *
 * PUBLIC: int ip_cursor __P((SCR *, size_t *, size_t *));
 */
int
ip_cursor(sp, yp, xp)
	SCR *sp;
	size_t *yp, *xp;
{
	IP_PRIVATE *ipp;

	ipp = IPP(sp);
	*yp = ipp->row;
	*xp = ipp->col;
	return (0);
}

/*
 * ip_deleteln --
 *	Delete the current line, scrolling all lines below it.
 *
 * PUBLIC: int ip_deleteln __P((SCR *));
 */
int
ip_deleteln(sp)
	SCR *sp;
{
	IP_BUF ipb;

	/*
	 * This clause is required because the curses screen uses reverse
	 * video to delimit split screens.  If the screen does not do this,
	 * this code won't be necessary.
	 *
	 * If the bottom line was in reverse video, rewrite it in normal
	 * video before it's scrolled.
	 */
	if (!F_ISSET(sp, SC_SCR_EXWROTE) && IS_SPLIT(sp)) {
		ipb.code = SI_REWRITE;
		ipb.val1 = RLNO(sp, LASTLINE(sp));
		if (vi_send("1", &ipb))
			return (1);
	}

	/*
	 * The bottom line is expected to be blank after this operation,
	 * and other screens must support that semantic.
	 */
	ipb.code = SI_DELETELN;
	return (vi_send(NULL, &ipb));
}

/*
 * ip_discard --
 *	Discard a screen.
 *
 * PUBLIC: int ip_discard __P((SCR *, SCR *));
 */
int
ip_discard(discardp, acquirep)
	SCR *discardp, *acquirep;
{
	return (0);
}

/* 
 * ip_ex_adjust --
 *	Adjust the screen for ex.
 *
 * PUBLIC: int ip_ex_adjust __P((SCR *, exadj_t));
 */
int
ip_ex_adjust(sp, action)
	SCR *sp;
	exadj_t action;
{
	abort();
	/* NOTREACHED */
}

/*
 * ip_insertln --
 *	Push down the current line, discarding the bottom line.
 *
 * PUBLIC: int ip_insertln __P((SCR *));
 */
int
ip_insertln(sp)
	SCR *sp;
{
	IP_BUF ipb;

	ipb.code = SI_INSERTLN;

	return (vi_send(NULL, &ipb));
}

/*
 * ip_keyval --
 *	Return the value for a special key.
 *
 * PUBLIC: int ip_keyval __P((SCR *, scr_keyval_t, CHAR_T *, int *));
 */
int
ip_keyval(sp, val, chp, dnep)
	SCR *sp;
	scr_keyval_t val;
	CHAR_T *chp;
	int *dnep;
{
	/*
	 * VEOF, VERASE and VKILL are required by POSIX 1003.1-1990,
	 * VWERASE is a 4BSD extension.
	 */
	switch (val) {
	case KEY_VEOF:
		*dnep = '\004';		/* ^D */
		break;
	case KEY_VERASE:
		*dnep = '\b';		/* ^H */
		break;
	case KEY_VKILL:
		*dnep = '\025';		/* ^U */
		break;
#ifdef VWERASE
	case KEY_VWERASE:
		*dnep = '\027';		/* ^W */
		break;
#endif
	default:
		*dnep = 1;
		break;
	}
	return (0);
}

/*
 * ip_move --
 *	Move the cursor.
 *
 * PUBLIC: int ip_move __P((SCR *, size_t, size_t));
 */
int
ip_move(sp, lno, cno)
	SCR *sp;
	size_t lno, cno;
{
	IP_PRIVATE *ipp;
	IP_BUF ipb;

	ipp = IPP(sp);
	ipp->row = lno;
	ipp->col = cno;

	ipb.code = SI_MOVE;
	ipb.val1 = RLNO(sp, lno);
	ipb.val2 = cno;
	return (vi_send("12", &ipb));
}

/*
 * ip_refresh --
 *	Refresh the screen.
 *
 * PUBLIC: int ip_refresh __P((SCR *, int));
 */
int
ip_refresh(sp, repaint)
	SCR *sp;
	int repaint;
{
	IP_BUF ipb;
	IP_PRIVATE *ipp;
	recno_t total;

	ipp = IPP(sp);

	/*
	 * If the scroll bar information has changed since we last sent
	 * it, resend it.  Currently, we send three values:
	 *
	 * top		The line number of the first line in the screen.
	 * num		The number of lines visible on the screen.
	 * total	The number of lines in the file.
	 *
	 * XXX
	 * This is a gross violation of layering... we're looking at data
	 * structures at which we have absolutely no business whatsoever
	 * looking...
	 */
	ipb.val1 = HMAP->lno;
	ipb.val2 = TMAP->lno - HMAP->lno;
	(void)db_last(sp, &total);
	ipb.val3 = total == 0 ? 1 : total;
	if (ipb.val1 != ipp->sb_top ||
	    ipb.val2 != ipp->sb_num || ipb.val3 != ipp->sb_total) {
		ipb.code = SI_SCROLLBAR;
		(void)vi_send("123", &ipb);
		ipp->sb_top = ipb.val1;
		ipp->sb_num = ipb.val2;
		ipp->sb_total = ipb.val3;
	}

	/* Refresh/repaint the screen. */
	ipb.code = repaint ? SI_REDRAW : SI_REFRESH;
	return (vi_send(NULL, &ipb));
}

/*
 * ip_rename --
 *	Rename the file.
 *
 * PUBLIC: int ip_rename __P((SCR *, char *, int));
 */
int
ip_rename(sp, name, on)
	SCR *sp;
	char *name;
	int on;
{
	IP_BUF ipb;

	ipb.code = SI_RENAME;
	ipb.str1 = name;
	ipb.len1 = strlen(name);
	return (vi_send("a", &ipb));
}

/*
 * ip_split --
 *	Split a screen.
 *
 * PUBLIC: int ip_split __P((SCR *, SCR *));
 */
int
ip_split(origp, newp)
	SCR *origp, *newp;
{
	return (0);
}

/*
 * ip_suspend --
 *	Suspend a screen.
 *
 * PUBLIC: int ip_suspend __P((SCR *, int *));
 */
int
ip_suspend(sp, allowedp)
	SCR *sp;
	int *allowedp;
{
	*allowedp = 0;
	return (0);
}

/*      
 * ip_usage --
 *      Print out the ip usage messages.
 *
 * PUBLIC: void ip_usage __P((void));
 */
void    
ip_usage()
{       
#define USAGE "\
usage: vi [-eFlRrSv] [-c command] [-I ifd.ofd] [-t tag] [-w size] [file ...]\n"
        (void)fprintf(stderr, "%s", USAGE);
#undef  USAGE
}
