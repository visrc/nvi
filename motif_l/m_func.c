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
static const char sccsid[] = "$Id: m_func.c,v 8.21 1996/12/18 10:25:27 bostic Exp $ (Berkeley) $Date: 1996/12/18 10:25:27 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <Xm/PanedW.h>
#include <Xm/ScrollBar.h>

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/common.h"
#include "../ipc/ip.h"
#include "m_motif.h"


static int
vi_addstr(ipbp)
	IP_BUF *ipbp;
{
#ifdef TRACE
	vtrace("addstr() {%.*s}\n", ipbp->len1, ipbp->str1);
#endif
	/* Add to backing store. */
	memcpy(CharAt(__vi_screen, __vi_screen->cury, __vi_screen->curx),
	    ipbp->str1, ipbp->len1);
	memset(FlagAt(__vi_screen, __vi_screen->cury, __vi_screen->curx),
	    __vi_screen->color, ipbp->len1);

	/* Draw from backing store. */
	__vi_draw_text(__vi_screen,
	    __vi_screen->cury, __vi_screen->curx, ipbp->len1);

	/* Advance the caret. */
	__vi_move_caret(__vi_screen,
	    __vi_screen->cury, __vi_screen->curx + ipbp->len1);
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
	/* probably ok to scroll again */
	__vi_clear_scroll_block();

	/* if the tag stack widget is active, set the text field there
	 * to agree with the current caret position.
	 * Note that this really ought to be done by core due to wrapping issues
	 */
	__vi_set_word_at_caret( __vi_screen );

	/* similarly, the text ruler... */
	__vi_set_text_ruler( __vi_screen->cury, __vi_screen->curx );

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
	if ((tail = strrchr(ipbp->str1, '/')) == NULL || *(tail + 1) == '\0')
		tail = ipbp->str1;
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
		      XmNtitle,		ipbp->str1,
		      0
		      );
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
	int top, size, maximum, old_max;

	/* in the buffer,
	 *	val1 contains the top visible line number
	 *	val2 contains the number of visible lines
	 *	val3 contains the number of lines in the file
	 */
	top	= ipbp->val1;
	size	= ipbp->val2;
	maximum	= ipbp->val3;

#if 0
	fprintf( stderr, "Setting scrollbar\n" );
	fprintf( stderr, "\tvalue\t\t%d\n",	top );
	fprintf( stderr, "\tsize\t\t%d\n",	size );
	fprintf( stderr, "\tmaximum\t\t%d\n",	maximum );
#endif

	/* armor plating.  core thinks there are no lines in an
	 * empty file, but says we are on line 1
	 */
	if ( top >= maximum ) {
#if 0
	    fprintf( stderr, "Correcting for top >= maximum\n" );
#endif
	    maximum	= top + 1;
	    size	= 1;
	}

	/* armor plating.  core may think there are more
	 * lines visible than remain in the file
	 */
	if ( top+size >= maximum ) {
#if 0
	    fprintf( stderr, "Correcting for top+size >= maximum\n" );
#endif
	    size	= maximum - top;
	}

	/* need to increase the maximum before changing the values */
	XtVaGetValues( __vi_screen->scroll, XmNmaximum, &old_max, 0 );
	if ( maximum > old_max )
	    XtVaSetValues( __vi_screen->scroll, XmNmaximum, maximum, 0 );

	/* change the rest of the values without generating a callback */
	XmScrollBarSetValues( __vi_screen->scroll,
			      top,
			      size,
			      1,	/* increment */
			      size,	/* page_increment */
			      False	/* do not notify me */
			      );

	/* need to decrease the maximum after changing the values */
	if ( maximum < old_max )
	    XtVaSetValues( __vi_screen->scroll, XmNmaximum, maximum, 0 );

	/* done */
	return (0);
}

static int
vi_select(ipbp)
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
	__vi_editopt,
	vi_insertln,
	vi_move,
	vi_quit,
	vi_redraw,
	vi_refresh,
	vi_rename,
	vi_rewrite,
	vi_scrollbar,
	vi_select,
	vi_split
};
