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
static const char sccsid[] = "$Id: m_options.c,v 8.3 1996/12/10 21:07:55 bostic Exp $ (Berkeley) $Date: 1996/12/10 21:07:55 $";
#endif /* not lint */

#include <X11/X.h>
#include <X11/Intrinsic.h>
#include <Xm/DialogS.h>
#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/PushBG.h>
#include <Xm/TextF.h>
#include <Xm/ToggleBG.h>
#include <Xm/RowColumn.h>

#include "ipc_motif.h"
#include "../include/ipc_mextern.h"

/*
 * Types
 */
typedef enum {
	optToggle,
	optInteger,
	optString,
	optFile
} optKind;

typedef struct {
	optKind	kind;
	String	name;
	void	*value;		/* really should get this from core */
} optData;


/* constants */

#if defined(SelfTest)

/* in production, get these from the resource list */

#define	toggleColumns	6
#define	intColumns	4
#define	otherColumns	3

#endif

/* in production, get these from the resource list */

#define	intWidth	10	/* characters */
#define	otherWidth	18	/* characters */


/*
 * global data
 */

static	optData	toggles[] = {
	{ optToggle,	"altwerase",	(void *) False	},
	{ optToggle,	"extended",	(void *) False	},
	{ optToggle,	"mesg",		(void *) True	},
	{ optToggle,	"ruler",	(void *) False	},
	{ optToggle,	"tildeop",	(void *) False	},
	{ optToggle,	"autoindent",	(void *) True	},
	{ optToggle,	"modeline",	(void *) False	},
	{ optToggle,	"timeout",	(void *) True	},
	{ optToggle,	"autoprint",	(void *) True	},
	{ optToggle,	"flash",	(void *) True	},
	{ optToggle,	"searchincr",	(void *) True	},
	{ optToggle,	"ttywerase",	(void *) False	},
	{ optToggle,	"autowrite",	(void *) False	},
	{ optToggle,	"secure",	(void *) False	},
	{ optToggle,	"verbose",	(void *) False	},
	{ optToggle,	"iclower",	(void *) False	},
	{ optToggle,	"number",	(void *) False	},
	{ optToggle,	"warn",		(void *) True	},
	{ optToggle,	"beautify",	(void *) False	},
	{ optToggle,	"ignorecase",	(void *) True	},
	{ optToggle,	"octal",	(void *) False	},
	{ optToggle,	"showmatch",	(void *) False	},
	{ optToggle,	"open",		(void *) True	},
	{ optToggle,	"showmode",	(void *) False	},
	{ optToggle,	"windowname",	(void *) False	},
	{ optToggle,	"leftright",	(void *) False	},
	{ optToggle,	"optimize",	(void *) True	},
	{ optToggle,	"slowopen",	(void *) False	},
	{ optToggle,	"comment",	(void *) False	},
	{ optToggle,	"lisp",		(void *) False	},
	{ optToggle,	"prompt",	(void *) True	},
	{ optToggle,	"sourceany",	(void *) False	},
	{ optToggle,	"wrapscan",	(void *) True	},
	{ optToggle,	"edcompatible",	(void *) False	},
	{ optToggle,	"list",		(void *) False	},
	{ optToggle,	"readonly",	(void *) False	},
	{ optToggle,	"writeany",	(void *) False	},
	{ optToggle,	"lock",		(void *) True	},
	{ optToggle,	"redraw",	(void *) False	},
	{ optToggle,	"errorbells",	(void *) False	},
	{ optToggle,	"magic",	(void *) True	},
	{ optToggle,	"remap",	(void *) True	},
	{ optToggle,	"exrc",		(void *) False	},
	{ optToggle,	"terse",	(void *) False	},
};

static	optData	ints[] = {
	{ optInteger,	"scroll",	(void *) "11"	},
	{ optInteger,	"hardtabs",	(void *) "0"	},
	{ optInteger,	"shiftwidth",	(void *) "4"	},
	{ optInteger,	"window",	(void *) "23"	},
	{ optInteger,	"keytime",	(void *) "6"	},
	{ optInteger,	"sidescroll",	(void *) "16"	},
	{ optInteger,	"wraplen",	(void *) "0"	},
	{ optInteger,	"columns",	(void *) "80"	},
	{ optInteger,	"lines",	(void *) "24"	},
	{ optInteger,	"wrapmargin",	(void *) "0"	},
	{ optInteger,	"tabstop",	(void *) "8"	},
	{ optInteger,	"escapetime",	(void *) "1"	},
	{ optInteger,	"taglength",	(void *) "0"	},
	{ optInteger,	"matchtime",	(void *) "7"	},
	{ optInteger,	"report",	(void *) "5"	},
};

static	optData	others[] = {
	{ optString,	"filec",	(void *) "^["	},
	{ optString,	"msgcat",	(void *) "./"	},
	{ optString,	"noprint",	(void *) ""	},
	{ optString,	"backup",	(void *) ""	},
	{ optString,	"cdpath",	(void *) ":"	},
	{ optString,	"cedit",	(void *) ""	},
	{ optString,	"print",	(void *) ""	},
	{ optString,	"term",		(void *) "motif"	},
	{ optFile,	"directory",	(void *) "/tmp"	},
	{ optString,	"paragraphs",	(void *) "IPLPPPQPP LIpplpipbp"	},
	{ optString,	"path",		(void *) "src:include:/debugger/src:/debugger/include/"	},
	{ optFile,	"recdir",	(void *) "/var/tmp/vi.recover"	},
	{ optString,	"sections",	(void *) "NHSHH HUnhsh"	},
	{ optFile,	"shell",	(void *) "/bin/csh"	},
	{ optString,	"shellmeta",	(void *) "~{[*?$`'\"\\"	},
	{ optString,	"tags",		(void *) "tags /debugger/tmp/tags10 /debugger/tags /debugger/lib/tags"	},
};


/* callbacks */

#if defined(__STDC__)
static	void	change_toggle( Widget w, optData *option )
#else
static	void	change_toggle( w, option )
	Widget	w;
	optData	*option;
#endif
{
    char	buffer[1024];
    Boolean	set;

    XtVaGetValues( w, XmNset, &set, 0 );

    sprintf( buffer, ":set %s%s", (set) ? "" : "no", option->name );

#if defined(SelfTest)
    printf( "sending command <<%s>>\n", buffer );
#else
    __vi_send_command_string( buffer );
#endif
}


#if defined(__STDC__)
static	void	change_string( Widget w, optData *option )
#else
static	void	change_string( w, option )
	Widget	w;
	optData	*option;
#endif
{
    char	buffer[1024];
    String	str;

    str = XmTextFieldGetString( w );
    sprintf( buffer, ":set %s=%s", option->name, str );

#if defined(SelfTest)
    printf( "sending command <<%s>>\n", buffer );
#else
    __vi_send_command_string( buffer );
#endif
}


/* add a control to the property sheet */

#if defined(__STDC__)
static	void	add_toggle( Widget parent, optData *option )
#else
static	void	add_toggle( parent, option )
	Widget	parent;
	optData	*option;
#endif
{
    Widget	w;

    w = XtVaCreateManagedWidget( option->name,
				 xmToggleButtonGadgetClass,
				 parent,
				 XmNset,	(Boolean) option->value,
				 0
				 );
    XtAddCallback( w, XmNvalueChangedCallback, change_toggle, option );
}


/* draw a matrix of text fields and their labels
 * Note that rowcolumns widgets are column major, so we
 * need to go through this nonsense rather than just adding the
 * darn things in order
 */

#define	numRows(cols,count)	((2*(count)+(cols)-1)/(cols))

#if defined(__STDC__)
static	void	add_string_options( Widget parent,
				    optData *options,
				    int count,
				    int width
				    )
#else
static	void	add_string_options( parent, options, count, width )
	Widget	parent;
	optData	*options;
	int	count;
	int	width;
#endif
{
    int	i;
    Widget	f, w;

    for ( i=0; i<count; i++ ) {
	f = XtVaCreateWidget( "form",
			      xmFormWidgetClass,
			      parent,
			      0
			      );
	w = XtVaCreateManagedWidget( options[i].name,
				     xmTextFieldWidgetClass,
				     f,
#if defined(SelfTest)
				     XmNcolumns,	width,
#endif
				     XmNtopAttachment,	XmATTACH_FORM,
				     XmNbottomAttachment,XmATTACH_FORM,
				     XmNrightAttachment,XmATTACH_FORM,
				     0
				     );
	XtVaCreateManagedWidget( options[i].name,
				 xmLabelGadgetClass,
				 f,
				 XmNtopAttachment,	XmATTACH_FORM,
				 XmNbottomAttachment,	XmATTACH_FORM,
				 XmNleftAttachment,	XmATTACH_FORM,
				 XmNrightAttachment,	XmATTACH_WIDGET,
				 XmNrightWidget,	w,
				 XmNalignment,		XmALIGNMENT_END,
				 0
				 );
	XmTextFieldSetString( w, (char *) options[i].value );
	XtAddCallback( w, XmNactivateCallback, change_string, &options[i] );
	XtManageChild( f );
    }
}



/* Draw and display a dialog the describes nvi options */

#if defined(__STDC__)
static	Widget	create_options_dialog( Widget parent, String title )
#else
static	Widget	create_options_dialog( parent, title )
	Widget	parent;
	String	title;
#endif
{
    Widget	box, form, form2;
    int		i;

    box = XtVaCreatePopupShell( title,
				xmDialogShellWidgetClass,
				parent,
				XmNtitle,		title,
				XmNallowShellResize,	False,
				0
				);
    XtAddCallback( box, XmNpopdownCallback, __vi_cancel_cb, 0 );

    form = XtVaCreateWidget( "form", 
			     xmRowColumnWidgetClass,
			     box,
			     0
			     );

    form2 = XtVaCreateWidget( "toggleOptions", 
			      xmRowColumnWidgetClass,
			      form,
			      XmNpacking,	XmPACK_COLUMN,
#if defined(SelfTest)
			      XmNnumColumns,	toggleColumns,
#endif
			      0
			      );

    for (i=0; i<XtNumber(toggles); i++) {
	add_toggle( form2, &toggles[i] );
    }
    XtManageChild( form2 );

    form2 = XtVaCreateWidget( "intOptions", 
			      xmRowColumnWidgetClass,
			      form,
			      XmNpacking,	XmPACK_COLUMN,
#if defined(SelfTest)
			      XmNnumColumns,	intColumns,
#endif
			      0
			      );
    add_string_options( form2, ints, XtNumber(ints), intWidth );
    XtManageChild( form2 );

    form2 = XtVaCreateWidget( "otherOptions", 
			      xmRowColumnWidgetClass,
			      form,
			      XmNpacking,	XmPACK_COLUMN,
#if defined(SelfTest)
			      XmNnumColumns,	otherColumns,
#endif
			      0
			      );
    add_string_options( form2, others, XtNumber(others), otherWidth );
    XtManageChild( form2 );

    return form;
}



/* module entry point
 *	__vi_show_options_dialog( parent, title )
 */

#if defined(__STDC__)
void	__vi_show_options_dialog( Widget parent, String title )
#else
void	__vi_show_options_dialog( parent, title )
Widget	parent;
String	title;
#endif
{
    Widget 	db = create_options_dialog( parent, title ),
		shell = XtParent(db);
    Dimension	height, width;

    /* this one does not resize */
    XtVaGetValues( shell, XmNheight, &height, XmNwidth, &width, 0 );
    XtVaSetValues( shell, XmNminHeight, height, XmNminWidth, width,
			  XmNmaxHeight, height, XmNmaxWidth, width,
			  0 );

#if defined(SelfTest)
    /* wait until it goes away */
    XtPopup( shell, XtGrabNone );
#else
    /* wait until it goes away */
    __vi_modal_dialog( db );
#endif
}



#if defined(SelfTest)

#if defined(__STDC__)
static void show_options( Widget w, XtPointer data, XtPointer cbs )
#else
static void show_options( w, data, cbs )
Widget w;
XtPointer	data;
XtPointer	cbs;
#endif
{
    __vi_show_options_dialog( data, "Preferences" );
}

main( int argc, char *argv[] )
{
    XtAppContext	ctx;
    Widget		top_level, rc, button;
    extern		exit();

    /* create a top-level shell for the window manager */
    top_level = XtVaAppInitialize( &ctx,
				   argv[0],
				   NULL, 0,	/* options */
				   (ArgcType) &argc,
				   argv,	/* might get modified */
				   NULL,
				   NULL
				   );

    rc = XtVaCreateManagedWidget( "rc",
				  xmRowColumnWidgetClass,
				  top_level,
				  0
				  );

    button = XtVaCreateManagedWidget( "Pop up options dialog",
				      xmPushButtonGadgetClass,
				      rc,
				      0
				      );
    XtAddCallback( button, XmNactivateCallback, show_options, rc );

    button = XtVaCreateManagedWidget( "Quit",
				      xmPushButtonGadgetClass,
				      rc,
				      0
				      );
    XtAddCallback( button, XmNactivateCallback, exit, 0 );

    XtRealizeWidget(top_level);
    XtAppMainLoop(ctx);
}
#endif
