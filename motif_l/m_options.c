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
static const char sccsid[] = "$Id: m_options.c,v 8.8 1996/12/14 14:04:39 bostic Exp $ (Berkeley) $Date: 1996/12/14 14:04:39 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <X11/X.h>
#include <X11/Intrinsic.h>
#include <Xm/DialogS.h>
#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/PushBG.h>
#include <Xm/TextF.h>
#include <Xm/ToggleBG.h>
#include <Xm/RowColumn.h>

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/common.h"
#include "../ip/ip.h"
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
	void	*value;
} optData;

typedef	struct {
	String	name;
	Widget	holder;
	optData	*toggles;
	optData	*ints;
	optData	*others;
} optSheet;

static void set_opt __P((Widget, optData *));


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
	{ optToggle,	"edcompatible",	},
	{ optToggle,	"extended",	},
	{ optToggle,	"iclower",	},
	{ optToggle,	"ignorecase",	},
	{ optToggle,	"magic",	},
	{ optToggle,	"searchincr",	},
	{ optToggle,	"wrapscan",	},
	{ optTerminator,		},
},
		File_toggles[] = {
	{ optToggle,	"autowrite",	},
	{ optToggle,	"lock",		},
	{ optToggle,	"readonly",	},
	{ optToggle,	"warn",		},
	{ optToggle,	"writeany",	},
	{ optTerminator,		},
},
		Shell_toggles[] = {
	{ optToggle,	"secure",	},
	{ optTerminator,		},
},
		Ex_toggles[] = {
	{ optToggle,	"autoprint",	},
	{ optToggle,	"exrc",		},
	{ optToggle,	"prompt",	},
	{ optTerminator,		},
},
		Programming_toggles[] = {
	{ optToggle,	"autoindent",	},
	{ optToggle,	"lisp",		},
	{ optToggle,	"showmatch",	},
	{ optTerminator,		},
},
		Display_toggles[] = {
	{ optToggle,	"comment",	},
	{ optToggle,	"leftright",	},
	{ optToggle,	"list",		},
	{ optToggle,	"number",	},
	{ optToggle,	"octal",	},
	{ optToggle,	"ruler",	},
	{ optToggle,	"showmode",	},
	{ optToggle,	"windowname",	},
	{ optTerminator,		},
},
		Insert_toggles[] = {
	{ optToggle,	"altwerase",	},
	{ optToggle,	"beautify",	},
	{ optToggle,	"remap",	},
	{ optToggle,	"timeout",	},
	{ optToggle,	"ttywerase",	},
	{ optTerminator,		},
},
		Command_toggles[] = {
	{ optToggle,	"tildeop",	},
	{ optTerminator,		},
},
		Error_toggles[] = {
	{ optToggle,	"errorbells",	},
	{ optToggle,	"flash",	},
	{ optToggle,	"verbose",	},
	{ optTerminator,		},
};

static	optData Programming_ints[] = {
	{ optInteger,	"matchtime",	},
	{ optInteger,	"shiftwidth",	},
	{ optInteger,	"taglength",	},
	{ optTerminator,		},
},
		Display_ints[] = {
	{ optInteger,	"report",	},
	{ optInteger,	"tabstop",	},
	{ optTerminator,		},
},
		Insert_ints[] = {
	{ optInteger,	"wrapmargin",	},
	{ optInteger,	"escapetime",	},
	{ optInteger,	"wraplen",	},
	{ optTerminator,		},
};

static	optData	Search_others[] = {
	{ optString,	"paragraphs",	},
	{ optString,	"sections",	},
	{ optTerminator,		},
},
		File_others[] = {
	{ optFile,	"directory",	},
	{ optFile,	"recdir",	},
	{ optString,	"backup",	},
	{ optString,	"filec",	},
	{ optString,	"path",		},
	{ optTerminator,		},
},
		Shell_others[] = {
	{ optString,	"cdpath",	},
	{ optFile,	"shell",	},
	{ optString,	"shellmeta",	},
	{ optTerminator,		},
},
		Ex_others[] = {
	{ optString,	"cedit",	},
	{ optTerminator,		},
},
		Programming_others[] = {
	{ optString,	"tags",		},
	{ optTerminator,		},
},
		Display_others[] = {
	{ optString,	"print",	},
	{ optString,	"noprint",	},
	{ optTerminator,		},
},
		Error_others[] = {
	{ optString,	"msgcat",	},
	{ optTerminator,		},
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


/*
 * __vi_editopt --
 *	Set an edit option based on a core message.
 *
 * PUBLIC: int __vi_editopt __P((IP_BUF *));
 */
int
__vi_editopt(ipbp)
	IP_BUF *ipbp;
{
	optData *opt;

#undef	SEARCH
#define	SEARCH(toggle) {						\
	for (opt = toggle; opt->kind != optTerminator; ++opt)		\
		if (!strcmp(opt->name, ipbp->str1))			\
			goto found;					\
}

	SEARCH(Search_toggles);
	SEARCH(File_toggles);
	SEARCH(Shell_toggles);
	SEARCH(Ex_toggles);
	SEARCH(Programming_toggles);
	SEARCH(Display_toggles);
	SEARCH(Insert_toggles);
	SEARCH(Command_toggles);
	SEARCH(Error_toggles);
	SEARCH(Programming_ints);
	SEARCH(Display_ints);
	SEARCH(Insert_ints);
	SEARCH(Search_others);
	SEARCH(File_others);
	SEARCH(Shell_others);
	SEARCH(Ex_others);
	SEARCH(Programming_others);
	SEARCH(Display_others);
	SEARCH(Error_others);
	return (0);

found:	switch (opt->kind) {
	case optToggle:
		opt->value = (void *)ipbp->val1;
		break;
	case optInteger:
		if (opt->value != NULL)
			free(opt->value);
		if ((opt->value = malloc(15)) != NULL)
			(void)snprintf(opt->value,
			    15, "%lu", (u_long)ipbp->val1);
		break;
	case optString:
	case optFile:
		if (opt->value != NULL)
			free(opt->value);
		if ((opt->value = malloc(ipbp->len2)) != NULL)
			memcpy(opt->value, ipbp->str2, ipbp->len2);
		break;
	}
	return (0);
}

/*
 * set_opt --
 *	Send a set-edit-option message to core.
 */
static void
set_opt(w, opt)
	Widget w;
	optData *opt;
{
	Boolean set;
	IP_BUF ipb;
	String str;

	ipb.code = VI_EDITOPT;
	ipb.str1 = opt->name;
	ipb.len1 = strlen(opt->name);

	switch (opt->kind) {
	case optToggle:
		XtVaGetValues(w, XmNset, &set, 0);
		ipb.val1 = set;
		ipb.len2 = 0;

		if (strcmp(opt->name, "ruler") == 0)
			if (set)
				__vi_show_text_ruler_dialog(
				    __vi_screen->area, "Ruler");
			else
				__vi_clear_text_ruler_dialog();
		break;
	case optInteger:
		str = XmTextFieldGetString(w);
		ipb.val1 = atoi(str);
		ipb.len2 = 0;
		break;
	case optFile:
	case optString:
		ipb.str2 = XmTextFieldGetString(w);
		ipb.len2 = strlen(ipb.str2);
		break;
	}
	__vi_send("ab1", &ipb);
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
    XtAddCallback( w, XmNvalueChangedCallback, set_opt, option );
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
	XtAddCallback( w, XmNactivateCallback, set_opt, &options[i] );
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
