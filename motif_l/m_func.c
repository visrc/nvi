/*-
 * Copyright (c) 1996
 *	Rob Zimmermann.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#ifndef lint
static const char XXsccsid[] = "$Id: m_func.c,v 8.1 1996/11/27 09:26:05 bostic Exp $ (Berkeley) $Date: 1996/11/27 09:26:05 $";
#endif /* not lint */

int
ipo_addstr(ipbp)
	IP_BUF *ipbp;
{
	/* Add to backing store. */
	memcpy(CharAt(cur_screen, cur_screen->cury, cur_screen->curx),
	    ipbp->str, ipbp->len);
	memset(FlagAt(cur_screen, cur_screen->cury, cur_screen->curx),
	    cur_screen->color, ipbp->len);

	/* Draw from backing store. */
	draw_text(cur_screen, cur_screen->cury, cur_screen->curx, ipbp->len);

	/* Advance the caret. */
	move_caret(cur_screen, cur_screen->cury, cur_screen->curx + ipbp->len);
	return (0);
}

int
ipo_attribute(ipbp)
	IP_BUF *ipbp;
{
	switch (ipbp->val1) {
	case SA_ALTERNATE:
		/* XXX: Nothing. */
		break;
	case SA_INVERSE:
		cur_screen->color = ipbp->val2;
		break;
	}
	return (0);
}

int
ipo_bell(ipbp)
	IP_BUF *ipbp;
{
	/*
	 * XXX
	 * Future... implement visible bell.
	 */
	XBell(XtDisplay(cur_screen->area), 0);
	return (0);
}

int
ipo_busy(ipbp)
	IP_BUF *ipbp;
{
#if 0
	/*
	 * I'm just guessing here, but I believe we are supposed to set
	 * the busy cursor when the text is non-null, and restore the
	 * default cursor otherwise.
	 */
	set_cursor(cur_screen, ipbp->len != 0);
#endif
	return (0);
}

int
ipo_clrtoeol(ipbp)
	IP_BUF *ipbp;
{
	int len;
	char *ptr;

	len = cur_screen->cols - cur_screen->curx;
	ptr = CharAt(cur_screen, cur_screen->cury, cur_screen->curx);
	
	/* Clear backing store. */
	memset(ptr, ' ', len);
	memset(FlagAt(cur_screen, cur_screen->cury, cur_screen->curx),
	    COLOR_STANDARD, len);

	/* Draw from backing store. */
	draw_text(cur_screen, cur_screen->cury, cur_screen->curx, len);

	return (0);
}

int
ipo_deleteln(ipbp)
	IP_BUF *ipbp;
{
	int y, rows, len, height, width;

	y = cur_screen->cury;
	rows = cur_screen->rows - y;
	len = cur_screen->cols * rows;

	/* Don't want to copy the caret! */
	erase_caret(cur_screen);

	/* Adjust backing store and the flags. */
	memmove(CharAt(cur_screen, y, 0), CharAt(cur_screen, y+1, 0), len);
	memmove(FlagAt(cur_screen, y, 0), FlagAt(cur_screen, y+1, 0), len);

	/* Move the bits on the screen. */
	width = cur_screen->ch_width * cur_screen->cols;
	height = cur_screen->ch_height * rows;
	XCopyArea(XtDisplay(cur_screen->area),		/* display */
		  XtWindow(cur_screen->area),		/* src */
		  XtWindow(cur_screen->area),		/* dest */
		  copy_gc,				/* context */
		  0, YTOP(cur_screen, y+1),		/* srcx, srcy */
		  width, height,
		  0, YTOP(cur_screen, y)		/* dstx, dsty */
		  );
	/* Need to let X take over. */
	XmUpdateDisplay(cur_screen->area);

	return (0);
}

int
ipo_discard(ipbp)
	IP_BUF *ipbp;
{
	/* XXX: Nothing. */
	return (0);
}

int
ipo_insertln(ipbp)
	IP_BUF *ipbp;
{
	int y, rows, height, width;
	char *from, *to;

	y = cur_screen->cury;
	rows = cur_screen->rows - (1+y);
	from = CharAt(cur_screen, y, 0),
	to = CharAt(cur_screen, y+1, 0);

	/* Don't want to copy the caret! */
	erase_caret(cur_screen);

	/* Adjust backing store. */
	memmove(to, from, cur_screen->cols * rows);
	memset(from, ' ', cur_screen->cols);

	/* And the backing store. */
	from = FlagAt(cur_screen, y, 0),
	to = FlagAt(cur_screen, y+1, 0);
	memmove(to, from, cur_screen->cols * rows);
	memset(from, COLOR_STANDARD, cur_screen->cols);

	/* Move the bits on the screen. */
	width = cur_screen->ch_width * cur_screen->cols;
	height = cur_screen->ch_height * rows;

	XCopyArea(XtDisplay(cur_screen->area),		/* display */
		  XtWindow(cur_screen->area),		/* src */
		  XtWindow(cur_screen->area),		/* dest */
		  copy_gc,				/* context */
		  0, YTOP(cur_screen, y),		/* srcx, srcy */
		  width, height,
		  0, YTOP(cur_screen, y+1)		/* dstx, dsty */
		  );

	/* Need to let X take over. */
	XmUpdateDisplay(cur_screen->area);

	return (0);
}

int
ipo_move(ipbp)
	IP_BUF *ipbp;
{
	move_caret(cur_screen, ipbp->val1, ipbp->val2);
	return (0);
}

int
ipo_redraw(ipbp)
	IP_BUF *ipbp;
{
	expose_func(0, cur_screen, 0);
	return (0);
}

int
ipo_refresh(ipbp)
	IP_BUF *ipbp;
{
#if 0
	/* Force synchronous update of the widget. */
	XmUpdateDisplay(cur_screen->area);
#endif
	return (0);
}

int
ipo_rename(ipbp)
	IP_BUF *ipbp;
{
	/*
	 * XXX
	 * Future:  Attach a title to each screen.  For now, we change
	 * the title of the shell.
	 */
	XtVaSetValues(top_level, XmNtitle, ipbp->str, 0);

	return (0);
}

int
ipo_rewrite(ipbp)
	IP_BUF *ipbp;
{
	/* XXX: Nothing. */
	return (0);
}

int
ipo_split(ipbp)
	IP_BUF *ipbp;
{
	/* XXX: Nothing. */
	return (0);
}

int (*iplist[IPO_EVENT_MAX]) __P((IP_BUF *)) = {
	ipo_addstr,
	ipo_attribute,
	ipo_bell,
	ipo_busy,
	ipo_clrtoeol,
	ipo_deleteln,
	ipo_discard,
	ipo_insertln,
	ipo_move,
	ipo_redraw,
	ipo_refresh,
	ipo_rename,
	ipo_rewrite,
	ipo_split
};
