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
static const char sccsid[] = "$Id: m_vi.c,v 8.21 1996/12/10 17:07:23 bostic Exp $ (Berkeley) $Date: 1996/12/10 17:07:23 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include <Xm/PanedW.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/ScrollBar.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "../ip_vi/ip.h"
#include "ipc_motif.h"
#include "ipc_mutil.h"
#include "ipc_extern.h"
#include "ipc_mextern.h"
#include "pathnames.h"

static	void	f_copy();
static	void	f_paste();
static	void	f_clear();
	void	 _vi_set_cursor();


/*
 * Globals and costants
 */

#define	BufferSize	1024

static	XFontStruct	*font;
static	GC		gc;
	GC		_vi_copy_gc;
static	XtAppContext	ctx;

	xvi_screen	*_vi_screen = NULL;
static	Cursor		std_cursor;
static	Cursor		busy_cursor;
static	XtTranslations	area_trans;
static	int		multi_click_length;

static	char	bp[ BufferSize ];		/* input buffer from pipe */
static	size_t	len, blen = sizeof(bp);


#if defined(__STDC__)
static	void	set_gc_colors( xvi_screen *this_screen, int val )
#else
static	void	set_gc_colors( this_screen, val )
xvi_screen	*this_screen;
int		val;
#endif
{
    static Pixel	fg, bg, hi, shade;
    static int		prev = COLOR_INVALID;

    /* no change? */
    if ( prev == val ) return;

    /* init? */
    if ( gc == NULL ) {

	/* what colors are selected for the drawing area? */
	XtVaGetValues( this_screen->area,
		       XtNbackground,		&bg,
		       XtNforeground,		&fg,
		       XmNhighlightColor,	&hi,
		       XmNtopShadowColor,	&shade,
		       0
		       );

	gc = XCreateGC( XtDisplay(this_screen->area),
		        DefaultRootWindow(XtDisplay(this_screen->area)),
			0,
			0
			);

	XSetFont( XtDisplay(this_screen->area), gc, font->fid );
    }

    /* special colors? */
    if ( val & COLOR_CARET ) {
	XSetForeground( XtDisplay(this_screen->area), gc, fg );
	XSetBackground( XtDisplay(this_screen->area), gc, hi );
    }
    else if ( val & COLOR_SELECT ) {
	XSetForeground( XtDisplay(this_screen->area), gc, fg );
	XSetBackground( XtDisplay(this_screen->area), gc, shade );
    }
    else switch (val) {
	case COLOR_STANDARD:
	    XSetForeground( XtDisplay(this_screen->area), gc, fg );
	    XSetBackground( XtDisplay(this_screen->area), gc, bg );
	    break;
	case COLOR_INVERSE:
	    XSetForeground( XtDisplay(this_screen->area), gc, bg );
	    XSetBackground( XtDisplay(this_screen->area), gc, fg );
	    break;
	default:	/* implement color map later */
	    break;
    }
}


/*
 * Memory utilities
 */

#ifdef REALLOC
#undef REALLOC
#endif

#define REALLOC( ptr, size )	\
	((ptr == NULL) ? malloc(size) : realloc(ptr,size))


/* X windows routines.
 * We currently create a single, top-level shell.  In that is a
 * single drawing area into which we will draw text.  This allows
 * us to put multi-color (and font, but we'll never build that) text
 * into the drawing area.  In the future, we'll add scrollbars to the
 * drawing areas
 */

void	select_start();
void	select_extend();
void	select_paste();
void	key_press();
void	insert_string();
void	beep();
void	find();
void	command();

static XtActionsRec	area_actions[] = {
    { "select_start",	select_start	},
    { "select_extend",	select_extend	},
    { "select_paste",	select_paste	},
    { "key_press",	key_press	},
    { "insert_string",	insert_string	},
    { "beep",		beep		},
    { "find",		find		},
    { "command",	command		},
};

char	areaTrans[] =
    "<Btn1Down>:	select_start()		\n\
     <Btn1Motion>:	select_extend()		\n\
     <Btn2Down>:	select_paste()		\n\
     <Btn3Down>:	select_extend()		\n\
     <Btn3Motion>:	select_extend()		\n\
     <Key>End:		command(IPO_C_BOTTOM)	\n\
     <Key>Escape:	command(EINSERT)	\n\
     <Key>Find:		find()			\n\
     <Key>Home:		command(IPO_C_TOP)	\n\
     <Key>Next:		command(IPO_C_PGDOWN)	\n\
     <Key>Prior:	command(IPO_C_PGUP)	\n\
     <Key>osfBackSpace:	command(IPO_C_LEFT)	\n\
     <Key>osfBeginLine:	command(IPO_C_BOL)	\n\
     <Key>osfCopy:	beep()			\n\
     <Key>osfCut:	beep()			\n\
     <Key>osfDelete:	command(IPO_C_DEL)	\n\
     <Key>osfDown:	command(IPO_C_DOWN)	\n\
     <Key>osfEndLine:	command(IPO_C_EOL)	\n\
     <Key>osfInsert:	command(IPO_C_INSERT)	\n\
     <Key>osfLeft:	command(IPO_C_LEFT)	\n\
     <Key>osfPageDown:	command(IPO_C_PGDOWN)	\n\
     <Key>osfPageUp:	command(IPO_C_PGUP)	\n\
     <Key>osfPaste:	insert_string(p)	\n\
     <Key>osfRight:	command(IPO_C_RIGHT)	\n\
     <Key>osfUndo:	command(IPO_UNDO)	\n\
     <Key>osfUp:	command(IPO_C_UP)	\n\
     Ctrl<Key>C:	command(IPO_INTERRUPT)	\n\
     <Key>:		key_press()";


static  XutResource resource[] = {
    { "font",		XutRKfont,	&font		},
    { "pointerShape",	XutRKcursor,	&std_cursor	},
    { "busyShape",	XutRKcursor,	&busy_cursor	},
};



#if defined(__STDC__)
static	Boolean		process_pipe_input( XtPointer pread )
#else
static	Boolean		process_pipe_input( pread )
	XtPointer	pread;
#endif
{
    /* might have read more since the last call */
    len += (pread) ? *((int *)pread) : 0;

    /* Parse to data end or partial message. */
    (void)ip_trans(bp, &len);

    if (len > 0) {
#ifdef TR
	    trace("pipe_input_func: abort with %d in the buffer\n", len);
#endif
	    /* call me again later */
	    return False;
    }

    /* do NOT call me again later */
    return True;
}


/* We've received input on the pipe from vi... */
#if defined(__STDC__)
Boolean		vi_pipe_input_func( XtPointer client_data,
				    int *source,
				    XtInputId id
				    )
#else
Boolean		vi_pipe_input_func( client_data, source, id )
XtPointer	client_data;
int		*source;
XtInputId	id;
#endif
{
    int	nr;

    /* Read waiting vi messags and translate to X calls. */
    switch (nr = read( *source, bp + len, blen - len)) {
    case 0:
#ifdef TR
	    trace("pipe_input_func:  empty input from vi\n");
#endif
	    return True;
    case -1:
	    perror("ip_cl: read");
	    exit (1);
    default:
#ifdef TR
	    trace("input from vi, %d bytes read\n", nr);
#endif
	    break;
    }

    /* parse and dispatch on commands in the queue */
    if ( ! process_pipe_input( &nr ) ) {
	    /* check the pipe for unused events when not busy */
	    XtAppAddWorkProc( ctx, process_pipe_input, NULL );
    }

    return True;
}



/* Send the window size. */
#if defined(__STDC__)
static	void	send_resize( xvi_screen *this_screen )
#else
static	void	send_resize( this_screen )
xvi_screen	*this_screen;
#endif
{
    IP_BUF	ipb;

    ipb.val1 = this_screen->rows;
    ipb.val2 = this_screen->cols;
    ipb.code = IPO_RESIZE;

#ifdef TR
    trace("resize_func ( %d x %d )\n", this_screen->rows, this_screen->cols);
#endif

    /* send up the pipe */
    ip_send("12", &ipb);
}


#if defined(__STDC__)
static	void	resize_backing_store( xvi_screen *this_screen )
#else
static	void	resize_backing_store( this_screen )
xvi_screen	*this_screen;
#endif
{
    int	total_chars = this_screen->rows * this_screen->cols;

    this_screen->characters	= REALLOC( this_screen->characters,
					   total_chars
					   );
    memset( this_screen->characters, ' ', total_chars );

    this_screen->flags		= REALLOC( this_screen->flags,
					   total_chars
					   );
    memset( this_screen->flags, 0, total_chars );
}



/* X will call this when we are resized */
#if defined(__STDC__)
static	void	resize_func( Widget wid,
			     XtPointer client_data,
			     XtPointer call_data
			     )
#else
static	void	resize_func( wid, client_data, call_data )
Widget		wid;
XtPointer	client_data;
XtPointer	call_data;
#endif
{
    xvi_screen			*this_screen = (xvi_screen *) client_data;
    Dimension			height, width;

    XtVaGetValues( wid, XmNheight, &height, XmNwidth, &width, 0 );

    /* generate correct sizes when we have font metrics implemented */
    this_screen->cols = width / this_screen->ch_width;
    this_screen->rows = height / this_screen->ch_height;

    resize_backing_store( this_screen );
    send_resize( this_screen );
}


/*
 * _vi_draw_text --
 *	Draw from backing store.
 *
 * PUBLIC: void	_vi_draw_text __P((xvi_screen *, int, int, int));
 */
void
_vi_draw_text(this_screen, row, start_col, len)
	xvi_screen *this_screen;
	int row, start_col, len;
{
    int		col, color, xpos;
    char	*start, *end;

    start = CharAt( _vi_screen, row, start_col );
    color = *FlagAt( _vi_screen, row, start_col );
    xpos  = XPOS( _vi_screen, start_col );

    /* one column at a time */
    for ( col=start_col;
	  col<this_screen->cols && col<start_col+len;
	  col++ ) {

	/* has the color changed? */
	if ( *FlagAt( _vi_screen, row, col ) == color )
	    continue;

	/* is there anything to write? */
	end  = CharAt( _vi_screen, row, col );
	if ( end == start )
	    continue;

	/* yes. write in the previous color */
	set_gc_colors( _vi_screen, color );

	/* add to display */
	XDrawImageString( XtDisplay(_vi_screen->area),
			  XtWindow(_vi_screen->area),
			  gc,
			  xpos,
			  YPOS( _vi_screen, row ),
			  start,
			  end - start
			  );

	/* this is the new context */
	color = *FlagAt( _vi_screen, row, col );
	xpos  = XPOS( _vi_screen, col );
	start = end;
    }

    /* is there anything to write? */
    end = CharAt( _vi_screen, row, col );
    if ( end != start ) {
	/* yes. write in the previous color */
	set_gc_colors( _vi_screen, color );

	/* add to display */
	XDrawImageString( XtDisplay(_vi_screen->area),
			  XtWindow(_vi_screen->area),
			  gc,
			  xpos,
			  YPOS( _vi_screen, row ),
			  start,
			  end - start
			  );
    }
}


/* set clipping rectangles accordingly */
#if defined(__STDC__)
static	void	add_to_clip( xvi_screen *cur_screen, int x, int y, int width, int height )
#else
static	void	add_to_clip( cur_screen, x, y, width, height )
	xvi_screen *cur_screen;
	int	x;
	int	y;
	int	width;
	int	height;
#endif
{
    XRectangle	rect;
    rect.x	= x;
    rect.y	= y;
    rect.height	= height;
    rect.width	= width;
    if ( cur_screen->clip == NULL )
	cur_screen->clip = XCreateRegion();
    XUnionRectWithRegion( &rect, cur_screen->clip, cur_screen->clip );
}


/*
 * _vi_expose_func --
 *	Redraw the window's contents.
 *
 * NOTE: When vi wants to force a redraw, we are called with NULL widget
 *	 and call_data.
 *
 * PUBLIC: void	_vi_expose_func __P((Widget, XtPointer, XtPointer));
 */
void
_vi_expose_func(wid, client_data, call_data)
	Widget wid;
	XtPointer client_data, call_data;
{
    xvi_screen			*this_screen;
    XmDrawingAreaCallbackStruct	*cbs;
    XExposeEvent		*xev;
    XGraphicsExposeEvent	*gev;
    int				row;

    /* convert pointers */
    this_screen = (xvi_screen *) client_data;
    cbs		= (XmDrawingAreaCallbackStruct *) call_data;

    /* first exposure? tell vi we are ready... */
    if ( this_screen->init == False ) {

	/* what does the user want to see? */
	_vi_set_cursor( _vi_screen, False );

	/* vi wants a resize as the first event */
	send_resize( _vi_screen );

	/* fine for now.  we'll be back */
	this_screen->init = True;
	return;
    }

    if ( call_data == NULL ) {

	/* vi core calls this when it wants a full refresh */
#ifdef TR
	trace("expose_func:  full refresh\n");
#endif

	XClearWindow( XtDisplay(this_screen->area),
		      XtWindow(this_screen->area)
		      );
    }
    else {
	switch ( cbs->event->type ) {

	    case GraphicsExpose:
		gev = (XGraphicsExposeEvent *) cbs->event;

		/* set clipping rectangles accordingly */
		add_to_clip( this_screen,
			     gev->x, gev->y,
			     gev->width, gev->height
			     );

		/* X calls here when XCopyArea exposes new bits */
#ifdef TR
		trace("expose_func (X):  (x=%d,y=%d,w=%d,h=%d), count=%d\n",
			     gev->x, gev->y,
			     gev->width, gev->height,
			     gev->count);
#endif

		/* more coming?  do it then */
		if ( gev->count > 0 ) return;

		/* set clipping region */
		XSetRegion( XtDisplay(wid), gc, this_screen->clip );
		break;

	    case Expose:
		xev = (XExposeEvent *) cbs->event;

		/* set clipping rectangles accordingly */
		add_to_clip( this_screen,
			     xev->x, xev->y,
			     xev->width, xev->height
			     );

		/* Motif calls here when DrawingArea is exposed */
#ifdef TR
		trace("expose_func (Motif):  (x=%d,y=%d,w=%d,h=%d), count=%d\n",
			     xev->x, xev->y,
			     xev->width, xev->height,
			     xev->count);
#endif

		/* more coming?  do it then */
		if ( xev->count > 0 ) return;

		/* set clipping region */
		XSetRegion( XtDisplay(wid), gc, this_screen->clip );
		break;

	    default:
		/* don't care? */
		return;
	}
    }

    /* one row at a time */
    for (row=0; row<this_screen->rows; row++) {

	/* draw from the backing store */
	_vi_draw_text( this_screen, row, 0, this_screen->cols );
    }

    /* clear clipping region */
    XSetClipMask( XtDisplay(this_screen->area), gc, None );
    if ( this_screen->clip != NULL ) {
	XDestroyRegion( this_screen->clip );
	this_screen->clip = NULL;
    }

}


#if defined(__STDC__)
static void	xexpose	( Widget w,
			  XtPointer client_data,
			  XEvent *ev,
			  Boolean *cont
			  )
#else
static void	xexpose	( w, client_data, ev, cont )
Widget		w;
XtPointer	client_data;
XEvent		*ev;
Boolean		*cont;
#endif
{
    XmDrawingAreaCallbackStruct	cbs;

    switch ( ev->type ) {
	case GraphicsExpose:
	    cbs.event	= ev;
	    cbs.window	= XtWindow(w);
	    cbs.reason	= XmCR_EXPOSE;
	    _vi_expose_func( w, client_data, (XtPointer) &cbs );
	    *cont	= False;	/* we took care of it */
	    break;
	default:
	    /* don't care */
	    break;
    }
}


/* unimplemented keystroke or command */
#if defined(__STDC__)
static void	beep( Widget w )
#else
static void	beep( w )
Widget	w;
#endif
{
    XBell(XtDisplay(w),0);
}


/* give me a search dialog */
#if defined(__STDC__)
static void	find( Widget w )
#else
static void	find( w )
Widget	w;
#endif
{
    _vi_show_search_dialog( w, "Find" );
}

/*
 * command --
 *	Translate simple keyboard input into vi protocol commands.
 */
void
command(widget, event, str, cardinal)
	Widget widget; 
	XKeyEvent *event; 
	String *str;    
	Cardinal *cardinal;
{
	static struct {
		String	name;
		int	code;
		int	count;
	} table[] = {
		{ "IPO_C_BOL",		IPO_C_BOL,	0 },
		{ "IPO_C_BOTTOM",	IPO_C_BOTTOM,	0 },
		{ "IPO_C_DEL",		IPO_C_DEL,	0 },
		{ "IPO_C_DOWN",		IPO_C_DOWN,	1 },
		{ "IPO_C_EOL",		IPO_C_EOL,	0 },
		{ "IPO_C_INSERT",	IPO_C_INSERT,	0 },
		{ "IPO_C_LEFT",		IPO_C_LEFT,	0 },
		{ "IPO_C_PGDOWN",	IPO_C_PGDOWN,	1 },
		{ "IPO_C_PGUP",		IPO_C_PGUP,	1 },
		{ "IPO_C_RIGHT",	IPO_C_RIGHT,	0 },
		{ "IPO_C_TOP",		IPO_C_TOP,	0 },
		{ "IPO_C_UP",		IPO_C_UP,	1 },
		{ "IPO_INTERRUPT",	IPO_INTERRUPT,	0 },
	};
	IP_BUF ipb;
	int i;

	/*
	 * XXX
	 * Do fast lookup based on character #6 -- sleazy, but I don't
	 * want to do 10 strcmp's per keystroke.
	 */
	ipb.val1 = 1;
	for (i = 0; i < XtNumber(table); i++)
		if (table[i].name[6] == (*str)[6] &&
		    strcmp(table[i].name, *str) == 0) {
			ipb.code = table[i].code;
			ip_send(table[i].count ? "1" : NULL, &ipb);
			return;
		}

	/* oops. */
	beep(widget);
}

/* mouse or keyboard input. */
#if defined(__STDC__)
static	void	insert_string( Widget widget, 
			       XKeyEvent *event, 
			       String *str, 
			       Cardinal *cardinal
			       )
#else
static	void	insert_string( widget, event, str, cardinal )
Widget          widget; 
XKeyEvent       *event; 
String          *str;    
Cardinal        *cardinal;
#endif
{
    IP_BUF	ipb;

    ipb.len = strlen( *str );
    if ( ipb.len != 0 ) {
	ipb.code = IPO_STRING;
	ipb.str = *str;
	ip_send("s", &ipb);
    }

#ifdef TR
    trace("insert_string {%.*s}\n", strlen( *str ), *str );
#endif
}


/* mouse or keyboard input. */
#if defined(__STDC__)
static	void	key_press( Widget widget, 
			   XKeyEvent *event, 
			   String str, 
			   Cardinal *cardinal
			   )
#else
static	void	key_press( widget, event, str, cardinal )
Widget          widget; 
XKeyEvent       *event; 
String          str;    
Cardinal        *cardinal;
#endif
{
    IP_BUF	ipb;
    char	bp[BufferSize];

    ipb.len = XLookupString( event, bp, BufferSize, NULL, NULL );
    if ( ipb.len != 0 ) {
	ipb.code = IPO_STRING;
	ipb.str = bp;
#ifdef TR
	trace("key_press {%.*s}\n", ipb.len, bp );
#endif
	ip_send("s", &ipb);
    }

}


#if defined(__STDC__)
static	xvi_screen	*create_screen( Widget parent, int rows, int cols )
#else
static	xvi_screen	*create_screen( parent, rows, cols )
	Widget		parent;
	int		rows, cols;
#endif
{
    xvi_screen	*new_screen = (xvi_screen *) calloc( 1, sizeof(xvi_screen) );
    Widget	frame;

    /* init... */
    new_screen->color		= COLOR_STANDARD;
    new_screen->parent		= parent;

    /* figure out the sizes */
    new_screen->rows		= rows;
    new_screen->cols		= cols;
    new_screen->ch_width	= font->max_bounds.width;
    new_screen->ch_height	= font->descent + font->ascent;
    new_screen->ch_descent	= font->descent;
    new_screen->clip		= NULL;

    /* allocate and init the backing stores */
    resize_backing_store( new_screen );

    /* set up a translation table for the X toolkit */
    if ( area_trans == NULL )   
	area_trans = XtParseTranslationTable(areaTrans);

    /* future, new screen gets inserted into the parent sash
     * immediately after the current screen.  Default Pane action is
     * to add it to the end
     */

    /* use a form to hold the drawing area and the scrollbar */
    new_screen->form = XtVaCreateManagedWidget( "form",
	    xmFormWidgetClass,
	    parent,
	    XmNpaneMinimum,		2*new_screen->ch_height,
	    XmNallowResize,		True,
	    NULL
	    );

    /* create a scroolbar.
     * Future, connect new Protocol messages to position the scrollbar
     * Future, connect scrollbar messages to new Protocol messages 
     */
    new_screen->scroll = XtVaCreateManagedWidget( "scroll",
	    xmScrollBarWidgetClass,
	    new_screen->form,
	    XmNtopAttachment,		XmATTACH_FORM,
	    XmNbottomAttachment,	XmATTACH_FORM,
	    XmNrightAttachment,		XmATTACH_FORM,
	    NULL
	    );

    /* create a frame because they look nice */
    frame = XtVaCreateManagedWidget( "frame",
	    xmFrameWidgetClass,
	    new_screen->form,
	    XmNshadowType,		XmSHADOW_ETCHED_IN,
	    XmNtopAttachment,		XmATTACH_FORM,
	    XmNbottomAttachment,	XmATTACH_FORM,
	    XmNleftAttachment,		XmATTACH_FORM,
	    XmNrightAttachment,		XmATTACH_WIDGET,
	    XmNrightWidget,		new_screen->scroll,
	    NULL
	    );

    /* create a drawing area into which we will put text */
    new_screen->area = XtVaCreateManagedWidget( "screen",
	    xmDrawingAreaWidgetClass,
	    frame,
	    XmNheight,		new_screen->ch_height * new_screen->rows,
	    XmNwidth,		new_screen->ch_width * new_screen->cols,
	    XmNtranslations,	area_trans,
	    XmNuserData,	new_screen,
	    XmNnavigationType,	XmNONE,
	    XmNtraversalOn,	False,
	    NULL
	    );

    /* this callback is for when the drawing area is resized */
    XtAddCallback( new_screen->area,
		   XmNresizeCallback,
		   resize_func,
		   new_screen
		   );

    /* this callback is for when the drawing area is exposed */
    XtAddCallback( new_screen->area,
		   XmNexposeCallback,
		   _vi_expose_func,
		   new_screen
		   );

    /* this callback is for when we expose obscured bits 
     * (e.g. there is a window over part of our drawing area
     */
    XtAddEventHandler( new_screen->area,
		       0,	/* no standard events */
		       True,	/* we *WANT* GraphicsExpose */
		       xexpose,	/* what to do */
		       new_screen
		       );

    return new_screen;
}


static	xvi_screen	*split_screen()
{
    Cardinal	num;
    WidgetList	c;
    int		rows = _vi_screen->rows / 2;
    xvi_screen	*new_screen;

    /* Note that (global) cur_screen needs to be correctly set so that
     * insert_here knows which screen to put the new one after
     */
    new_screen = create_screen( _vi_screen->parent,
				rows,
				_vi_screen->cols
				);

    /* what are the screens? */
    XtVaGetValues( _vi_screen->parent,
		   XmNnumChildren,	&num,
		   XmNchildren,		&c,
		   NULL
		   );

    /* unmanage all children in preparation for resizing */
    XtUnmanageChildren( c, num );

    /* force resize of the affected screens */
    XtVaSetValues( new_screen->form,
		   XmNheight,	new_screen->ch_height * rows,
		   NULL
		   );
    XtVaSetValues( _vi_screen->form,
		   XmNheight,	_vi_screen->ch_height * rows,
		   NULL
		   );

    /* re-manage */
    XtManageChildren( c, num );

    /* done */
    return new_screen;
}


/* Tell me where to insert the next subpane */
#if defined(__STDC__)
static	Cardinal	insert_here( Widget wid )
#else
static	Cardinal	insert_here( wid )
	Widget		wid;
#endif
{
    Cardinal	i, num;
    WidgetList	c;

    XtVaGetValues( XtParent(wid),
		   XmNnumChildren,	&num,
		   XmNchildren,		&c,
		   NULL
		   );

    /* The  default  XmNinsertPosition  procedure  for  PanedWindow
     * causes sashes to be inserted at the end of the list of children
     * and causes non-sash widgets to be inserted after  other
     * non-sash children but before any sashes.
     */
    if ( ! XmIsForm( wid ) )
	return num;

    /* We will put the widget after the one with the current screen */
    for (i=0; i<num && XmIsForm(c[i]); i++) {
	if ( _vi_screen == NULL || _vi_screen->form == c[i] )
	    return i+1;	/* after the i-th */
    }

    /* could not find it?  this should never happen */
    return num;
}


/* create the necessary widgetry */
#if defined(__STDC__)
Widget	vi_create_editor( String name, Widget parent )
#else
Widget	vi_create_editor( name, parent )
String	name;
Widget	parent;
#endif
{
    Widget	pane_w;
    Display	*display = XtDisplay( parent );

    /* first time through? */
    if ( ctx == NULL ) {

	/* save this for later */
	ctx = XtWidgetToApplicationContext( parent );

	/* add our own special actions */
	XtAppAddActions( ctx, area_actions, XtNumber(area_actions) );

	/* how long is double-click? */
	multi_click_length = XtGetMultiClickTime( display );

	/* check the resource database for interesting resources */
	XutConvertResources( parent,
			     vi_progname,
			     resource,
			     XtNumber(resource)
			     );

	/* we need a context for moving bits around in the windows */
	_vi_copy_gc = XCreateGC( display,
				 DefaultRootWindow(display),
				 0,
				 0
				 );

	/* routines for inter client communications conventions */
	_vi_InitCopyPaste( f_copy, f_paste, f_clear, fprintf );
    }

    /* create the paned window */
    pane_w = XtVaCreateManagedWidget( "pane",
				      xmPanedWindowWidgetClass,
				      parent,
				      XmNinsertPosition,	insert_here,
				      NULL
				      );

    /* allocate our data structure.  in the future we will have several
     * screens running around at the same time
     */
    _vi_screen = create_screen( pane_w, 24, 80 );

    /* force creation of our color text context */
    set_gc_colors( _vi_screen, COLOR_STANDARD );

    /* done */
    return pane_w;
}


/* These routines deal with the selection buffer */

static	int	selection_start, selection_end, selection_anchor;
static	enum	select_enum {
	    select_char, select_word, select_line
	}	select_type = select_char;
static	int	last_click;

static	char	*clipboard = NULL;
static	int	clipboard_size = 0,
		clipboard_length;


#if defined(__STDC__)
static	void	copy_to_clipboard( xvi_screen *cur_screen )
#else
static	void	copy_to_clipboard( cur_screen )
xvi_screen	*cur_screen;
#endif
{
    /* for now, copy from the backing store.  in the future,
     * vi core will tell us exactly what the selection buffer contains
     */
    clipboard_length = 1 + selection_end - selection_start;

    if ( clipboard == NULL )
	clipboard = (char *) malloc( clipboard_length );
    else if ( clipboard_size < clipboard_length )
	clipboard = (char *) realloc( clipboard, clipboard_length );

    memcpy( clipboard,
	    cur_screen->characters + selection_start,
	    clipboard_length
	    );
}


#if defined(__STDC__)
static	void	mark_selection( xvi_screen *cur_screen, int start, int end )
#else
static	void	mark_selection( cur_screen, start, end )
xvi_screen	*cur_screen;
int		start;
int		end;
#endif
{
    int	row, col, i;

    for ( i=start; i<=end; i++ ) {
	if ( !( cur_screen->flags[i] & COLOR_SELECT ) ) {
	    cur_screen->flags[i] |= COLOR_SELECT;
	    ToRowCol( cur_screen, i, row, col );
	    _vi_draw_text( cur_screen, row, col, 1 );
	}
    }
}


#if defined(__STDC__)
static	void	erase_selection( xvi_screen *cur_screen, int start, int end )
#else
static	void	erase_selection( cur_screen, start, end )
xvi_screen	*cur_screen;
int		start;
int		end;
#endif
{
    int	row, col, i;

    for ( i=start; i<=end; i++ ) {
	if ( cur_screen->flags[i] & COLOR_SELECT ) {
	    cur_screen->flags[i] &= ~COLOR_SELECT;
	    ToRowCol( cur_screen, i, row, col );
	    _vi_draw_text( cur_screen, row, col, 1 );
	}
    }
}


#if defined(__STDC__)
static	void	left_expand_selection( xvi_screen *cur_screen, int *start )
#else
static	void	left_expand_selection( cur_screen, start )
xvi_screen	*cur_screen;
int		*start;
#endif
{
    int row, col;

    switch ( select_type ) {
	case select_word:
	    if ( *start == 0 || isspace( cur_screen->characters[*start] ) )
		return;
	    for (;;) {
		if ( isspace( cur_screen->characters[*start-1] ) )
		    return;
		if ( --(*start) == 0 )
		   return;
	    }
	case select_line:
	    ToRowCol( cur_screen, *start, row, col );
	    col = 0;
	    *start = Linear( cur_screen, row, col );
	    break;
    }
}


#if defined(__STDC__)
static	void	right_expand_selection( xvi_screen *cur_screen, int *end )
#else
static	void	right_expand_selection( cur_screen, end )
xvi_screen	*cur_screen;
int		*end;
#endif
{
    int row, col, last = cur_screen->cols * cur_screen->rows - 1;

    switch ( select_type ) {
	case select_word:
	    if ( *end == last || isspace( cur_screen->characters[*end] ) )
		return;
	    for (;;) {
		if ( isspace( cur_screen->characters[*end+1] ) )
		    return;
		if ( ++(*end) == last )
		   return;
	    }
	case select_line:
	    ToRowCol( cur_screen, *end, row, col );
	    col = cur_screen->cols -1;
	    *end = Linear( cur_screen, row, col );
	    break;
    }
}


#if defined(__STDC__)
static	void	select_start( Widget widget, 
			      XEvent *event,
			      String str, 
			      Cardinal *cardinal
			      )
#else
static	void	select_start( widget, event, str, cardinal )
Widget		widget;     
XEvent		*event;
String		str; 
Cardinal        *cardinal;
#endif
{
    char		buffer[BufferSize];
    IP_BUF		ipb;
    int			xpos, ypos;
    XPointerMovedEvent	*ev = (XPointerMovedEvent *) event;
    static int		last_click;

    /*
     * NOTE: when multiple panes are implemented, we need to find the correct
     * screen.  For now, there is only one.
     */
    xpos = COLUMN( _vi_screen, ev->x );
    ypos = ROW( _vi_screen, ev->y );

    /* Remove the old one. */
    erase_selection( _vi_screen, selection_start, selection_end );

    /* Send the new cursor position. */
    ipb.code = IPO_MOUSE_MOVE;
    ipb.val1 = ypos;
    ipb.val2 = xpos;
    (void)ip_send("12", &ipb);

    /* click-click, and we go for words, lines, etc */
    if ( ev->time - last_click < multi_click_length )
	select_type = (enum select_enum) ((((int)select_type)+1)%3);
    else
	select_type = select_char;
    last_click = ev->time;

    /* put the selection here */
    selection_anchor	= Linear( _vi_screen, ypos, xpos );
    selection_start	= selection_anchor;
    selection_end	= selection_anchor;

    /* expand to include words, line, etc */
    left_expand_selection( _vi_screen, &selection_start );
    right_expand_selection( _vi_screen, &selection_end );

    /* draw the new one */
    mark_selection( _vi_screen, selection_start, selection_end );

    /* and tell the window manager we own the selection */
    if ( select_type != select_char ) {
	_vi_AcquirePrimary( widget );
	copy_to_clipboard( _vi_screen );
    }
}


#if defined(__STDC__)
static	void	select_extend( Widget widget, 
			       XEvent *event,
			       String str, 
			       Cardinal *cardinal
			       )
#else
static	void	select_extend( widget, event, str, cardinal )
Widget		widget;     
XEvent		*event;
String		str; 
Cardinal        *cardinal;
#endif
{
    int			xpos, ypos, pos;
    XPointerMovedEvent	*ev = (XPointerMovedEvent *) event;

    /* NOTE:  when multiple panes are implemented, we need to find
     * the correct screen.  For now, there is only one.
     */
    xpos = COLUMN( _vi_screen, ev->x );
    ypos = ROW( _vi_screen, ev->y );

    /* deal with words, lines, etc */
    pos = Linear( _vi_screen, ypos, xpos );
    if ( pos < selection_anchor )
	left_expand_selection( _vi_screen, &pos );
    else
	right_expand_selection( _vi_screen, &pos );

    /* extend from before the start? */
    if ( pos < selection_start ) {
	mark_selection( _vi_screen, pos, selection_start-1 );
	selection_start = pos;
    }

    /* extend past the end? */
    else if ( pos > selection_end ) {
	mark_selection( _vi_screen, selection_end+1, pos );
	selection_end = pos;
    }

    /* between the anchor and the start? */
    else if ( pos < selection_anchor ) {
	erase_selection( _vi_screen, selection_start, pos-1 );
	selection_start = pos;
    }

    /* between the anchor and the end? */
    else {
	erase_selection( _vi_screen, pos+1, selection_end );
	selection_end = pos;
    }

    /* and tell the window manager we own the selection */
    _vi_AcquirePrimary( widget );
    copy_to_clipboard( _vi_screen );
}


#if defined(__STDC__)
static	void	select_paste( Widget widget, 
			      XEvent *event,
			      String str, 
			      Cardinal *cardinal
			      )
#else
static	void	select_paste( widget, event, str, cardinal )
Widget		widget;     
XEvent		*event;
String		str; 
Cardinal        *cardinal;
#endif
{
    _vi_PasteFromClipboard( widget );
}


/* Interface to copy and paste
 * (a) callbacks from the window manager
 *	f_copy	-	it wants our buffer
 *	f_paste	-	it wants us to paste some text
 *	f_clear	-	we've lost the selection, clear it
 */

#if defined(__STDC__)
static	void	f_copy( String *buffer, int *len )
#else
static	void	f_copy( buffer, len )
	String	*buffer;
	int	*len;
#endif
{
#ifdef TR
    trace("f_copy() called");
#endif
    *buffer	= clipboard;
    *len	= clipboard_length;
}



static	void	f_paste( widget, buffer, length )
	int widget, buffer, length;
{
    /* NOTE:  when multiple panes are implemented, we need to find
     * the correct screen.  For now, there is only one.
     */
#ifdef TR
    trace("f_paste() called with '%*.*s'\n", length, length, buffer);
#endif
}


#if defined(__STDC__)
static	void	f_clear( Widget widget )
#else
static	void	f_clear( widget )
Widget	widget;
#endif
{
    xvi_screen	*cur_screen;

#ifdef TR
    trace("f_clear() called");
#endif

    XtVaGetValues( widget, XmNuserData, &cur_screen, 0 );

    erase_selection( cur_screen, selection_start, selection_end );
}


/*
 * These routines deal with the cursor.
 *
 * PUBLIC: void _vi_set_cursor __P((xvi_screen *, int));
 */
void
_vi_set_cursor(cur_screen, is_busy)
	xvi_screen *cur_screen;
	int is_busy;
{
    XDefineCursor( XtDisplay(cur_screen->area),
		   XtWindow(cur_screen->area),
		   (is_busy) ? busy_cursor : std_cursor
		   );
}



/*
 * These routines deal with the caret.
 *
 * PUBLIC: void draw_caret __P((xvi_screen *));
 */
static void
draw_caret(this_screen)
	xvi_screen *this_screen;
{
    /* draw the caret by drawing the text in highlight color */
    *FlagAt( this_screen, this_screen->cury, this_screen->curx ) |= COLOR_CARET;
    _vi_draw_text( this_screen, this_screen->cury, this_screen->curx, 1 );
}

/*
 * PUBLIC: void _vi_erase_caret __P((xvi_screen *));
 */
void
_vi_erase_caret(this_screen)
	xvi_screen *this_screen;
{
    /* erase the caret by drawing the text in normal video */
    *FlagAt( this_screen, this_screen->cury, this_screen->curx ) &= ~COLOR_CARET;
    _vi_draw_text( this_screen, this_screen->cury, this_screen->curx, 1 );
}

/*
 * PUBLIC: void	_vi_move_caret __P((xvi_screen *, int, int));
 */
void
_vi_move_caret(this_screen, newy, newx)
	xvi_screen *this_screen;
	int newy, newx;
{
    /* caret is now here */
    _vi_erase_caret( this_screen );
    this_screen->curx = newx;
    this_screen->cury = newy;
    draw_caret( this_screen );
}

#if 0

int
main(argc, argv)
	int argc;
	char *argv[];
{
	/*
	 * Initialize the X widgetry.  We must do this before picking off
	 * arguments as well-behaved X programs have common argument lists
	 * (e.g. -rv for reverse video).
	 */
	ip_x_init(&argc, argv );

	/* Run vi: the child returns. */
	(void)run_vi(argc, argv, &i_fd, &o_fd);

	/* Initialize signals. */
	ip_siginit();

	/* tell X that we are interested in input on the pipe */
	XtAppAddInput( ctx, i_fd, XtInputReadMask, pipe_input_func, NULL );

	/* what does the user want to see? */
	_vi_set_cursor( cur_screen, False );

	/* vi wants a resize as the first event */
	send_resize( cur_screen );

	/* Main loop. */
	XtAppMainLoop( ctx );

	/* NOTREACHED */
	abort();
}
#endif
