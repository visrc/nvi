/*-
 * Copyright (c) 1996
 *	Rob Zimmermann.  All rights reserved.
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: m_func.c,v 8.10 1996/12/10 17:44:31 bostic Exp $ (Berkeley) $Date: 1996/12/10 17:44:31 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include "Xm/PanedW.h"

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/common.h"
#include "../ip_vi/ip.h"
#include "ipc_motif.h"
#include "ipc_mextern.h"
#include "ipc_extern.h"

static int
vi_addstr(ipbp)
	IP_BUF *ipbp;
{
#ifdef TR
	trace("addstr() {%.*s}\n", ipbp->len, ipbp->str);
#endif
	/* Add to backing store. */
	memcpy(CharAt(_vi_screen, _vi_screen->cury, _vi_screen->curx),
	    ipbp->str, ipbp->len);
	memset(FlagAt(_vi_screen, _vi_screen->cury, _vi_screen->curx),
	    _vi_screen->color, ipbp->len);

	/* Draw from backing store. */
	_vi_draw_text(_vi_screen,
	    _vi_screen->cury, _vi_screen->curx, ipbp->len);

	/* Advance the caret. */
	_vi_move_caret(_vi_screen,
	    _vi_screen->cury, _vi_screen->curx + ipbp->len);
	return (0);
}

static int
vi_attribute(ipbp)
	IP_BUF *ipbp;
{
	switch (ipbp->val1) {
	case SA_ALTERNATE:
		/* XXX: Nothing. */
		break;
	case SA_INVERSE:
		_vi_screen->color = ipbp->val2;
		break;
	}
	return (0);
}

static int
vi_bell(ipbp)
	IP_BUF *ipbp;
{
	/*
	 * XXX
	 * Future... implement visible bell.
	 */
	XBell(XtDisplay(_vi_screen->area), 0);
	return (0);
}

static int
vi_busyon(ipbp)
	IP_BUF *ipbp;
{
	_vi_set_cursor(_vi_screen, 1);
	return (0);
}

static int
vi_busyoff(ipbp)
	IP_BUF *ipbp;
{
	_vi_set_cursor(_vi_screen, 0);
	return (0);
}

static int
vi_clrtoeol(ipbp)
	IP_BUF *ipbp;
{
	int len;
	char *ptr;

	len = _vi_screen->cols - _vi_screen->curx;
	ptr = CharAt(_vi_screen, _vi_screen->cury, _vi_screen->curx);
	
	/* Clear backing store. */
	memset(ptr, ' ', len);
	memset(FlagAt(_vi_screen, _vi_screen->cury, _vi_screen->curx),
	    COLOR_STANDARD, len);

	/* Draw from backing store. */
	_vi_draw_text(_vi_screen, _vi_screen->cury, _vi_screen->curx, len);

	return (0);
}

static int
vi_deleteln(ipbp)
	IP_BUF *ipbp;
{
	int y, rows, len, height, width;

	y = _vi_screen->cury;
	rows = _vi_screen->rows - (y+1);
	len = _vi_screen->cols * rows;

	/* Don't want to copy the caret! */
	_vi_erase_caret(_vi_screen);

	/* Adjust backing store and the flags. */
	memmove(CharAt(_vi_screen, y, 0), CharAt(_vi_screen, y+1, 0), len);
	memmove(FlagAt(_vi_screen, y, 0), FlagAt(_vi_screen, y+1, 0), len);

	/* Move the bits on the screen. */
	width = _vi_screen->ch_width * _vi_screen->cols;
	height = _vi_screen->ch_height * rows;
	XCopyArea(XtDisplay(_vi_screen->area),		/* display */
		  XtWindow(_vi_screen->area),		/* src */
		  XtWindow(_vi_screen->area),		/* dest */
		  _vi_copy_gc,				/* context */
		  0, YTOP(_vi_screen, y+1),		/* srcx, srcy */
		  width, height,
		  0, YTOP(_vi_screen, y)		/* dstx, dsty */
		  );
	/* Need to let X take over. */
	XmUpdateDisplay(_vi_screen->area);

	return (1);
}

static int
vi_discard(ipbp)
	IP_BUF *ipbp;
{
	/* XXX: Nothing. */
	return (0);
}

static int
vi_insertln(ipbp)
	IP_BUF *ipbp;
{
	int y, rows, height, width;
	char *from, *to;

	y = _vi_screen->cury;
	rows = _vi_screen->rows - (1+y);
	from = CharAt(_vi_screen, y, 0),
	to = CharAt(_vi_screen, y+1, 0);

	/* Don't want to copy the caret! */
	_vi_erase_caret(_vi_screen);

	/* Adjust backing store. */
	memmove(to, from, _vi_screen->cols * rows);
	memset(from, ' ', _vi_screen->cols);

	/* And the backing store. */
	from = FlagAt(_vi_screen, y, 0),
	to = FlagAt(_vi_screen, y+1, 0);
	memmove(to, from, _vi_screen->cols * rows);
	memset(from, COLOR_STANDARD, _vi_screen->cols);

	/* Move the bits on the screen. */
	width = _vi_screen->ch_width * _vi_screen->cols;
	height = _vi_screen->ch_height * rows;

	XCopyArea(XtDisplay(_vi_screen->area),		/* display */
		  XtWindow(_vi_screen->area),		/* src */
		  XtWindow(_vi_screen->area),		/* dest */
		  _vi_copy_gc,				/* context */
		  0, YTOP(_vi_screen, y),		/* srcx, srcy */
		  width, height,
		  0, YTOP(_vi_screen, y+1)		/* dstx, dsty */
		  );

	/* clear out the new space */
	XClearArea(XtDisplay(_vi_screen->area),		/* display */
		   XtWindow(_vi_screen->area),		/* window */
		   0, YTOP(_vi_screen, y),		/* srcx, srcy */
		   0, _vi_screen->ch_height,		/* w=full, height */
		   True					/* no exposures */
		   );

	/* Need to let X take over. */
	XmUpdateDisplay(_vi_screen->area);

	return (1);
}

static int
vi_move(ipbp)
	IP_BUF *ipbp;
{
	_vi_move_caret(_vi_screen, ipbp->val1, ipbp->val2);
	return (0);
}

static int
vi_redraw(ipbp)
	IP_BUF *ipbp;
{
	_vi_expose_func(0, _vi_screen, 0);
	return (0);
}

static int
vi_refresh(ipbp)
	IP_BUF *ipbp;
{
#if 0
	/* Force synchronous update of the widget. */
	XmUpdateDisplay(_vi_screen->area);
#endif
	return (0);
}

static int
vi_rename(ipbp)
	IP_BUF *ipbp;
{
	const char *tail;
	Widget	shell;

	/* For the icon, use the tail. */
	if ((tail = strrchr(ipbp->str, '/')) == NULL || *(tail + 1) == '\0')
		tail = ipbp->str;
	else
		tail++;

	/*
	 * XXX
	 * Future:  Attach a title to each screen.  For now, we change
	 * the title of the shell.
	 */
	shell = _vi_screen->area;
	while ( ! XtIsShell(shell) ) shell = XtParent(shell);
	XtVaSetValues(shell,
		      XmNiconName,	tail,
		      XmNtitle,		ipbp->str,
		      0);
	return (0);
}

static int
vi_rewrite(ipbp)
	IP_BUF *ipbp;
{
	/* XXX: Nothing. */
	return (0);
}

static int
vi_scrollbar(ipbp)
	IP_BUF *ipbp;
{
	/* XXX: Nothing. */
	return (0);
}

static int
vi_split(ipbp)
	IP_BUF *ipbp;
{
	/* XXX: Nothing. */
	return (0);
}

int (*_vi_iplist[IPO_EVENT_MAX]) __P((IP_BUF *)) = {
	vi_addstr,
	vi_attribute,
	vi_bell,
	vi_busyoff,
	vi_busyon,
	vi_clrtoeol,
	vi_deleteln,
	vi_discard,
	vi_insertln,
	vi_move,
	vi_redraw,
	vi_refresh,
	vi_rename,
	vi_rewrite,
	vi_scrollbar,
	vi_split
};
