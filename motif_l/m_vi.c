/* TODO List:
 *	scrollbars	Wrap a scrolledWindow widget around the
 *			drawingArea (or create scrollbar manually because we
 *			will manually manage it anyway)
 *			User should be able to enable/disable bar display
 *
 *	expose_func	optimize for redraw of clipped rectangle(s) only
 *
 *	insert/delete	move display bits manually.  calling expose_func
 *			is too expensive
 *
 *	split		implement split screens by adding children to
 *			the paned window.  Ought to be able to put a
 *			title on each pane
 *
 *	bell		user settable visible bell
 *
 *	busy		don't understand the protocol
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
#include "Xm/MainW.h"
#include "xutilities.h"

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


/*
 * describes a single 'screen' implemented in X widgetry
 */

typedef	struct {
    Widget	area;
    GC		gc;
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


/*
 * Color support
 */

#define	COLOR_INVALID	-1
#define	COLOR_STANDARD	0
#define	COLOR_INVERSE	1


#if defined(__STDC__)
void		set_gc_colors( xvi_screen *this_screen, int val )
#else
void		set_gc_colors( this_screen, val )
xvi_screen	*this_screen;
int		val;
#endif
{
    Pixel	fg, bg;

    /* no change? */
    if ( this_screen->color == val ) return;

    /* what colors are selected for the drawing area? */
    XtVaGetValues( this_screen->area,
		   XtNbackground,	&bg,
		   XtNforeground,	&fg,
		   0
		   );

    switch (this_screen->color = val) {
	case COLOR_STANDARD:
	    XSetForeground( XtDisplay(this_screen->area), this_screen->gc, fg );
	    XSetBackground( XtDisplay(this_screen->area), this_screen->gc, bg );
	    break;
	case COLOR_INVERSE:
	    XSetForeground( XtDisplay(this_screen->area), this_screen->gc, bg );
	    XSetBackground( XtDisplay(this_screen->area), this_screen->gc, fg );
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

#define	CharAt( scr, y, x )	\
	    ( scr->characters + (y) * scr->cols + (x) )

#define	FlagAt( scr, y, x )	\
	    ( scr->flags + (y) * scr->cols + (x) )

#define	XPOS( scr, x )	\
	scr->ch_width * (x)
#define	YPOS( scr, y )	\
	scr->ch_height * ((y)+1) - scr->ch_descent

xvi_screen	*cur_screen = NULL;
Widget		top_level;
XtAppContext	ctx;
XFontStruct	*font;
Cursor		std_cursor;
Cursor		busy_cursor;
GC		gc_highlight;

static	String	fallback_rsrcs[] = {

    /* Do not define default colors when running under CDE
     * (e.g. VUE on HPUX). The result is that you don't look
     * like a normal desktop application
     */
    "*highlightColor:		red",
    "*background:		gray75",
    "*screen.background:	wheat",
    "*highlightColor:		red",

    "*screen.borderWidth:	1",
    "*pane.borderWidth:		1",

    "*font:			-*-*-*-r-*--14-*-*-*-m-*-*-*",
    "*pointerShape:		xterm",
    "*busyShape:		watch",
    "*screen*borderWidth:	1",
    "*pane*borderWidth:		1",
    NULL
};

static  XutResource resource[] = {
    { "font",		XutRKfont,	&font		},
    { "pointerShape",	XutRKcursor,	&std_cursor	},
    { "busyShape",	XutRKcursor,	&busy_cursor	},
};

static	String	*get_fallback_rsrcs( name )
	String	name;
{
    String	*copy = (String *) malloc( XtNumber(fallback_rsrcs)*sizeof(String) );
    int		i;

    for ( i=0; i<XtNumber(fallback_rsrcs); i++ ) {
	if ( fallback_rsrcs[i] != NULL ) {
	    copy[i] = malloc( strlen(name) + strlen(fallback_rsrcs[i]) + 1 );
	    strcpy( copy[i], name );
	    strcat( copy[i], fallback_rsrcs[i] );
	}
	else
	    copy[i] = NULL;
    }

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
    static	char	bp[ 1024 ];
    static	int	len = 0, blen = 1024;
		int	nr, skip;

    TRACE( ("input from vi\n") );

    /* Read waiting vi messags and translate to X calls. */
    switch (nr = read( *source, bp + len, blen - len)) {
    case 0:
	    return;
    case -1:
	    perror("ip_cl: read");
	    exit (1);
    default:
	    break;
    }

    /* Parse to data end or partial message. */
    for (len += nr, skip = 0; len > skip &&
	ip_trans(bp + skip, len - skip, &skip) == 1;);

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
    XmDrawingAreaCallbackStruct	*cbs;
    XConfigureEvent		*ev;
    Dimension			height, width;

    XtVaGetValues( wid, XmNheight, &height, XmNwidth, &width, 0 );

    /* generate correct sizes when we have font metrics implemented */
    this_screen->cols = width / this_screen->ch_width;
    this_screen->rows = height / this_screen->ch_height;

    resize_backing_store( this_screen );
    send_resize( this_screen );
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
    IP_BUF			ipb;
    xvi_screen			*this_screen = (xvi_screen *) client_data;
    XmDrawingAreaCallbackStruct	*cbs;
    int				row, col, xpos;
    int				color, save_color;
    char			*start, *end;

    /* future:  only do redraw when last expose is received.
     *		set clipping rectangles accordingly
     */

    XClearWindow( XtDisplay(cur_screen->area),
		  XtWindow(cur_screen->area)
		  );

    /* save context */
    save_color = this_screen->color;

    /* one row at a time */
    for (row=0; row<this_screen->rows; row++) {

	start = CharAt( cur_screen, row, 0 );
	color = *FlagAt( cur_screen, row, 0 );
	xpos  = XPOS( cur_screen, 0 );

	/* one column at a time */
	for (col=0; col<this_screen->cols; col++) {

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
			      cur_screen->gc,
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
			      cur_screen->gc,
			      xpos,
			      YPOS( cur_screen, row ),
			      start,
			      end - start
			      );
	}
    }

    /* when vi wants to force a redraw, we are called with
     * NULL widget and call_data.  When we are called from X,
     * we must restore the caret
     */
    draw_caret( cur_screen );

    TRACE( ("expose_func\n") );
}


/* mouse or keyboard input. */
#if defined(__STDC__)
void		input_func( Widget wid,
			    XtPointer client_data,
			    XtPointer call_data
			    )
#else
void		input_func( wid, client_data, call_data )
Widget		wid;
XtPointer	client_data;
XtPointer	call_data;
#endif
{
    IP_BUF			ipb;
    xvi_screen			*this_screen = (xvi_screen *) client_data;
    XmDrawingAreaCallbackStruct	*cbs;
    XEvent			*ev;
    char			bp[1024];

    cbs = (XmDrawingAreaCallbackStruct *) call_data;
    ev = cbs->event;
    switch( ev->type ) {
	case KeyPress:
	    ipb.len = XLookupString( (XKeyEvent *) ev, bp, 1024, NULL, NULL );
	    if ( ipb.len != 0 ) {
		ipb.code = IPO_STRING;
		ipb.str = bp;
		ip_send("s", &ipb);
	    }
	    break;
	case ButtonPress:
	    break;
	default:
	    break;
    }
}


#if defined(__STDC__)
xvi_screen	*create_screen( Widget parent )
#else
xvi_screen	*create_screen( parent )
Widget		parent;
#endif
{
    xvi_screen	*new_screen = (xvi_screen *) calloc( 1, sizeof(xvi_screen) );

    /* figure out the sizes */
    new_screen->color		= COLOR_INVALID;	/* force GC */
    new_screen->rows		= 24;
    new_screen->cols		= 80;
    new_screen->ch_width	= font->max_bounds.width;
    new_screen->ch_height	= font->descent + font->ascent;
    new_screen->ch_descent	= font->descent;

    /* allocate and init the backing stores */
    resize_backing_store( new_screen );

    /* populate it with a drawing area into which we will put text */
    new_screen->area = XtVaCreateManagedWidget( "screen",
		    xmDrawingAreaWidgetClass,
		    parent,
		    XmNheight,	new_screen->ch_height * new_screen->rows,
		    XmNwidth,	new_screen->ch_width * new_screen->cols,
		    NULL
		    );

    /* this callback is for keyboard and mouse input */
    XtAddCallback( new_screen->area,
		   XmNinputCallback,
		   input_func,
		   new_screen
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

    /* create the drawing context.  as a default we'll just set the
     * font and fg/bg colors and function.
     */
    new_screen->gc = XCreateGC(	XtDisplay(parent),
				DefaultRootWindow(XtDisplay(parent)),
				0,
				0
				);
    set_gc_colors( new_screen, COLOR_STANDARD );
    XSetFont( XtDisplay(new_screen->area), new_screen->gc, font->fid );

    return new_screen;
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

    /* X gets quite upset if the program name is not simple */
    if (( ptr = strrchr( argv[0], '/' )) != NULL ) argv[0] = ++ptr;

    /* create a top-level shell for the window manager */
    top_level = XtAppInitialize( &ctx,
				 argv[0],
				 NULL, 0,	/* options */
				 argc, argv,	/* might get modified */
				 get_fallback_rsrcs( argv[0] ),
				 NULL, 0	/* args */
				 );

    /* check the resource database for interesting resources */
    XutConvertResources( top_level,
			 argv[0],
			 resource,
			 XtNumber(resource)
			 );

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
				      NULL
				      );

    /* allocate our data structure.  in the future we will have several
     * screens running around at the same time
     */
    cur_screen = create_screen( pane_w );

    /* put it up */
    XtRealizeWidget( top_level );
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

/* move the caret */
#if defined(__STDC__)
void		draw_caret( xvi_screen *this_screen )
#else
void		move_caret( this_screen )
xvi_screen	*this_screen;
#endif
{
    Pixel	fg, hi;

    /* what color do we draw the caret? */
    if ( gc_highlight == NULL ) {
	XtVaGetValues( this_screen->area,
		       XtNforeground,		&fg,
		       XmNhighlightColor,	&hi,
		       0
		       );
	gc_highlight = XCreateGC( XtDisplay(this_screen->area),
				  DefaultRootWindow(XtDisplay(this_screen->area)),
				  0,
				  0
				  );
	XSetFont( XtDisplay(this_screen->area), gc_highlight, font->fid );
	XSetForeground( XtDisplay(this_screen->area), gc_highlight, fg );
	XSetBackground( XtDisplay(this_screen->area), gc_highlight, hi );
    }

    /* draw the caret by drawing the text in highlight color */
    XDrawImageString( XtDisplay(this_screen->area),
		      XtWindow(this_screen->area),
		      gc_highlight,
		      XPOS( this_screen, this_screen->curx ),
		      YPOS( this_screen, this_screen->cury ),
		      CharAt( this_screen,
			      this_screen->cury,
			      this_screen->curx
			      ),
		      1
		      );
}

/* move the caret */
#if defined(__STDC__)
void		move_caret( xvi_screen *this_screen, int newy, int newx )
#else
void		move_caret( this_screen, newy, newx )
xvi_screen	*this_screen;
int		newy;
int		newx,
#endif
{
    int color = this_screen->color;

    /* erase the caret by drawing the text in normal video */
    set_gc_colors( cur_screen,
		   *FlagAt( this_screen,
			    this_screen->cury,
			    this_screen->curx
			    )
		  );
    XDrawImageString( XtDisplay(this_screen->area),
		      XtWindow(this_screen->area),
		      this_screen->gc,
		      XPOS( this_screen, this_screen->curx ),
		      YPOS( this_screen, this_screen->cury ),
		      CharAt( this_screen,
			      this_screen->cury,
			      this_screen->curx
			      ),
		      1
		      );
    set_gc_colors( cur_screen, color );

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

		/* add to display */
		XDrawImageString( XtDisplay(cur_screen->area),
				  XtWindow(cur_screen->area),
				  cur_screen->gc,
				  XPOS( cur_screen, cur_screen->curx ),
				  YPOS( cur_screen, cur_screen->cury ),
				  ipb.str,
				  ipb.len
				  );

		/* add to backing store */
		memcpy( CharAt(cur_screen, cur_screen->cury, cur_screen->curx),
			ipb.str,
			ipb.len
			);
		memset( FlagAt(cur_screen, cur_screen->cury, cur_screen->curx),
			cur_screen->color,
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
			set_gc_colors( cur_screen, ipb.val2 );
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

		/* clear display (note that we use the cleared backing store) */
		XDrawImageString( XtDisplay(cur_screen->area),
				  XtWindow(cur_screen->area),
				  cur_screen->gc,
				  XPOS( cur_screen, cur_screen->curx ),
				  YPOS( cur_screen, cur_screen->cury ),
				  ptr,
				  len
				  );
		}
		break;

	case IPO_DELETELN:
		{
		int len = cur_screen->cols * (cur_screen->rows - cur_screen->cury);

		/* adjust backing store and the flags */
		memmove( CharAt( cur_screen, cur_screen->cury, 0 ),
			 CharAt( cur_screen, cur_screen->cury+1, 0 ),
			 len
			 );
		memmove( FlagAt( cur_screen, cur_screen->cury, 0 ),
			 FlagAt( cur_screen, cur_screen->cury+1, 0 ),
			 len
			 );

		/* force a refresh */
		expose_func( 0, cur_screen, 0 );

		TRACE( ("deleteln\n") );
		}
		break;

	case IPO_INSERTLN:
		{
		char *from = CharAt( cur_screen, cur_screen->cury, 0 ),
		     *to = CharAt( cur_screen, cur_screen->cury+1, 0 );
		int rows = cur_screen->rows - (1+cur_screen->cury);

		/* adjust backing store */
		memmove( to, from, cur_screen->cols * rows );
		memset( from, ' ', cur_screen->cols );

		/* and the backing store */
		from = FlagAt( cur_screen, cur_screen->cury, 0 ),
		to = FlagAt( cur_screen, cur_screen->cury+1, 0 );
		memmove( to, from, cur_screen->cols * rows );
		memset( from, COLOR_STANDARD, cur_screen->cols );

		/* force a refresh */
		expose_func( 0, cur_screen, 0 );

		TRACE( ("insertln\n") );
		}
		break;

	case IPO_MOVE:
		TRACE( ("move: %lu %lu\n", (u_long)ipb.val1, (u_long)ipb.val2) );

		/* advance the caret */
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

