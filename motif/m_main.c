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
static const char sccsid[] = "$Id: m_main.c,v 8.22 1996/12/10 21:05:08 bostic Exp $ (Berkeley) $Date: 1996/12/10 21:05:08 $";
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
#include "ipc_mextern.h"
#include "pathnames.h"

#include "nvi.xbm"		/* Icon bitmap. */

int vi_ofd;			/* GLOBAL: known to vi_pipe_input_func(). */

static	pid_t		pid;
static	Pixel		icon_fg,
			icon_bg;
static	Pixmap		icon_pm;
static	Widget		top_level;
static	XtAppContext	ctx;

static void onchld __P((int));


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

static XutResource resource[] = {
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
    running_cde = __vi_is_cde( d );
    XCloseDisplay(d);

    for ( i=0; i<XtNumber(fallback_rsrcs); i++ ) {

	/* stop here if running CDE */
	if ( fallback_rsrcs[i][0] == '?' ) {
	    if ( running_cde ) break;
	    (void)fallback_rsrcs[i] = strdup(fallback_rsrcs[i]);
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
    Widget	main_w, menu_b;
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
	int i_fd;

	/*
	 * Initialize the X widgetry.  We must do this before picking off
	 * arguments as well-behaved X programs have common argument lists
	 * (e.g. -rv for reverse video).
	 */
	create_top_level_shell(&argc, argv);

	/* We need to know if the child process dies horribly. */
	(void)signal(SIGCHLD, onchld);

	/* Run vi: the parent returns, the child is the vi process. */
	(void)vi_run(argc, argv, &i_fd, &vi_ofd, &pid);

	/* Tell X that we are interested in input on the pipe. */
	XtAppAddInput(ctx, i_fd,
	    (XtPointer)XtInputReadMask, vi_pipe_input_func, NULL);

	/* Main loop. */
	XtAppMainLoop(ctx);

	/* NOTREACHED */
	abort();
}

/*
 * onchld --
 *	Handle SIGCHLD.
 */
static void
onchld(signo)
	int signo;
{
	/* If the vi process goes away, we exit as well. */
	if (kill(pid, 0))
		vi_fatal_message(top_level, "The vi process died.  Exiting.");
}
