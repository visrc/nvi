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
static const char sccsid[] = "$Id: m_util.c,v 8.6 1996/12/03 10:12:47 bostic Exp $ (Berkeley) $Date: 1996/12/03 10:12:47 $";
#endif /* not lint */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>

#include <stdio.h>
#include <string.h>

#include "ipc_mutil.h"

static Colormap	cmap = 0;
static Display  *cmap_display = 0;


/* ***********************************************************************
 * Useful macros
 * ***********************************************************************
 */

#define	Screen		DefaultScreen( display )
#define	Black		BlackPixel( display, Screen )
#define	White		WhitePixel( display, Screen )

#define	get_current_colormap()	\
	    (cmap) ? cmap : DefaultColormap( display, Screen )


/* Widget hierarchy routines
 *
 * void XutShowWidgetTree( FILE *fp, Widget root, int indent )
 *	prints the widgets and sub-widgets beneath the named root widget
 */


#if defined(__STDC__)
void	XutShowWidgetTree( FILE *fp, Widget root, int indent )
#else
void	XutShowWidgetTree( fp, root, indent )
FILE	*fp;
Widget	root;
int	indent;
#endif
{
#if ! defined(DECWINDOWS)
    WidgetList	l, l2;
    int		i, count = 0;

    /* print where we are right now */
    fprintf( fp,
	     "%*.*swidget => 0x%x name => \"%s\"\n\r",
	     indent,
	     indent,
	     "",
	     root,
	     XtName(root) );

    /* get the correct widget values */
    XtVaGetValues( root, XtNchildren, &l, 0 );
    XtVaGetValues( root, XtNchildren, &l2, 0 );
    XtVaGetValues( root, XtNnumChildren, &count, 0 );

    /* print the sub-widgets */
    for ( i=0; i<count; i++ ) {
	XutShowWidgetTree( fp, l[i], indent+4 );
    }

    /* tidy up if this thing contained children */
    if ( count > 0 ) {
	/* we may or may not have to free the children */
	if ( l != l2 ) {
	    XtFree( (char *) l );
	    XtFree( (char *) l2 );
	}
    }
#endif
}



/* Color support utilities
 *
 * Pixel XutGetContrastingPixel( Display *display, Pixel pix )
 *	select a Black or White to contrast with the specified color
 *
 * Bool	XutStringToColor( Widget, String, Pixel * )
 *	given a String (either a name or RBG in hex) returns a
 *	pixel which matches the String.
 */

#define MaxIntensity	65535

#if defined(__STDC__)
Bool	XutStringToColor( Widget w, String name, Pixel *pixel_value )
#else
Bool	XutStringToColor( w, name, pixel_value )
Widget	w;
String	name;
Pixel	*pixel_value;
#endif
{
    XrmValue	fromVal, toVal;

    fromVal.addr = name;
    fromVal.size = strlen(name);
    XtConvert( w, XtRString, &fromVal, XtRPixel, &toVal );

    if ( ! toVal.addr ) return False;

    *pixel_value = *((Pixel *)toVal.addr);
    return True;
}


#if defined(__STDC__)
void	XutGetColorComponents( Display *display, Pixel pix, XColor *col )
#else
void	XutGetColorComponents( display, pix, col )
Display	*display;
Pixel	pix;
XColor	*col;
#endif
{
    col->pixel = pix;
    XQueryColor( display, get_current_colormap(), col );
}


#if defined(__STDC__)
Pixel	XutGetContrastingPixel( Display *display, Pixel pix )
#else
Pixel	XutGetContrastingPixel( display, pix )
Display	*display;
Pixel	pix;
#endif
{
    XColor	col;
    int		intensity = 0, possible = 0;
    Visual	*vis = DefaultVisual( display, Screen );

    /* for b&w displays, invert the pixel value */
    if ( DisplayPlanes( display, Screen ) == 1 ) return ! pix;

    /* who knows? */
    if ( vis == NULL ) return Black;

    /* what are the RGB components of the color? */
    XutGetColorComponents( display, pix, &col );

    if ( col.flags & DoGreen ) {
	intensity += col.green;
	possible += MaxIntensity;
	}
    /* if a gray-scale monitor, ignore the unhooked wires */
    if ( vis->class != GrayScale && vis->class != StaticGray ) {
	if ( col.flags & DoRed ) {
	    intensity += col.red;
	    possible += MaxIntensity;
	    }
	if ( col.flags & DoBlue ) {
	    intensity += col.blue;
	    possible += MaxIntensity;
	    }
	}

    /* use black for the contrast unless the background is more than 66% dark. */
    return  ( intensity < 0.33 * possible ) ? White : Black;
}


/* Utilities for converting X resources...
 *
 * XutConvertResources( Widget, String root, XutResource *, int count )
 *	The resource block is loaded with converted values
 *	If the X resource does not exist, no change is made to the value
 *	'root' should be the application name.
 */

#if defined(__STDC__)
void		XutConvertResources( Widget wid,
				     String root,
				     XutResource *resources,
				     int count
				     )
#else
void		XutConvertResources( wid, root, resources, count )
Widget		wid;
String		root;
XutResource	*resources;
int		count;
#endif
{
    int		i;
    XrmValue	from, to;
    String	kind;
    Boolean	success = True;

    /* for each resource */
    for (i=0; i<count; i++) {

	/* is there a value in the database? */
	from.addr = XGetDefault( XtDisplay(wid), root, resources[i].name );
	if ( from.addr == NULL || *from.addr == '\0' )
	    continue;

	/* load common parameters */
	from.size = strlen( from.addr );
	to.addr   = resources[i].value;

	/* load type-specific parameters */
	switch ( resources[i].kind ) {
	    case XutRKinteger:
		to.size	= sizeof(int);
		kind	= XtRInt;
		break;

	    case XutRKboolean:
		/* String to Boolean */
		to.size	= sizeof(Boolean);
		kind	= XtRBoolean;
		break;

	    case XutRKfont:
		/* String to Font structure */
		to.size	= sizeof(XFontStruct *);
		kind	= XtRFontStruct;
		break;

	    case XutRKpixelBackup:
		/* String to Pixel backup algorithm */
		if ( success ) continue;
		/* FALL through */

	    case XutRKpixel:
		/* String to Pixel */
		to.size	= sizeof(Pixel);
		kind	= XtRPixel;
		break;

	    case XutRKcursor:
		/* String to Cursor */
		to.size	= sizeof(int);
		kind	= XtRCursor;
		break;

	    default:
		return;
	}

	/* call the converter */
	success = XtConvertAndStore( wid, XtRString, &from, kind, &to );
    }
}


#if defined(__STDC__)
void	XutSetIcon( Widget wid, int height, int width, Pixmap p )
#else
void	XutSetIcon( wid, height, width, p )
Widget	wid;
int	height, width;
Pixmap	p;
#endif
{
    Display	*display = XtDisplay(wid);
    Window	win;

    /* best bet is to set the icon window */
    XtVaGetValues( wid, XtNiconWindow, &win, 0 );

    if ( win == None ) {
	win = XCreateSimpleWindow( display,
				   RootWindow( display, Screen ),
				   0, 0,
				   width, height,
				   0,
				   CopyFromParent,
				   CopyFromParent
				   );
    }

    if ( win != None ) {
	XtVaSetValues( wid, XtNiconWindow, win, 0 );
	XSetWindowBackgroundPixmap( display, win, p );
    }

    else {
	/* do it the old fashioned way */
	XtVaSetValues( wid, XtNiconPixmap, p, 0 );
    }
}


/* Support for multiple colormaps
 *
 * XutInstallColormap( String name, Widget wid )
 *	The first time called, this routine checks to see if the
 *	resource "name*installColormap" is "True".  If so, the
 *	widget is assigned a newly allocated colormap.
 *
 *	Subsequent calls ignore the "name" parameter and use the
 *	same colormap.
 *
 *	Future versions of this routine may handle multiple colormaps
 *	by name.
 */

static	enum { cmap_look, cmap_use, cmap_ignore } cmap_state = cmap_look;

static	Boolean	use_colormap = False;

XutResource	colormap_resources[] = {
    { "installColormap",	XutRKboolean,	&use_colormap	}
};

#if defined(__STDC__)
void	XutInstallColormap( String name, Widget wid )
#else
void	XutInstallColormap( name, wid )
String	name;
Widget	wid;
#endif
{
    Display	*display = XtDisplay(wid);

    /* what is the current finite state? */
    if ( cmap_state == cmap_look ) {

	/* what does the resource say? */
	XutConvertResources( wid,
			     name,
			     colormap_resources,
			     XtNumber(colormap_resources)
			     );

	/* was the result "True"? */
	if ( ! use_colormap ) {
	    cmap_state = cmap_ignore;
	    return;
	}

	/* yes it was */
	cmap_state = cmap_use;
	cmap_display = display;
	cmap = XCopyColormapAndFree( display,
				     DefaultColormap( display,
						      DefaultScreen( display )
						      )
				     );
    }

    /* use the private colormap? */
    if ( cmap_state == cmap_use ) {
	XtVaSetValues( wid, XtNcolormap, cmap, 0 );
    }
}


#if defined(__STDC__)
void	XutFreeColormap( String name )
#else
void	XutFreeColormap( name )
String	name;
#endif
{
    if ( cmap_state == cmap_use ) {
	XFreeColormap( cmap_display, cmap );
	cmap = 0;
	cmap_state = cmap_look;
    }
}


#if defined(__STDC__)
Colormap	XutGetColormap( Widget wid, String name )
#else
Colormap	XutGetColormap( wid, name )
Widget		wid;
String		name;
#endif
{
    Display *display = XtDisplay( wid );

    return get_current_colormap();
}
