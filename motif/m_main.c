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
static const char sccsid[] = "$Id: m_main.c,v 8.21 1996/12/10 17:07:21 bostic Exp $ (Berkeley) $Date: 1996/12/10 17:07:21 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <Xm/MainW.h>

#include <bitstring.h>
#include <ctype.h>
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

#if XtSpecificationRelease == 4
#define	ArgcType	Cardinal *
#else
#define	ArgcType	int *
#endif

int	i_fd, o_fd;				/* Input/output fd's. */

void	ip_siginit __P((void));
void	onchld __P((int));
void	onintr __P((int));


/*
 * Globals and costants
 */

/* our icon bitmap */
#include "nvi.xbm"

static	Pixmap		icon_pm;
static	Pixel		icon_fg,
			icon_bg;

static	Widget		top_level;
static	XtAppContext	ctx;



/* resources for the vi widgets unless the user overrides them */

String	fallback_rsrcs[] = {

    "*font:			-*-*-*-r-*--14-*-*-*-m-*-*-*",
    "*pointerShape:		xterm",
    "*busyShape:		watch",
    "*iconName:			vi",

    /* coloring for the icons */
    "*iconForeground:	XtDefaultForeground",
    "*iconBackground:	XtDefaultBackground",

    /* layout for the temporary preferences page */
    "*toggleOptions.numColumns: 6",
    "*intOptions.numColumns:    4",
    "*otherOptions.numColumns:  3",
    "*intOptions*columns:       10",
    "*otherOptions*columns:     18",

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
    running_cde = _vi_is_cde( d );
    XCloseDisplay(d);

    for ( i=0; i<XtNumber(fallback_rsrcs); i++ ) {

	/* stop here if running CDE */
	if ( fallback_rsrcs[i][0] == '?' ) {
	    if ( running_cde ) break;
	    if ((fallback_rsrcs[i] = strdup(fallback_rsrcs[i])) == NULL)
		NULL;
	    fallback_rsrcs[i][0] = '*';
	}

	copy[i] = malloc( strlen(name) + strlen(fallback_rsrcs[i]) + 1 );
	strcpy( copy[i], name );
	strcat( copy[i], fallback_rsrcs[i] );
    }

    copy[i] = NULL;
    return copy;
}


/* create the shell widgetry */

#if defined(__STDC__)
static	void	create_top_level_shell( int *argc, char **argv )
#else
static	void	create_top_level_shell( argc, argv )
	int	*argc;
	char	**argv;
#endif
{
    char	*ptr;
    Widget	main_w, menu_b, pane_w;
    Display	*display;

    /* X gets quite upset if the program name is not simple */
    if (( ptr = strrchr( argv[0], '/' )) != NULL ) argv[0] = ++ptr;
    vi_progname = argv[0];

    /* create a top-level shell for the window manager */
    top_level = XtVaAppInitialize( &ctx,
				   vi_progname,
				   NULL, 0,	/* options */
				   (ArgcType) argc,
				   argv,	/* might get modified */
				   get_fallback_rsrcs( argv[0] ),
				   NULL
				   );
    display = XtDisplay(top_level);

    /* might need to go technicolor... */
    XutInstallColormap( argv[0], top_level );

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

    /* in the shell, we will stack a menubar an editor */
    main_w = XtVaCreateManagedWidget( "main",
				      xmMainWindowWidgetClass,
				      top_level,
				      NULL
				      );

    /* create the menubar */
    menu_b = (Widget) vi_create_menubar( main_w );
    XtManageChild( menu_b );

    /* add the VI widget from the library */
    vi_create_editor( "editor", main_w );

    /* put it up */
    XtRealizeWidget( top_level );
}


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
	create_top_level_shell(&argc, argv);

	/* Run vi: the child returns. */
	(void)run_vi(argc, argv, &i_fd, &o_fd);

	/* Initialize signals. */
	ip_siginit();

	/* tell X that we are interested in input on the pipe */
	XtAppAddInput( ctx, i_fd, XtInputReadMask, vi_pipe_input_func, NULL );

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
    vi_fatal_message( top_level, "The VI process died.  Exiting" );
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
