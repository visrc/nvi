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
static const char sccsid[] = "$Id: m_func.c,v 8.14 1996/12/11 20:58:23 bostic Exp $ (Berkeley) $Date: 1996/12/11 20:58:23 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include "Xm/PanedW.h"

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/common.h"
#include "../ip/ip.h"
#include "m_motif.h"
#include "m_extern.h"
#include "ipc_extern.h"

static int
vi_addstr(ipbp)
	IP_BUF *ipbp;
{
#ifdef TRACE
	trace("addstr() {%.*s}\n", ipbp->len, ipbp->str);
#endif
	/* Add to backing store. */
	memcpy(CharAt(__vi_screen, __vi_screen->cury, __vi_screen->curx),
	    ipbp->str, ipbp->len);
	memset(FlagAt(__vi_screen, __vi_screen->cury, __vi_screen->curx),
	    __vi_screen->color, ipbp->len);

	/* Draw from backing store. */
	__vi_draw_text(__vi_screen,
	    __vi_screen->cury, __vi_screen->curx, ipbp->len);

	/* Advance the caret. */
	__vi_move_caret(__vi_screen,
	    __vi_screen->cury, __vi_screen->curx + ipbp->len);
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
		__vi_screen->color = ipbp->val2;
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
	XBell(XtDisplay(__vi_screen->area), 0);
	return (0);
}

static int
vi_busyon(ipbp)
	IP_BUF *ipbp;
{
	__vi_set_cursor(__vi_screen, 1);
	return (0);
}

static int
vi_busyoff(ipbp)
	IP_BUF *ipbp;
{
	__vi_set_cursor(__vi_screen, 0);
	return (0);
}

static int
vi_clrtoeol(ipbp)
	IP_BUF *ipbp;
{
	int len;
	char *ptr;

	len = __vi_screen->cols - __vi_screen->curx;
	ptr = CharAt(__vi_screen, __vi_screen->cury, __vi_screen->curx);
	
	/* Clear backing store. */
	memset(ptr, ' ', len);
	memset(FlagAt(__vi_screen, __vi_screen->cury, __vi_screen->curx),
	    COLOR_STANDARD, len);

	/* Draw from backing store. */
	__vi_draw_text(__vi_screen, __vi_screen->cury, __vi_screen->curx, len);

	return (0);
}

static int
vi_deleteln(ipbp)
	IP_BUF *ipbp;
{
	int y, rows, len, height, width;

	y = __vi_screen->cury;
	rows = __vi_screen->rows - (y+1);
	len = __vi_screen->cols * rows;

	/* Don't want to copy the caret! */
	__vi_erase_caret(__vi_screen);

	/* Adjust backing store and the flags. */
	memmove(CharAt(__vi_screen, y, 0), CharAt(__vi_screen, y+1, 0), len);
	memmove(FlagAt(__vi_screen, y, 0), FlagAt(__vi_screen, y+1, 0), len);

	/* Move the bits on the screen. */
	width = __vi_screen->ch_width * __vi_screen->cols;
	height = __vi_screen->ch_height * rows;
	XCopyArea(XtDisplay(__vi_screen->area),		/* display */
		  XtWindow(__vi_screen->area),		/* src */
		  XtWindow(__vi_screen->area),		/* dest */
		  __vi_copy_gc,				/* context */
		  0, YTOP(__vi_screen, y+1),		/* srcx, srcy */
		  width, height,
		  0, YTOP(__vi_screen, y)		/* dstx, dsty */
		  );
	/* Need to let X take over. */
	XmUpdateDisplay(__vi_screen->area);

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

	y = __vi_screen->cury;
	rows = __vi_screen->rows - (1+y);
	from = CharAt(__vi_screen, y, 0),
	to = CharAt(__vi_screen, y+1, 0);

	/* Don't want to copy the caret! */
	__vi_erase_caret(__vi_screen);

	/* Adjust backing store. */
	memmove(to, from, __vi_screen->cols * rows);
	memset(from, ' ', __vi_screen->cols);

	/* And the backing store. */
	from = FlagAt(__vi_screen, y, 0),
	to = FlagAt(__vi_screen, y+1, 0);
	memmove(to, from, __vi_screen->cols * rows);
	memset(from, COLOR_STANDARD, __vi_screen->cols);

	/* Move the bits on the screen. */
	width = __vi_screen->ch_width * __vi_screen->cols;
	height = __vi_screen->ch_height * rows;

	XCopyArea(XtDisplay(__vi_screen->area),		/* display */
		  XtWindow(__vi_screen->area),		/* src */
		  XtWindow(__vi_screen->area),		/* dest */
		  __vi_copy_gc,				/* context */
		  0, YTOP(__vi_screen, y),		/* srcx, srcy */
		  width, height,
		  0, YTOP(__vi_screen, y+1)		/* dstx, dsty */
		  );

	/* clear out the new space */
	XClearArea(XtDisplay(__vi_screen->area),	/* display */
		   XtWindow(__vi_screen->area),		/* window */
		   0, YTOP(__vi_screen, y),		/* srcx, srcy */
		   0, __vi_screen->ch_height,		/* w=full, height */
		   True					/* no exposures */
		   );

	/* Need to let X take over. */
	XmUpdateDisplay(__vi_screen->area);

	return (1);
}

static int
vi_move(ipbp)
	IP_BUF *ipbp;
{
	__vi_move_caret(__vi_screen, ipbp->val1, ipbp->val2);
	return (0);
}

static int
vi_redraw(ipbp)
	IP_BUF *ipbp;
{
	__vi_expose_func(0, __vi_screen, 0);
	return (0);
}

static int
vi_refresh(ipbp)
	IP_BUF *ipbp;
{
#if 0
	/* Force synchronous update of the widget. */
	XmUpdateDisplay(__vi_screen->area);
#endif
	return (0);
}

static int
vi_quit(ipbp)
	IP_BUF *ipbp;
{
	if (__vi_exitp != NULL)
		__vi_exitp();

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
	shell = __vi_screen->area;
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

int (*__vi_iplist[SI_EVENT_MAX]) __P((IP_BUF *)) = {
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
	vi_quit,
	vi_redraw,
	vi_refresh,
	vi_rename,
	vi_rewrite,
	vi_scrollbar,
	vi_split
};
