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
static const char sccsid[] = "$Id: m_vi.c,v 8.11 1996/12/03 10:12:47 bostic Exp $ (Berkeley) $Date: 1996/12/03 10:12:47 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include "X11/Intrinsic.h"
#include "X11/StringDefs.h"
#include "X11/cursorfont.h"
#include "Xm/PanedW.h"
#include "Xm/DrawingA.h"
#include "Xm/Form.h"
#include "Xm/Frame.h"
#include "Xm/ScrollBar.h"
#include "Xm/MainW.h"

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if XtSpecificationRelease == 4
#define	ArgcType	Cardinal *
#else
#define	ArgcType	int *
#endif

#include "../common/common.h"
#include "../ip_vi/ip.h"
#include "ipc_mutil.h"
#include "ipc_extern.h"
#include "pathnames.h"

#include "nvi.xbm"

char *progname = "vi_curses";			/* Program name. */

int	i_fd, o_fd;				/* Input/output fd's. */

void	ip_siginit __P((void));
void	onchld __P((int));
void	onintr __P((int));
static	void	f_copy();
static	void	f_paste();
static	void	f_clear();


/*
 * debug trace routines
 */

#ifdef TR

static	FILE	*trace_fp;

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#ifdef __STDC__
trace(const char *fmt, ...)
#else
trace(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	static FILE *tfp;
	va_list ap;

	if (tfp == NULL && (tfp = fopen(TR, "w")) == NULL)
	tfp = stderr;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(tfp, fmt, ap);
	va_end(ap);

	(void)fflush(tfp);
}


void	trace_init()
{
	trace_fp = fopen( TR, "w" );
}
#endif


/*
 * describes a single 'screen' implemented in X widgetry
 */
typedef	struct {
    Widget	parent,		/* the pane */
		area,		/* text goes here */
		form,		/* holds text and scrollbar */
		scroll;		/* not connected yet */
    Region	clip;
    int		color;
    int		rows,
		cols;
    int		ch_width,
		ch_height,
		ch_descent;
    int		curx, cury;
    char	*characters;
    char	*flags;
} xvi_screen;

void	draw_caret __P( (xvi_screen *) );
void	set_cursor __P( (xvi_screen *, Boolean ) );

#define	ToRowCol( scr, lin, r, c )	\
	    r = (lin) / scr->cols;	\
	    c = ((lin) - r * (scr->cols)) % scr->cols;
#define	Linear( scr, y, x )	\
	    ( (y) * scr->cols + (x) )
#define	CharAt( scr, y, x )	\
	    ( scr->characters + Linear( scr, y, x ) )
#define	FlagAt( scr, y, x )	\
	    ( scr->flags + Linear( scr, y, x ) )

#define	XPOS( scr, x )	\
	scr->ch_width * (x)
#define	YTOP( scr, y )	\
	scr->ch_height * (y)
#define	YPOS( scr, y )	\
	YTOP( scr, ((y)+1) ) - scr->ch_descent

#define	ROW( scr, y )	\
	( (y) / scr->ch_height )

#define	COLUMN( scr, x )	\
	( (x) / scr->ch_width )


/*
 * Globals and costants
 */

#define	BufferSize	1024

XFontStruct	*font;
GC		gc;
GC		copy_gc;
Pixmap		icon_pm;
Pixel		icon_fg, icon_bg;

xvi_screen	*cur_screen = NULL;
Widget		top_level;
XtAppContext	ctx;
Cursor		std_cursor;
Cursor		busy_cursor;
XtTranslations	area_trans;
int		multi_click_length;

char	bp[ BufferSize ];		/* input buffer from pipe */
int	len = 0, blen = BufferSize;


/*
 * Color support
 */

#define	COLOR_INVALID	0xff	/* force color change */

/* These are color indeces.  When vi passes color info,
 * we can do 2..0x3f in the 8 bits i've allocated
 */
#define	COLOR_STANDARD	0x00	/* standard video */
#define	COLOR_INVERSE	0x01	/* reverse video */

/* These are flag bits.  they override the above colors */
#define	COLOR_CARET	0x80	/* draw the caret */
#define	COLOR_SELECT	0x40	/* draw the selection */


#if defined(__STDC__)
void		set_gc_colors( xvi_screen *this_screen, int val )
#else
void		set_gc_colors( this_screen, val )
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

void
nomem()
{
	perror("ip_cl");
	exit (1);
}

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

static XtActionsRec	area_actions[] = {
    { "select_start",	select_start	},
    { "select_extend",	select_extend	},
    { "select_paste",	select_paste	},
    { "key_press",	key_press	},
    { "insert_string",	insert_string	},
};

char	areaTrans[] =
    "<Btn1Down>:	select_start()		\n\
     <Btn3Down>:	select_extend()		\n\
     <Btn1Motion>:	select_extend()		\n\
     <Btn3Motion>:	select_extend()		\n\
     <Btn2Down>:	select_paste()		\n\
     <Key>Up:		insert_string(\033[A)	\n\
     <Key>Down:		insert_string(\033[B)	\n\
     <Key>Right:	insert_string(\033[C)	\n\
     <Key>Left:		insert_string(\033[D)	\n\
     <Key>Prior:	insert_string(\002)	\n\
     <Key>Next:		insert_string(\006)	\n\
     <Key>:		key_press()";

String	fallback_rsrcs[] = {

    "*font:			-*-*-*-r-*--14-*-*-*-m-*-*-*",
    "*pointerShape:		xterm",
    "*busyShape:		watch",
    "*iconName:			vi",

    /* When *NOT* running MWM, Motif needs to be told how the
     * virtual keys map to the physical keys.  There *ought* to
     * system defaults, but...
     *
     * In any event, it shouldn't be bad to define these...
     */

    "*defaultVirtualBindings:	\
    	osfPageUp	<Key>Prior	\n\
    	osfPageDown	<Key>Next	\n\
    	osfUp		<Key>Up		\n\
    	osfLeft		<Key>Left	\n\
    	osfRight	<Key>Right	\n\
    	osfDown		<Key>Down",

    /* coloring for the icons.  we may need to change this */
    "*iconForeground:	XtDefaultForeground",
    "*iconBackground:	XtDefaultBackground",

    /* --------------------------------------------------------------------- *
     * anything below this point is only defined when we are not running CDE *
     * --------------------------------------------------------------------- */

    /* Do not define default colors when running under CDE
     * (e.g. VUE on HPUX). The result is that you don't look
     * like a normal desktop application
     */
    "?highlightColor:		red",
    "?background:		gray75",
    "?screen.background:	wheat",
    "?highlightColor:		red"
};

static  XutResource resource[] = {
    { "font",		XutRKfont,	&font		},
    { "pointerShape",	XutRKcursor,	&std_cursor	},
    { "busyShape",	XutRKcursor,	&busy_cursor	},
    { "iconForeground",	XutRKpixel,	&icon_fg	},
    { "iconBackground",	XutRKpixel,	&icon_bg	},
};

#if defined(__STDC__)
static	String	*get_fallback_rsrcs( String name )
#else
static	String	*get_fallback_rsrcs( name )
	String	name;
#endif
{
    String	*copy = (String *) malloc( (1+XtNumber(fallback_rsrcs))*sizeof(String) );
    int		i, running_cde;
    Display	*d;

    /* connect to server and see if the CDE atoms are present */
    d = XOpenDisplay(0);
    running_cde = is_cde( d );
    XCloseDisplay(d);

    for ( i=0; i<XtNumber(fallback_rsrcs); i++ ) {

	/* stop here if running CDE */
	if ( fallback_rsrcs[i][0] == '?' ) {
	    if ( running_cde ) break;
	    if ((fallback_rsrcs[i] = strdup(fallback_rsrcs[i])) == NULL)
		nomem();
	    fallback_rsrcs[i][0] = '*';
	}

	copy[i] = malloc( strlen(name) + strlen(fallback_rsrcs[i]) + 1 );
	strcpy( copy[i], name );
	strcat( copy[i], fallback_rsrcs[i] );
    }

    copy[i] = NULL;
    return copy;
}


#if defined(__STDC__)
Boolean		process_pipe_input( XtPointer pread )
#else
Boolean		process_pipe_input( pread )
XtPointer	pread;
#endif
{
    int	nr, skip;

    /* might have read more since the last call */
    nr = (pread) ? *((int *)pread) : 0;
    len += nr;
    skip = 0;

    /* Parse to data end or partial message. */
    while ( len > skip )
	if ( ! ip_trans(bp + skip, len - skip, &skip) )
	    break;

    /* Copy any partial messages down in the buffer. */
    len -= skip;
    if (len > 0) {
	    memmove(bp, bp + skip, len);
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
void		pipe_input_func( XtPointer client_data,
				 int *source,
				 XtInputId id
				 )
#else
void		pipe_input_func( client_data, source, id )
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
	    return;
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
}


/* Send the window size. */
#if defined(__STDC__)
void		send_resize( xvi_screen *this_screen )
#else
void		send_resize( this_screen )
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
void		resize_backing_store( xvi_screen *this_screen )
#else
void		resize_backing_store( this_screen )
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
void		resize_func( wid, client_data, call_data )
Widget		wid;
XtPointer	client_data;
XtPointer	call_data;
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


/* draw from backing store */
#if defined(__STDC__)
void		draw_text( xvi_screen *this_screen,
			   int row,
			   int start_col,
			   int len
			   )
#else
void		draw_text( this_screen, row, start_col, len )
xvi_screen	*this_screen;
int		row;
int		start_col;
int		len;
#endif
{
    int		col, color, xpos;
    char	*start, *end;

    start = CharAt( cur_screen, row, start_col );
    color = *FlagAt( cur_screen, row, start_col );
    xpos  = XPOS( cur_screen, start_col );

    /* one column at a time */
    for ( col=start_col;
	  col<this_screen->cols && col<start_col+len;
	  col++ ) {

	/* has the color changed? */
	if ( *FlagAt( cur_screen, row, col ) == color )
	    continue;

	/* is there anything to write? */
	end  = CharAt( cur_screen, row, col );
	if ( end == start )
	    continue;

	/* yes. write in the previous color */
	set_gc_colors( cur_screen, color );

	/* add to display */
	XDrawImageString( XtDisplay(cur_screen->area),
			  XtWindow(cur_screen->area),
			  gc,
			  xpos,
			  YPOS( cur_screen, row ),
			  start,
			  end - start
			  );

	/* this is the new context */
	color = *FlagAt( cur_screen, row, col );
	xpos  = XPOS( cur_screen, col );
	start = end;
    }

    /* is there anything to write? */
    end = CharAt( cur_screen, row, col );
    if ( end != start ) {
	/* yes. write in the previous color */
	set_gc_colors( cur_screen, color );

	/* add to display */
	XDrawImageString( XtDisplay(cur_screen->area),
			  XtWindow(cur_screen->area),
			  gc,
			  xpos,
			  YPOS( cur_screen, row ),
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


/* redraw the window's contents.
 * NOTE:  when vi wants to force a redraw, we are called with
 * NULL widget and call_data
 */
#if defined(__STDC__)
void		expose_func( Widget wid,
			     XtPointer client_data,
			     XtPointer call_data
			     )
#else
void		expose_func( wid, client_data, call_data )
Widget		wid;
XtPointer	client_data;
XtPointer	call_data;
#endif
{
    xvi_screen			*this_screen;
    XmDrawingAreaCallbackStruct	*cbs;
    XExposeEvent		*xev;
    XGraphicsExposeEvent	*gev;
    int				row;

    /* convert pointers */
    this_screen = (xvi_screen *) client_data;
    cbs		= (XmDrawingAreaCallbackStruct *) call_data;

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
	draw_text( this_screen, row, 0, this_screen->cols );
    }

    /* clear clipping region */
    XSetClipMask( XtDisplay(this_screen->area), gc, None );
    if ( this_screen->clip != NULL ) {
	XDestroyRegion( this_screen->clip );
	this_screen->clip = NULL;
    }

}


#if defined(__STDC__)
void		xexpose	( Widget w,
			  XtPointer client_data,
			  XEvent *ev,
			  Boolean *cont
			  )
#else
void		xexpose	( w, client_data, ev, cont )
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
	    expose_func( w, client_data, (XtPointer) &cbs );
	    *cont	= False;	/* we took care of it */
	    break;
	default:
	    /* don't care */
	    break;
    }
}


/* mouse or keyboard input. */
#if defined(__STDC__)
void		insert_string( Widget widget, 
			       XKeyEvent *event, 
			       String *str, 
			       Cardinal *cardinal
			       )
#else
void		insert_string( widget, event, str, cardinal )
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
void		key_press( Widget widget, 
			   XKeyEvent *event, 
			   String str, 
			   Cardinal *cardinal
			   )
#else
void		key_press( widget, event, str, cardinal )
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
xvi_screen	*create_screen( Widget parent, int rows, int cols )
#else
xvi_screen	*create_screen( parent, rows, cols )
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
		   expose_func,
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


xvi_screen	*split_screen()
{
    Cardinal	num;
    WidgetList	c;
    int		rows = cur_screen->rows / 2;
    xvi_screen	*new_screen;

    /* Note that (global) cur_screen needs to be correctly set so that
     * insert_here knows which screen to put the new one after
     */
    new_screen = create_screen( cur_screen->parent,
				rows,
				cur_screen->cols
				);

    /* what are the screens? */
    XtVaGetValues( cur_screen->parent,
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
    XtVaSetValues( cur_screen->form,
		   XmNheight,	cur_screen->ch_height * rows,
		   NULL
		   );

    /* re-manage */
    XtManageChildren( c, num );
}


/* Tell me where to insert the next subpane */
#if defined(__STDC__)
Cardinal	insert_here( Widget wid )
#else
Cardinal	insert_here( wid )
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
	if ( cur_screen == NULL || cur_screen->form == c[i] )
	    return i+1;	/* after the i-th */
    }

    /* could not find it?  this should never happen */
    return num;
}


/* create the necessary widgetry */
#if defined(__STDC__)
void	ip_x_init( int *argc, char **argv )
#else
void	ip_x_init( argc, argv )
int	*argc;
char	**argv;
#endif
{
    char	*ptr;
    Widget	main_w, menu_b, pane_w;
    Display	*display;

#if XtSpecificationRelease == 4
#define	ArgcType	Cardinal *
#else
#define	ArgcType	int *
#endif

    /* X gets quite upset if the program name is not simple */
    if (( ptr = strrchr( argv[0], '/' )) != NULL ) argv[0] = ++ptr;

    /* create a top-level shell for the window manager */
    top_level = XtVaAppInitialize( &ctx,
				   argv[0],
				   NULL, 0,	/* options */
				   (ArgcType) argc,
				   argv,	/* might get modified */
				   get_fallback_rsrcs( argv[0] ),
				   NULL
				   );
    display = XtDisplay(top_level);

    /* might need to go technicolor... */
    XutInstallColormap( argv[0], top_level );

    /* add our own special actions */
    XtAppAddActions( ctx, area_actions, XtNumber(area_actions) );

    /* how long is double-click? */
    multi_click_length = XtGetMultiClickTime( display );

    /* check the resource database for interesting resources */
    XutConvertResources( top_level,
			 argv[0],
			 resource,
			 XtNumber(resource)
			 );

    /* we need a context for moving bits around in the windows */
    copy_gc = XCreateGC( display,
			 DefaultRootWindow(display),
			 0,
			 0
			 );

    /* create our icon
     * do this *before* realizing the shell widget in case the -iconic
     * option was specified.
     */
    icon_pm = XCreatePixmapFromBitmapData(
			display,
		        DefaultRootWindow(display),
			(char *) nvi_bits,
			nvi_width,
			nvi_height,
			icon_fg,
			icon_bg,
			DefaultDepth( display, DefaultScreen(display) )
			);
    XutSetIcon( top_level, nvi_height, nvi_width, icon_pm );

    /* in the shell, we will stack a menubar and paned window */
    main_w = XtVaCreateManagedWidget( "main",
				      xmMainWindowWidgetClass,
				      top_level,
				      NULL
				      );

    /* create the menubar */
    menu_b = (Widget) create_menubar( main_w );
    XtManageChild( menu_b );

    /* create the paned window */
    pane_w = XtVaCreateManagedWidget( "pane",
				      xmPanedWindowWidgetClass,
				      main_w,
				      XmNinsertPosition,	insert_here,
				      NULL
				      );

    /* allocate our data structure.  in the future we will have several
     * screens running around at the same time
     */
    cur_screen = create_screen( pane_w, 24, 80 );

    /* force creation of our color text context */
    set_gc_colors( cur_screen, COLOR_STANDARD );

    /* routines for inter client communications conventions */
    InitCopyPaste( f_copy, f_paste, f_clear, fprintf );

    /* put it up */
    XtRealizeWidget( top_level );
}


/* These routines deal with the selection buffer */

int	selection_start, selection_end, selection_anchor;
enum	select_enum {
	    select_char, select_word, select_line
	} select_type = select_char;
int	last_click;

char	*clipboard = NULL;
int	clipboard_size = 0,
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
void		mark_selection( xvi_screen *cur_screen, int start, int end )
#else
void		mark_selection( cur_screen, start, end )
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
	    draw_text( cur_screen, row, col, 1 );
	}
    }
}


#if defined(__STDC__)
void		erase_selection( xvi_screen *cur_screen, int start, int end )
#else
void		erase_selection( cur_screen, start, end )
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
	    draw_text( cur_screen, row, col, 1 );
	}
    }
}


#if defined(__STDC__)
void		left_expand_selection( xvi_screen *cur_screen, int *start )
#else
void		left_expand_selection( cur_screen, start )
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
void		right_expand_selection( xvi_screen *cur_screen, int *end )
#else
void		right_expand_selection( cur_screen, end )
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

    /* NOTE:  when multiple panes are implemented, we need to find
     * the correct screen.  For now, there is only one.
     */
    xpos = COLUMN( cur_screen, ev->x );
    ypos = ROW( cur_screen, ev->y );

    /* remove the old one */
    erase_selection( cur_screen, selection_start, selection_end );

    /* left click should also move the caret for vi.
     * we really want to send an r,c position, but for now
     * the protocol is only existing vi commands.  Note that the |
     * will get the correct column, but we can't take into account
     * tabs or line wrappping
     */
    if ( ypos == 0 )
	sprintf( buffer, "H%d|", xpos+1 );
    else
	sprintf( buffer, "H%dj%d|", ypos, xpos+1 );
    ipb.len = strlen( buffer );
    ipb.code = IPO_STRING;
    ipb.str = buffer;
    ip_send("s", &ipb);

    /* click-click, and we go for words, lines, etc */
    if ( ev->time - last_click < multi_click_length )
	select_type = (enum select_enum) ((((int)select_type)+1)%3);
    else
	select_type = select_char;
    last_click = ev->time;

    /* put the selection here */
    selection_anchor	= Linear( cur_screen, ypos, xpos );
    selection_start	= selection_anchor;
    selection_end	= selection_anchor;

    /* expand to include words, line, etc */
    left_expand_selection( cur_screen, &selection_start );
    right_expand_selection( cur_screen, &selection_end );

    /* draw the new one */
    mark_selection( cur_screen, selection_start, selection_end );

    /* and tell the window manager we own the selection */
    if ( select_type != select_char ) {
	AcquirePrimary( widget );
	copy_to_clipboard( cur_screen );
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
    xpos = COLUMN( cur_screen, ev->x );
    ypos = ROW( cur_screen, ev->y );

    /* deal with words, lines, etc */
    pos = Linear( cur_screen, ypos, xpos );
    if ( pos < selection_anchor )
	left_expand_selection( cur_screen, &pos );
    else
	right_expand_selection( cur_screen, &pos );

    /* extend from before the start? */
    if ( pos < selection_start ) {
	mark_selection( cur_screen, pos, selection_start-1 );
	selection_start = pos;
    }

    /* extend past the end? */
    else if ( pos > selection_end ) {
	mark_selection( cur_screen, selection_end+1, pos );
	selection_end = pos;
    }

    /* between the anchor and the start? */
    else if ( pos < selection_anchor ) {
	erase_selection( cur_screen, selection_start, pos-1 );
	selection_start = pos;
    }

    /* between the anchor and the end? */
    else {
	erase_selection( cur_screen, pos+1, selection_end );
	selection_end = pos;
    }

    /* and tell the window manager we own the selection */
    AcquirePrimary( widget );
    copy_to_clipboard( cur_screen );
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
    PasteFromClipboard( widget );
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


/* These routines deal with the cursor */

#if defined(__STDC__)
void		set_cursor( xvi_screen *cur_screen, Boolean is_busy )
#else
void		set_cursor( cur_screen, is_busy )
xvi_screen	*cur_screen;
Boolean		is_busy;
#endif
{
    XDefineCursor( XtDisplay(cur_screen->area),
		   XtWindow(cur_screen->area),
		   (is_busy) ? busy_cursor : std_cursor
		   );
}



/* These routines deal with the caret */

#if defined(__STDC__)
void		draw_caret( xvi_screen *this_screen )
#else
void		draw_caret( this_screen )
xvi_screen	*this_screen;
#endif
{
    /* draw the caret by drawing the text in highlight color */
    *FlagAt( cur_screen, this_screen->cury, this_screen->curx ) |= COLOR_CARET;
    draw_text( this_screen, this_screen->cury, this_screen->curx, 1 );
}


#if defined(__STDC__)
void		erase_caret( xvi_screen *this_screen )
#else
void		erase_caret( this_screen )
xvi_screen	*this_screen;
#endif
{
    /* erase the caret by drawing the text in normal video */
    *FlagAt( cur_screen, this_screen->cury, this_screen->curx ) &= ~COLOR_CARET;
    draw_text( cur_screen, this_screen->cury, this_screen->curx, 1 );
}


#if defined(__STDC__)
void		move_caret( xvi_screen *this_screen, int newy, int newx )
#else
void		move_caret( this_screen, newy, newx )
xvi_screen	*this_screen;
int		newy;
int		newx;
#endif
{
    /* caret is now here */
    erase_caret( this_screen );
    this_screen->curx = newx;
    this_screen->cury = newy;
    draw_caret( this_screen );
}

#include "ipc_mfunc.c"


int
main(argc, argv)
	int argc;
	char *argv[];
{
#ifdef TR
	/*
	 * init debug print
	 */
	trace_init();
#endif
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
	set_cursor( cur_screen, False );

	/* vi wants a resize as the first event */
	send_resize( cur_screen );

	/* Main loop. */
	XtAppMainLoop( ctx );

	/* NOTREACHED */
	abort();
}

/*
 * ip_siginit --
 *	Initialize the signals.
 */
void
ip_siginit()
{
	/* We need to know if vi dies horribly. */
	(void)signal(SIGCHLD, onchld);

	/* We want to allow interruption at least for now. */
	(void)signal(SIGINT, onintr);
}

/*
 * onchld --
 *	Handle SIGCHLD.
 */
void
onchld(signo)
	int signo;
{
    /* not sure at the moment, but it's likely the case that
     * if the vi process goes away, we should too
     */
    exit(0);
}

/*
 * onintr --
 *	Handle SIGINT.
 */
void
onintr(signo)
	int signo;
{
	(void)signal(SIGINT, SIG_DFL);
	kill(getpid(), SIGINT);
}

/*
 * usage --
 *	Usage message.
 */
void
usage()
{
	(void)fprintf(stderr,
	    "usage: vi_curses [-D] [-P vi_program] [vi arguments]\n");
	exit(1);
}
