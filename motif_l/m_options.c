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
static const char sccsid[] = "$Id: m_options.c,v 8.7 1996/12/14 09:04:12 bostic Exp $ (Berkeley) $Date: 1996/12/14 09:04:12 $";
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

#include "m_motif.h"
#include "m_extern.h"

/*
 * Types
 */
typedef enum {
	optToggle,
	optInteger,
	optString,
	optFile,
	optTerminator
} optKind;

typedef struct {
	optKind	kind;
	String	name;
	void	*value;		/* really should get this from core */
} optData;

typedef	struct {
	String	name;
	Widget	holder;
	optData	*toggles;
	optData	*ints;
	optData	*others;
} optSheet;


/* constants */

#if defined(SelfTest)

/* in production, get these from the resource list */

#define	toggleColumns	6

#endif

#define	LargestSheet	0	/* file */


/*
 * global data
 */

static	Widget	preferences = NULL;

static	optData	Search_toggles[] = {
	{ optToggle,	"iclower",	(void *) False	},
	{ optToggle,	"searchincr",	(void *) True	},
	{ optToggle,	"wrapscan",	(void *) True	},
	{ optToggle,	"extended",	(void *) False	},
	{ optToggle,	"edcompatible",	(void *) False	},
	{ optToggle,	"ignorecase",	(void *) True	},
	{ optToggle,	"magic",	(void *) True	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		File_toggles[] = {
	{ optToggle,	"readonly",	(void *) False	},
	{ optToggle,	"autowrite",	(void *) False	},
	{ optToggle,	"warn",		(void *) True	},
	{ optToggle,	"lock",		(void *) True	},
	{ optToggle,	"writeany",	(void *) False	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Shell_toggles[] = {
	{ optToggle,	"secure",	(void *) False	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Ex_toggles[] = {
	{ optToggle,	"autoprint",	(void *) True	},
	{ optToggle,	"exrc",		(void *) False	},
	{ optToggle,	"prompt",	(void *) True	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Programming_toggles[] = {
	{ optToggle,	"lisp",		(void *) False	},
	{ optToggle,	"autoindent",	(void *) True	},
	{ optToggle,	"showmatch",	(void *) False	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Display_toggles[] = {
	{ optToggle,	"list",		(void *) False	},
	{ optToggle,	"number",	(void *) False	},
	{ optToggle,	"octal",	(void *) False	},
	{ optToggle,	"showmode",	(void *) False	},
	{ optToggle,	"comment",	(void *) False	},
	{ optToggle,	"windowname",	(void *) False	},
	{ optToggle,	"leftright",	(void *) False	},
	{ optToggle,	"ruler",	(void *) False	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Insert_toggles[] = {
	{ optToggle,	"altwerase",	(void *) False	},
	{ optToggle,	"ttywerase",	(void *) False	},
	{ optToggle,	"timeout",	(void *) True	},
	{ optToggle,	"beautify",	(void *) False	},
	{ optToggle,	"remap",	(void *) True	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Command_toggles[] = {
	{ optToggle,	"tildeop",	(void *) False	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Error_toggles[] = {
	{ optToggle,	"verbose",	(void *) False	},
	{ optToggle,	"errorbells",	(void *) False	},
	{ optToggle,	"flash",	(void *) True	},
	{ optTerminator,NULL,		(void *) NULL	}
};

static	optData Programming_ints[] = {
	{ optInteger,	"matchtime",	(void *) "7"	},
	{ optInteger,	"shiftwidth",	(void *) "4"	},
	{ optInteger,	"taglength",	(void *) "0"	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Display_ints[] = {
	{ optInteger,	"report",	(void *) "5"	},
	{ optInteger,	"tabstop",	(void *) "8"	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Insert_ints[] = {
	{ optInteger,	"wrapmargin",	(void *) "0"	},
	{ optInteger,	"escapetime",	(void *) "1"	},
	{ optInteger,	"wraplen",	(void *) "0"	},
	{ optTerminator,NULL,		(void *) NULL	}
};

static	optData	Search_others[] = {
	{ optString,	"paragraphs",	(void *) "IPLPPPQPP LIpplpipbp"	},
	{ optString,	"sections",	(void *) "NHSHH HUnhsh"	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		File_others[] = {
	{ optString,	"filec",	(void *) "^["	},
	{ optString,	"path",		(void *) "src:include:/debugger/src:/debugger/include/"	},
	{ optFile,	"recdir",	(void *) "/var/tmp/vi.recover"	},
	{ optFile,	"directory",	(void *) "/tmp"	},
	{ optString,	"backup",	(void *) ""	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Shell_others[] = {
	{ optString,	"cdpath",	(void *) ":"	},
	{ optFile,	"shell",	(void *) "/bin/csh"	},
	{ optString,	"shellmeta",	(void *) "~{[*?$`'\"\\"	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Ex_others[] = {
	{ optString,	"cedit",	(void *) ""	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Programming_others[] = {
	{ optString,	"tags",		(void *) "tags /debugger/tmp/tags10 /debugger/tags /debugger/lib/tags"	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Display_others[] = {
	{ optString,	"print",	(void *) ""	},
	{ optString,	"noprint",	(void *) ""	},
	{ optTerminator,NULL,		(void *) NULL	}
},
		Error_others[] = {
	{ optString,	"msgcat",	(void *) "./"	},
	{ optTerminator,NULL,		(void *) NULL	}
};

static	optSheet sheets[] = {
	{	"File",	/* must be first because it's the largest */
		NULL,
		File_toggles,
		NULL,
		File_others
	},
	{	"Search",
		NULL,
		Search_toggles,
		NULL,
		Search_others
	},
	{	"Shell",
		NULL,
		Shell_toggles,
		NULL,
		Shell_others
	},
	{	"Ex",
		NULL,
		Ex_toggles,
		NULL,
		Ex_others
	},
	{	"Programming",
		NULL,
		Programming_toggles,
		Programming_ints,
		Programming_others
	},
	{	"Display",
		NULL,
		Display_toggles,
		Display_ints,
		Display_others
	},
	{	"Insert",
		NULL,
		Insert_toggles,
		Insert_ints,
		NULL
	},
	{	"Command",
		NULL,
		Command_toggles,
		NULL,
		NULL
	},
	{	"Error",
		NULL,
		Error_toggles,
		NULL,
		Error_others
	},
};


/* callbacks */

#if defined(SelfTest)
void __vi_cancel_cb()
{
    puts( "cancelled" );
}
#endif


static	void destroyed()
{
    int i;

    puts( "destroyed" );

    /* some window managers destroy us upon popdown */
    for (i=0; i<XtNumber(sheets); i++) {
	sheets[i].holder = NULL;
    }
    preferences = NULL;
}


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
    option->value = (void *) set;

#if 1
    if ( strcmp( option->name, "ruler" ) == 0 )
	if ( set )
	    __vi_show_text_ruler_dialog( __vi_screen->area, "Ruler" );
	else
	    __vi_clear_text_ruler_dialog();
#endif

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

    /* Note memory leak.  We should free the old string, but that
     * would require another bit to let us know if it had been allocated.
     */
    option->value = (void *) str;

#if defined(SelfTest)
    printf( "sending command <<%s>>\n", buffer );
#else
    __vi_send_command_string( buffer );
#endif
}


/* add toggles to the property sheet */

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


static	Widget	create_toggles( outer, toggles )
	Widget	outer;
	optData	*toggles;
{
    Widget	inner;
    int		i;

    inner = XtVaCreateWidget( "toggleOptions",
			      xmRowColumnWidgetClass,
			      outer,
			      XmNpacking,		XmPACK_COLUMN,
#if defined(SelfTest)
			      XmNnumColumns,		toggleColumns,
#endif
			      XmNtopAttachment,		XmATTACH_FORM,
			      XmNrightAttachment,	XmATTACH_FORM,
			      XmNleftAttachment,	XmATTACH_FORM,
			      0
			      );

    /* first the booleans */
    for (i=0; toggles[i].kind != optTerminator; i++) {
	add_toggle( inner, &toggles[i] );
    }
    XtManageChild( inner );

    return inner;
}


/* draw text fields and their labels */

#if defined(__STDC__)
static	void	add_string_options( Widget parent,
				    optData *options
				    )
#else
static	void	add_string_options( parent, options )
	Widget	parent;
	optData	*options;
#endif
{
    int		i;
    Widget	f, w, l;

    for ( i=0; options[i].kind != optTerminator; i++ ) {

	f = XtVaCreateWidget( "form",
			      xmFormWidgetClass,
			      parent,
			      0
			      );

	XtVaCreateManagedWidget( options[i].name,
				 xmLabelGadgetClass,
				 f,
				 XmNtopAttachment,	XmATTACH_FORM,
				 XmNbottomAttachment,	XmATTACH_FORM,
				 XmNleftAttachment,	XmATTACH_FORM,
				 XmNrightAttachment,	XmATTACH_POSITION,
				 XmNrightPosition,	20,
				 XmNalignment,		XmALIGNMENT_END,
				 0
				 );

	w = XtVaCreateManagedWidget( "text",
				     xmTextFieldWidgetClass,
				     f,
				     XmNtopAttachment,		XmATTACH_FORM,
				     XmNbottomAttachment,	XmATTACH_FORM,
				     XmNrightAttachment,	XmATTACH_FORM,
				     XmNleftAttachment,		XmATTACH_POSITION,
				     XmNleftPosition,		20,
				     0
				     );

	XmTextFieldSetString( w, (char *) options[i].value );
	XtAddCallback( w, XmNactivateCallback, change_string, &options[i] );
	XtManageChild( f );
    }
}


/* draw and display a single page of properties */

#if defined(__STDC__)
static	Widget		create_sheet( Widget parent, optSheet *sheet )
#else
static	Widget		create_sheet( parent, sheet )
	Widget		parent;
	optSheet	*sheet;
#endif
{
    Widget	outer, inner;
    int		i;
    Dimension	height;

    outer = XtVaCreateWidget( sheet->name,
			      xmFormWidgetClass,
			      parent,
			      XmNtopAttachment,		XmATTACH_FORM,
			      XmNrightAttachment,	XmATTACH_FORM,
			      XmNbottomAttachment,	XmATTACH_FORM,
			      XmNleftAttachment,	XmATTACH_FORM,
			      XmNshadowType,		XmSHADOW_ETCHED_IN,
			      XmNshadowThickness,	2,
			      XmNverticalSpacing,	4,
			      XmNhorizontalSpacing,	4,
			      0
			      );

    /* add the toggles */
    inner = create_toggles( outer, sheet->toggles );

    inner = XtVaCreateWidget( "otherOptions",
			      xmRowColumnWidgetClass,
			      outer,
			      XmNpacking,		XmPACK_COLUMN,
			      XmNtopAttachment,		XmATTACH_WIDGET,
			      XmNtopWidget,		inner,
			      XmNrightAttachment,	XmATTACH_FORM,
			      XmNbottomAttachment,	XmATTACH_FORM,
			      XmNleftAttachment,	XmATTACH_FORM,
			      0
			      );

    /* then the ints */
    if ( sheet->ints != NULL ) {
	add_string_options( inner, sheet->ints );
    }

    /* then the rest */
    if ( sheet->others != NULL ) {
	add_string_options( inner, sheet->others );
    }
    XtManageChild( inner );

    /* finally, force resize of the parent */
    XtVaGetValues( outer, XmNheight, &height, 0 );
    XtVaSetValues( parent, XmNheight, height, 0 );

    return outer;
}


/* change preferences to another sheet */

static	void	change_sheet( parent, current )
	Widget	parent;
	int	current;
{
    static int		current_sheet = -1;

    /* create a new one? */
    if ( sheets[current].holder == NULL )
	sheets[current].holder = create_sheet( parent, &sheets[current] );

    /* done with the old one? */
    if ( current_sheet != -1 && sheets[current_sheet].holder != NULL )
	XtUnmanageChild( sheets[current_sheet].holder );

    current_sheet = current;
    XtManageChild( sheets[current].holder );
    XtManageChild( parent );
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
    Widget	box, form, inner;
    int		i;
    char	buffer[1024];

    /* already built? */
    if ( preferences != NULL ) return preferences;

    box = XtVaCreatePopupShell( title,
				xmDialogShellWidgetClass,
				parent,
				XmNtitle,		title,
				XmNallowShellResize,	False,
				0
				);
    XtAddCallback( box, XmNpopdownCallback, __vi_cancel_cb, 0 );
    XtAddCallback( box, XmNdestroyCallback, destroyed, 0 );

    form = XtVaCreateWidget( "options", 
			     xmFormWidgetClass,
			     box,
			     0
			     );

    /* copy the pointers to the sheet names */
    *buffer = '\0';
    for (i=0; i<XtNumber(sheets); i++) {
	strcat( buffer, "|" );
	strcat( buffer, sheets[i].name );
    }

    inner = __vi_CreateTabbedFolder( "tabs",
				    form,
				    buffer,
				    XtNumber(sheets),
				    change_sheet
				    );

    /* build the property sheets early */
    for ( i=0; i<XtNumber(sheets); i++ )
	change_sheet( inner, i );
    change_sheet( inner, LargestSheet );

    /* keep this global, we might destroy it later */
    preferences = form;

    /* done */
    return form;
}



/*
 * module entry point
 *
 * __vi_show_options_dialog --
 *
 *
 * PUBLIC: void __vi_show_options_dialog __P((Widget, String));
 */
void
__vi_show_options_dialog(parent, title)
	Widget parent;
	String title;
{
    Widget 	db = create_options_dialog( parent, title ),
		shell = XtParent(db);

    XtManageChild( db );

#if defined(SelfTest)
    /* wait until it goes away */
    XtPopup( shell, XtGrabNone );
#else
    /* wait until it goes away */
    __vi_modal_dialog( db );
#endif
}



/* module entry point
 * Utilities for the search dialog
 *
 *	Boolean	__vi_incremental_search()
 *	returns the value of searchincr
 *
 *	Widget	__vi_create_search_toggles( parent )
 *	creates the search toggles.  this is so the options and
 *	search widgets share their appearance
 */

Boolean	__vi_incremental_search()
{
	return (Boolean) Search_toggles[1].value;
}


Widget	__vi_create_search_toggles( parent )
{
    return  create_toggles( parent, Search_toggles );
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
