/* TODO List:
 *	scrollbars	Need protocol messages that tell us what to display
 *			in the scrollbars.  Suggestion:
 *				scrollbar( bottom, lines, home )
 *				bottom is $
 *				lines is lines shown in the window
 *					(takes wrap into account)
 *				home is the line number ot the top visible line
 *
 *			On the way back send scroll( top )
 *
 *			User should be able to enable/disable bar display
 *
 *			<yuch!> horizontal scrollbar
 *
 *	expose_func
 *	insert/delete	When we have a partially obscured window, we only
 *			refresh a single line after scrolling.  I believe this
 *			is due to the exposure events all showing up after
 *			the scrolling is completed (pipe_input_func does all
 *			of the scrolling and then we get back to XtMainLoop)
 *
 *	split		Ought to be able to put a title on each pane
 *			Need protocol messages to shift focus
 *
 *	bell		user settable visible bell
 *
 *	busy		don't understand the protocol
 *
 *	mouse		need to send IPO_MOVE_CARET( row, column )
 *			(note that screen code does not know about tabs or
 *			line wraps)
 *			Connect to window manager cut buffer
 *			need to send IPO_EXTEND_SELECT( r1, c1, r2, c1 )
 *			otherwise core and screen duplicate selection logic
 *			Need to determine correct screen for event.  Not
 *			needed until split is implemented.
 *
 *	arrow keys	need to define a protocol.  I can easily send
 *			the vt100 sequences (and currently do).
 *			In general, we need to define what special keys
 *			do (for example PageUp) and what happens when we
 *			are in Insert mode.
 *
 *			Suggestion: IPO_COMMAND( string ).  vi core can
 *			take it as a command even when in insert mode.
 *
 *	icon		Is currently B&W.  To get a color icon, would
 *			require a lot of work or that bostic pick up
 *			the xpm library.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "X11/Intrinsic.h"
#include "X11/StringDefs.h"
#include "X11/cursorfont.h"
#include "Xm/PanedW.h"
#include "Xm/DrawingA.h"
#include "Xm/Form.h"
#include "Xm/Frame.h"
#include "Xm/ScrollBar.h"
#include "Xm/MainW.h"

#if XtSpecificationRelease == 4
#define	ArgcType	Cardinal *
#else
#define	ArgcType	int *
#endif

#include "xutilities.h"
#include "nvi.xbm"

#include "../common/common.h"
#include "../ip/ip.h"
#include "pathnames.h"

int	i_fd, o_fd;				/* Input/output fd's. */

void	arg_format __P((int *, char **[], int, int));
void	attach __P((void));
void	ip_cur_end __P((void));
void	ip_cur_init __P((void));
void	ip_read __P((void));
int	ip_send __P((char *, IP_BUF *));
void	ip_siginit __P((void));
int	ip_trans __P((char *, size_t, size_t *));
void	onchld __P((int));
void	onintr __P((int));
void	trace __P((const char *, ...));
void	usage __P((void));
static	void	f_copy();
static	void	f_paste();
static	void	f_clear();


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


#if ! defined(__STDC__)
#define	memmove(a,b,c)	bcopy( b,a,c )
#endif


/*
 * TR --
 *	debugging trace routine.
 */
#ifdef TR

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#define	TRACE( args )	trace args

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

#else
#define	TRACE( args )
#endif

void
usage()
{
	(void)fprintf(stderr, "usage: ip_cl [-D]\n");
	exit(1);
}


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
    "*iconName:			nvi 2.0",

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
	    fallback_rsrcs[i][0] = '*';
	}

	copy[i] = malloc( strlen(name) + strlen(fallback_rsrcs[i]) + 1 );
	strcpy( copy[i], name );
	strcat( copy[i], fallback_rsrcs[i] );
    }

    copy[i] = NULL;
    return copy;
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
    static	char	bp[ BufferSize ];
    static	int	len = 0, blen = BufferSize;
		int	nr, skip;

    /* Read waiting vi messags and translate to X calls. */
    switch (nr = read( *source, bp + len, blen - len)) {
    case 0:
	    TRACE( ("empty input from vi\n") );
	    return;
    case -1:
	    perror("ip_cl: read");
	    exit (1);
    default:
	    TRACE( ("input from vi, %d bytes read\n", nr) );
	    break;
    }

    /* Parse to data end or partial message. */
    for ( len += nr, skip = 0;
	  len > skip && ip_trans(bp + skip, len - skip, &skip) == 1;
	  )
	/* nothing */;

    /* Copy any partial messages down in the buffer. */
    len -= skip;
    if (len > 0)
	memmove(bp, bp + skip, len);
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

    TRACE( ("resize_func ( %d x %d )\n", this_screen->rows, this_screen->cols) );

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
    XConfigureEvent		*ev;
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
	TRACE( ("expose_func:  full refresh\n") );

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
		TRACE( ("expose_func (X):  (x=%d,y=%d,w=%d,h=%d), count=%d\n",
			     gev->x, gev->y,
			     gev->width, gev->height,
			     gev->count ) );

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
		TRACE( ("expose_func (Motif):  (x=%d,y=%d,w=%d,h=%d), count=%d\n",
			     xev->x, xev->y,
			     xev->width, xev->height,
			     xev->count ) );

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

    TRACE( ("insert_string {%.*s}\n", strlen( *str ), *str ) );
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
    int		i, sym;

    ipb.len = XLookupString( event, bp, BufferSize, NULL, NULL );
    if ( ipb.len != 0 ) {
	ipb.code = IPO_STRING;
	ipb.str = bp;
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
    Dimension	h, w;
    Pixel	fg, bg;
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
    TRACE( ( "f_copy() called" ) );
    *buffer	= clipboard;
    *len	= clipboard_length;
}



static	void	f_paste( widget, buffer, length )
{
    /* NOTE:  when multiple panes are implemented, we need to find
     * the correct screen.  For now, there is only one.
     */
    TRACE( ("f_paste() called with '%*.*s'\n", length, length, buffer ) );
}


#if defined(__STDC__)
static	void	f_clear( Widget widget )
#else
static	void	f_clear( widget )
Widget	widget;
#endif
{
    xvi_screen	*cur_screen;

    TRACE( ( "f_clear() called" ) );

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



int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;	/* RAZ 29 Oct 96 */
	fd_set fdset;
	pid_t pid;
	size_t blen, len, skip;
	int ch, nr, rpipe[2], wpipe[2];

	/* Initialize the X widgetry.
	 * We must do this before picking off arguments as well-behaved
	 * X programs have common argument lists (e.g. -rv for reverse
	 * video).
	 */
	ip_x_init( &argc, argv );

	while ((ch = getopt(argc, argv, "D")) != EOF)
		switch (ch) {
		case 'D':
			attach();
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * Open the communications pipes.  The pipes are named from our
	 * viewpoint, so we read from rpipe[0] and write to wpipe[1].
	 * Vi reads from wpipe[0], and writes to rpipe[1].
	 */
	if (pipe(rpipe) == -1 || pipe(wpipe) == -1) {
		perror("ip_cl: pipe");
		exit (1);
	}
	i_fd = rpipe[0];
	o_fd = wpipe[1];

	/*
	 * Format our arguments, adding a -I to the list.  The first file
	 * descriptor to the -I argument is vi's input, and the second is
	 * vi's output.
	 */
	arg_format(&argc, &argv, wpipe[0], rpipe[1]);

	/* Run vi. */
	switch (pid = fork()) {
	case -1:				/* Error. */
		perror("ip_cl: fork");
		exit (1);
	case 0:					/* Vi. */
		execv(VI, argv);
		perror("ip_cl: execv ../build/nvi");
		exit (1);
	default:				/* Ip_cl. */
		break;
	}

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
}


/*
 * ip_trans --
 *	Translate vi messages into X calls.
 */
int
ip_trans(bp, len, skipp)
	char *bp;
	size_t len, *skipp;
{
	IP_BUF ipb;
	size_t cno, lno, nlen, oldy, oldx, spcnt;
	int ch;
	char *fmt, *p;

	switch (bp[0]) {
	case IPO_ADDSTR:
	case IPO_RENAME:
		fmt = "s";
		break;
	case IPO_BUSY:
		fmt = "s1";
		break;
	case IPO_ATTRIBUTE:
	case IPO_MOVE:
		fmt = "12";
		break;
	case IPO_REWRITE:
		fmt = "1";
		break;
	default:
		fmt = "";
	}

	nlen = IPO_CODE_LEN;
	p = bp + IPO_CODE_LEN;
	for (; *fmt != '\0'; ++fmt)
		switch (*fmt) {
		case '1':
			nlen += IPO_INT_LEN;
			if (len < nlen)
				return (0);
			memcpy((char*)&ipb.val1, p, IPO_INT_LEN);
			ipb.val1 = ntohl(ipb.val1);
			p += IPO_INT_LEN;
			break;
		case '2':
			nlen += IPO_INT_LEN;
			if (len < nlen)
				return (0);
			memcpy((char*)&ipb.val2, p, IPO_INT_LEN);
			ipb.val2 = ntohl(ipb.val2);
			p += IPO_INT_LEN;
			break;
		case 's':
			nlen += IPO_INT_LEN;
			if (len < nlen)
				return (0);
			memcpy((char*)&ipb.len, p, IPO_INT_LEN);
			ipb.len = ntohl(ipb.len);
			p += IPO_INT_LEN;
			nlen += ipb.len;
			if (len < nlen)
				return (0);
			ipb.str = p;
			p += ipb.len;
			break;
		}
	*skipp += nlen;

	switch (bp[0]) {

	case IPO_ADDSTR:
		TRACE( ("addnstr {%.*s}\n", (int)ipb.len, ipb.str) );

		/* add to backing store */
		memcpy( CharAt(cur_screen, cur_screen->cury, cur_screen->curx),
			ipb.str,
			ipb.len
			);
		memset( FlagAt(cur_screen, cur_screen->cury, cur_screen->curx),
			cur_screen->color,
			ipb.len
			);

		/* draw from backing store */
		draw_text( cur_screen,
			   cur_screen->cury,
			   cur_screen->curx,
			   ipb.len
			   );

		/* advance the caret */
		move_caret( cur_screen,
			    cur_screen->cury,
			    cur_screen->curx + ipb.len
			    );
		break;

	case IPO_ATTRIBUTE:
		switch (ipb.val1) {
		case SA_ALTERNATE:
			TRACE( ("attr: alternate\n") );
			/*
			 * XXX
			 * Nothing.
			 */
			break;
		case SA_INVERSE:
			TRACE( ("attr: inverse\n") );
			cur_screen->color = ipb.val2;
			break;
		default:
			abort();
			/* NOTREACHED */
		}
		break;

	case IPO_BELL:
		/* future... implement visible bell */
		XBell( XtDisplay( cur_screen->area ), 0 );
		TRACE( ("bell\n") );
		break;

	case IPO_BUSY:
		TRACE( ("busy %d {%.*s}\n", ipb.val1, (int)ipb.len, ipb.str) );

#if 0
		/* I'm just guessing here, but I believe we are
		 * supposed to set the busy cursor when the text
		 * is non-null, and restore the default cursor otherwise.
		 */
		set_cursor( cur_screen, ipb.len != 0 );
#endif
		break;

	case IPO_CLRTOEOL:
		{
		int len = cur_screen->cols - cur_screen->curx;
		char *ptr = CharAt(cur_screen, cur_screen->cury, cur_screen->curx);

		TRACE( ("clrtoeol\n") );

		/* clear backing store */
		memset( ptr, ' ', len );
		memset( FlagAt(cur_screen, cur_screen->cury, cur_screen->curx),
			COLOR_STANDARD,
			len
			);

		/* draw from backing store */
		draw_text( cur_screen,
			   cur_screen->cury,
			   cur_screen->curx,
			   len
			   );
		}
		break;

	case IPO_DELETELN:
		{
		int y = cur_screen->cury,
		    rows = cur_screen->rows - y,
		    len = cur_screen->cols * rows,
		    height,
		    width;

		TRACE( ("deleteln\n") );

		/* don't want to copy the caret! */
		erase_caret( cur_screen );

		/* adjust backing store and the flags */
		memmove( CharAt( cur_screen, y, 0 ),
			 CharAt( cur_screen, y+1, 0 ),
			 len
			 );
		memmove( FlagAt( cur_screen, y, 0 ),
			 FlagAt( cur_screen, y+1, 0 ),
			 len
			 );

		/* move the bits on the screen */
		width = cur_screen->ch_width * cur_screen->cols;
		height = cur_screen->ch_height * rows;

		XCopyArea( XtDisplay(cur_screen->area),		/* display */
			   XtWindow(cur_screen->area),		/* src */
			   XtWindow(cur_screen->area),		/* dest */
			   copy_gc,				/* context */
			   0, YTOP( cur_screen, y+1 ),		/* srcx, srcy */
			   width, height,
			   0, YTOP( cur_screen, y )		/* dstx, dsty */
			   );

		}

		/* need to let X take over */
		XmUpdateDisplay( cur_screen->area );
		return 0;

	case IPO_INSERTLN:
		{
		int y = cur_screen->cury,
		    rows = cur_screen->rows - (1+y),
		    height,
		    width;
		char *from = CharAt( cur_screen, y, 0 ),
		     *to = CharAt( cur_screen, y+1, 0 );

		/* don't want to copy the caret! */
		erase_caret( cur_screen );

		/* adjust backing store */
		memmove( to, from, cur_screen->cols * rows );
		memset( from, ' ', cur_screen->cols );

		/* and the backing store */
		from = FlagAt( cur_screen, y, 0 ),
		to = FlagAt( cur_screen, y+1, 0 );
		memmove( to, from, cur_screen->cols * rows );
		memset( from, COLOR_STANDARD, cur_screen->cols );

		/* move the bits on the screen */
		width = cur_screen->ch_width * cur_screen->cols;
		height = cur_screen->ch_height * rows;

		XCopyArea( XtDisplay(cur_screen->area),		/* display */
			   XtWindow(cur_screen->area),		/* src */
			   XtWindow(cur_screen->area),		/* dest */
			   copy_gc,				/* context */
			   0, YTOP( cur_screen, y ),		/* srcx, srcy */
			   width, height,
			   0, YTOP( cur_screen, y+1 )		/* dstx, dsty */
			   );

		TRACE( ("insertln\n") );
		}

		/* need to let X take over */
		XmUpdateDisplay( cur_screen->area );
		return 0;

	case IPO_MOVE:
		TRACE( ("move: %lu %lu\n", (u_long)ipb.val1, (u_long)ipb.val2) );
		move_caret( cur_screen, ipb.val1, ipb.val2 );
		break;

	case IPO_REDRAW:
		TRACE( ("redraw\n") );
		expose_func( 0, cur_screen, 0 );
		break;

	case IPO_REFRESH:
#if 0
		/* force synchronous update of the widget */
		XmUpdateDisplay( cur_screen->area );
#endif
		break;

	case IPO_RENAME:
		/* Future:  Attach a title to each screen.  For now,
		 * we change the title of the shell
		 */
		TRACE( ("rename {%.*s}\n", (int)ipb.len, ipb.str) );
		XtVaSetValues( top_level,
			       XmNtitle,	ipb.str,
			       0
			       );
		break;

	case IPO_REWRITE:
#if 0
		TRACE( ("rewrite {%lu}\n", (u_long)ipb.val1) );
		getyx(stdscr, oldy, oldx);
		for (lno = ipb.val1, cno = spcnt = 0;;) {
			(void)move(lno, cno);
			ch = winch(stdscr);
			if (isblank(ch))
				++spcnt;
			else {
				(void)move(lno, cno - spcnt);
				for (; spcnt > 0; --spcnt)
					(void)addch(' ');
				(void)addch(ch);
			}
			if (++cno >= cols)
				break;
		}
		(void)move(oldy, oldx);
#endif
		break;

	default:
		/*
		 * XXX: Protocol is out of sync?  
		 */
		abort();
	}

	return (1);
}

/*
 * arg_format
 */
void
arg_format(argcp, argvp, i_fd, o_fd)
	int *argcp, i_fd, o_fd;
	char **argvp[];
{
	char **largv, *iarg, *p;

	/* Get space for the argument array and the -I argument. */
	if ((iarg = malloc(64)) == NULL ||
	    (largv = (char **) malloc((*argcp + 3) * sizeof(char *))) == NULL) {
		perror("ip_cl");
		exit (1);
	}
	memcpy((char*)largv + 2, (char *)*argvp, *argcp * sizeof(char *) + 1);

	/* Reset argv[0] to be the exec'd program. */
	if ((p = strrchr(VI, '/')) == NULL)
		largv[0] = VI;
	else
		largv[0] = p + 1;

	/* Create the -I argument. */
	(void)sprintf(iarg, "-I%d%s%d", i_fd, ".", o_fd);
	largv[1] = iarg;

	/* Reset the argument array. */
	*argvp = largv;
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
 * ip_send --
 *	Construct and send an IP buffer.
 */
int
ip_send(fmt, ipbp)
	char *fmt;
	IP_BUF *ipbp;
{
	static char *bp;
	static size_t blen;
	size_t off;
	u_int32_t ilen;
	int nlen, n, nw;
	char *p;

	/* have not created the channel to vi yet? */
	if ( o_fd == 0 )
		return (0);

	if (blen == 0 && (bp = malloc(blen = 512)) == NULL)
		nomem();

	p = bp;
	nlen = 0;
	*p++ = ipbp->code;
	nlen += IPO_CODE_LEN;

	if (fmt != NULL)
		for (; *fmt != '\0'; ++fmt)
			switch (*fmt) {
			case '1':			/* Value 1. */
				ilen = htonl(ipbp->val1);
				goto value;
			case '2':			/* Value 2. */
				ilen = htonl(ipbp->val2);
value:				nlen += IPO_INT_LEN;
				if (nlen >= blen) {
					blen = blen * 2 + nlen;
					off = p - bp;
					if ((bp = realloc(bp, blen)) == NULL)
						nomem();
					p = bp + off;
				}
				memmove(p, &ilen, IPO_INT_LEN);
				p += IPO_INT_LEN;
				break;
			case 's':			/* String. */
				ilen = ipbp->len;	/* XXX: conversion. */
				ilen = htonl(ilen);
				nlen += IPO_INT_LEN + ipbp->len;
				if (nlen >= blen) {
					blen = blen * 2 + nlen;
					off = p - bp;
					if ((bp = realloc(bp, blen)) == NULL)
						nomem();
					p = bp + off;
				}
				memmove(p, &ilen, IPO_INT_LEN);
				p += IPO_INT_LEN;
				memmove(p, ipbp->str, ipbp->len);
				p += ipbp->len;
				break;
			}
#ifdef TR
	TRACE( ("WROTE: ") );
	for (n = p - bp, p = bp; n > 0; --n, ++p)
		if (isprint(*p))
			(void)TRACE( ("%c", *p) );
		else
			TRACE( ("<%x>", (u_char)*p) );
	TRACE( ("\n") );
#endif

	for (n = p - bp, p = bp; n > 0; n -= nw, p += nw)
		if ((nw = write(o_fd, p, n)) < 0) {
			perror("ip_cl: write");
			exit(1);
		}

	return (0);
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

void
attach()
{
	int fd;
	char ch;

	(void)printf("process %lu waiting, enter <CR> to continue: ",
	    (u_long)getpid());
	(void)fflush(stdout);

	if ((fd = open(_PATH_TTY, O_RDONLY, 0)) < 0) {
		perror(_PATH_TTY);
		exit (1);;
	}
	do {
		if (read(fd, &ch, 1) != 1) {
			(void)close(fd);
			return;
		}
	} while (ch != '\n' && ch != '\r');
	(void)close(fd);
}

