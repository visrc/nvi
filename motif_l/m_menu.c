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
static const char sccsid[] = "$Id: m_menu.c,v 8.13 1996/12/05 12:30:57 bostic Exp $ (Berkeley) $Date: 1996/12/05 12:30:57 $";
#endif /* not lint */

#include <sys/queue.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/FileSB.h>
#include <Xm/SelectioB.h>

#include <bitstring.h>
#include <stdio.h>

#include "../common/common.h"
#include "../ip_vi/ip.h"
#include "ipc_extern.h"

/* This module defines the menu structure for vi.  Each menu
 * item has an action routine associated with it.  For the most
 * part, those actions will simply call ip_send with vi commands.
 * others will pop up file selection dialogs and use them for
 * vi commands, and other will have to have special actions.
 *
 * Future:
 *	vi core will have to let us know when to be sensitive
 *	change IPO_STRING to IPO_COMMAND so that menu actions cannot
 *		be confusing when in insert mode
 *	need IPO_CUT, IPO_COPY, and IPO_PASTE to perform the appropriate
 *		operations on the visible text of yank buffer.  IPO_COPY
 *		is likely a NOP, but it will make users happy
 *	add mnemonics
 *	add accelerators
 *	implement file selection dialog boxes
 *	implement string prompt dialog boxes (e.g. for 'find')
 *
 * Interface:
 *	Widget	create_menubar( Widget parent ) creates and returns the
 *		X menu structure.  The caller can place this
 *		anywhere in the widget heirarchy.
 */

#define	BufferSize	1024

/* Utility:  Send a menu command to vi
 *
 *	Future:
 *	change IPO_STRING to IPO_COMMAND so that menu actions cannot
 *		be confusing when in insert mode
 *
 * XXX
 * THIS SHOULD GO AWAY -- WE SHOULDN'T SEND UNINTERPRETED STRINGS TO THE
 * CORE.
 */

#if defined(__STDC__)
void	send_command_string( String str )
#else
void	send_command_string( str )
String	str;
#endif
{
    IP_BUF	ipb;
    char	buffer[BufferSize];

    /* Future:  Need IPO_COMMAND so vi knows this is not text to insert
     * At that point, appending a cr/lf will not be necessary.  For now,
     * append iff we are a colon or slash command.  Of course, if we are in
     * insert mode, all bets are off.
     */
    strcpy( buffer, str );
    switch ( *str ) {
	case ':':
	case '/':
	    strcat( buffer, "\n" );
	    break;
    }

    ipb.code = IPO_STRING;
    ipb.str = buffer;
    ipb.len = strlen(buffer);
    ip_send("s", &ipb);
}
/* Utility:  beep for unimplemented command */

#if defined(__STDC__)
void	send_beep( Widget w )
#else
void	send_beep( w )
Widget	w;
#endif
{
    XBell( XtDisplay(w), 0 );
}


/* Utility:  make a dialog box go Modal */

Bool	have_answer;

#if defined(__STDC__)
void		cancel_cb( Widget w,
			   XtPointer client_data,
			   XtPointer call_data
			   )
#else
void		cancel_cb( w, client_data, call_data )
Widget		w;
XtPointer	client_data;
XtPointer	call_data;
#endif
{
    have_answer = True;
}

void	modal_dialog( db )
Widget	db;
{
    XtAppContext	ctx;

    /* post the dialog */
    XtManageChild( db );
    XtPopup( XtParent(db), XtGrabExclusive );

    /* wait for a response */
    ctx = XtWidgetToApplicationContext(db);
    XtAddGrab( XtParent(db), TRUE, FALSE );
    for ( have_answer = False; ! have_answer; )
	XtAppProcessEvent( ctx, XtIMAll );

    /* done with db */
    XtPopdown( XtParent(db) );
    XtRemoveGrab( XtParent(db) );
}


/* Utility:  Get a file (using standard File Selection Dialog Box) */

String	file_name;


#if defined(__STDC__)
void		ok_file_name( Widget w,
			      XtPointer client_data,
			      XtPointer call_data
			      )
#else
void		ok_file_name( w, client_data, call_data )
Widget		w;
XtPointer	client_data;
XtPointer	call_data;
#endif
{
    XmFileSelectionBoxCallbackStruct	*cbs;

    cbs = (XmFileSelectionBoxCallbackStruct *) call_data;
    XmStringGetLtoR( cbs->value, XmSTRING_DEFAULT_CHARSET, &file_name );

    have_answer = True;
}


#if defined(__STDC__)
String	get_file( Widget w, String prompt )
#else
String	get_file( w, prompt )
Widget	w;
String	prompt;
#endif
{
    /* make it static so we can reuse it */
    static	Widget	db;

    /* our return parameter */
    if ( file_name != NULL ) {
	XtFree( file_name );
	file_name = NULL;
    }

    /* create one? */
    if ( db == NULL ){ 
	db = XmCreateFileSelectionDialog( w, "file", NULL, 0 );
	XtAddCallback( db, XmNokCallback, ok_file_name, NULL );
	XtAddCallback( db, XmNcancelCallback, cancel_cb, NULL );
    }

    /* use the title as a prompt */
    XtVaSetValues( XtParent(db), XmNtitle, prompt, 0 );

    /* wait for a response */
    modal_dialog( db );

    /* done */
    return file_name;
}


/* Utility:  Get a string (using standard File Selection Dialog Box) */

String	string_name;


#if defined(__STDC__)
void		ok_string( Widget w,
			   XtPointer client_data,
			   XtPointer call_data
			   )
#else
void		ok_string( w, client_data, call_data )
Widget		w;
XtPointer	client_data;
XtPointer	call_data;
#endif
{
    XmSelectionBoxCallbackStruct	*cbs;

    cbs = (XmSelectionBoxCallbackStruct *) call_data;
    XmStringGetLtoR( cbs->value, XmSTRING_DEFAULT_CHARSET, &string_name );

    have_answer = True;
}


#if defined(__STDC__)
String	get_string( Widget w, String prompt )
#else
String	get_string( w, prompt )
Widget	w;
String	prompt;
#endif
{
    /* make it static so we can reuse it */
    static	Widget		db;
		XmString	xmstr;

    /* our return parameter */
    if ( string_name != NULL ) {
	XtFree( string_name );
	string_name = NULL;
    }

    /* create one? */
    if ( db == NULL ){ 
	db = XmCreatePromptDialog( w, "string", NULL, 0 );
	XtAddCallback( db, XmNokCallback, ok_string, NULL );
	XtAddCallback( db, XmNcancelCallback, cancel_cb, NULL );
    }

    /* this one has space for a prompt... */
    xmstr = XmStringCreateSimple( prompt );
    XtVaSetValues( db, XmNselectionLabelString, xmstr, 0 );
    XmStringFree( xmstr );

    /* wait for a response */
    modal_dialog( db );

    /* done */
    return string_name;
}


/*
 * string_command --
 *	Get a string and send it with the command to the core.
 */
static void
string_command(w, code, prompt)
	Widget w;
	int code;
	String prompt;
{
	IP_BUF ipb;
	char *str;

	if ((str = get_string(w, prompt)) != NULL ) {
		ipb.code = code;
		ipb.str = str;
		/*
		 * XXX
		 * This is REALLY sleazy.  We pass the nul along with the
		 * string so that the core editor doesn't have to copy the
		 * string to get a nul termination.  This should be fixed
		 * as part of making the editor fully 8-bit clean.
		 */
		ipb.len = strlen(str) + 1;
		ip_send("s", &ipb);
	}
}


/*
 * file_command --
 *	Get a file name and send it with the command to the core.
 */
static void
file_command(w, code, prompt)
	Widget	w;
	int code;
	String prompt;
{
	IP_BUF ipb;
	char *file;

	if ((file = get_file(w, prompt)) != NULL) {
		ipb.code = code;
		ipb.str = file;
		ipb.len = strlen(file);
		ip_send("s", &ipb);
	}
}


/*
 * Menu action routines (one per menu entry)
 *
 * These are in the order in which they appear in the menu structure.
 */
void
ma_edit_file(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	file_command(w, IPO_EDIT, "Edit");
}

void
ma_split(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	file_command(w, IPO_SPLIT, "Edit");
}

void
ma_save(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	IP_BUF ipb;

	ipb.code = IPO_WRITE;
	(void)ip_send(NULL, &ipb);
}

void
ma_save_as(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	file_command(w, IPO_WRITEAS, "Save As");
}

void
ma_wq(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	IP_BUF ipb;

	ipb.code = IPO_WQ;
	(void)ip_send(NULL, &ipb);
}

void
ma_quit(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	IP_BUF ipb;

	ipb.code = IPO_QUIT;
	(void)ip_send(NULL, &ipb);
}

void
ma_undo(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	IP_BUF ipb;

	ipb.code = IPO_UNDO;
	(void)ip_send(NULL, &ipb);
}

#if defined(__STDC__)
void		ma_cut(	Widget w,
			XtPointer call_data,
			XtPointer client_data
			)
#else
void		ma_cut( w, call_data, client_data )
Widget		w;
XtPointer	call_data;
XtPointer	client_data;
#endif
{
    /* future */
    send_beep( w );
}


#if defined(__STDC__)
void		ma_copy(	Widget w,
				XtPointer call_data,
				XtPointer client_data
				)
#else
void		ma_copy( w, call_data, client_data )
Widget		w;
XtPointer	call_data;
XtPointer	client_data;
#endif
{
    /* future */
    send_beep( w );
}


#if defined(__STDC__)
void		ma_paste(	Widget w,
				XtPointer call_data,
				XtPointer client_data
				)
#else
void		ma_paste( w, call_data, client_data )
Widget		w;
XtPointer	call_data;
XtPointer	client_data;
#endif
{
    /* future */
    send_beep( w );
}

void
ma_append(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	IP_BUF ipb;

	ipb.code = IPO_APPEND;
	(void)ip_send(NULL, &ipb);
}

void
ma_insert(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	IP_BUF ipb;

	ipb.code = IPO_INSERT;
	(void)ip_send(NULL, &ipb);
}

void
ma_escape(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	IP_BUF ipb;

	ipb.code = IPO_EINSERT;
	(void)ip_send(NULL, &ipb);
}

void
ma_find(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	xip_show_search_dialog( w, "Find" );
}

void
ma_find_next(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	xip_next_search();
}

void
ma_tag(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	IP_BUF ipb;

	ipb.code = IPO_TAG;
	(void)ip_send(NULL, &ipb);
}

void
ma_tagsplit(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	IP_BUF ipb;

	ipb.code = IPO_TAGSPLIT;
	(void)ip_send(NULL, &ipb);
}

void
ma_tagas(w, call_data, client_data)
	Widget w;
	XtPointer call_data, client_data;
{
	string_command(w, IPO_TAGAS, "Tag");
}

#if defined(__STDC__)
void		ma_preferences(	Widget w,
				XtPointer call_data,
				XtPointer client_data
				)
#else
void		ma_preferences( w, call_data, client_data )
Widget		w;
XtPointer	call_data;
XtPointer	client_data;
#endif
{
    xip_show_options_dialog( w, "Preferences" );
}


/* Menu construction routines */

typedef	struct {
    String	title;
    void	(*action)();
} pull_down;

typedef	struct {
    char	mnemonic;
    String	title;
    pull_down	*actions;
} menu_bar;

pull_down	file_menu[] = {
    { "Edit File...",		ma_edit_file },
    { "",			NULL },
    { "Split Window...",	ma_split },
    { "",			NULL },
    { "Save ",			ma_save },
    { "Save As...",		ma_save_as },
    { "",			NULL },
    { "Write and Quit",		ma_wq },
    { "Quit",			ma_quit },
    { NULL,			NULL },
};

pull_down	edit_menu[] = {
    { "Undo",			ma_undo },
    { "",			NULL },
    { "Cut",			ma_cut },
    { "Copy",			ma_copy },
    { "Paste",			ma_paste },
    { "",			NULL },
    { "Append",			ma_append },
    { "Insert",			ma_insert },
    { "Exit Append or Insert",	ma_escape },
    { "",			NULL },
    { "Find",			ma_find },
    { "Find Next",		ma_find_next },
    { NULL,			NULL },
};

pull_down	options_menu[] = {
    { "Preferences",		ma_preferences },
    { "Command Mode Maps",	NULL },
    { "Insert Mode Maps",	NULL },
    { NULL,			NULL },
};

pull_down	tag_menu[] = {
    { "Go To Tag",		ma_tag },
    { "Split To Tag",		ma_tagsplit },
    { "Tag As...",		ma_tagas },
    { NULL,			NULL },
};

pull_down	help_menu[] = {
    { NULL,			NULL },
};

menu_bar	main_menu[] = {
    { 'F',	"File",		file_menu	},
    { 'E',	"Edit", 	edit_menu	},
    { 'O',	"Options",	options_menu	},
    { 'T',	"Tag",		tag_menu	},
    { 'H',	"Help",		help_menu	},
    { 0,	NULL,		NULL		},
};


#if defined(__STDC__)
void		add_entries( Widget parent, pull_down *actions )
#else
void		add_entries( parent, actions )
Widget		parent;
pull_down	*actions;
#endif
{
    Widget	w;

    for ( ; actions->title != NULL; actions++ ) {

	/* a separator? */
	if ( *actions->title != '\0' ) {
	    w = XmCreatePushButton( parent, actions->title, NULL, 0 );
	    if ( actions->action != NULL )
		XtAddCallback(  w,
				XmNactivateCallback,
				(XtCallbackProc) actions->action,
				actions
				);
	}
	else {
	    w = XmCreateSeparator( parent, "separator", NULL, 0 );
	}

	XtManageChild( w );

    }
}

/*
 * create_menubar --
 *
 * PUBLIC: Widget create_menubar __P((Widget));
 */
Widget
create_menubar(parent)
	Widget parent;
{
    Widget	menu, pull, button;
    menu_bar	*ptr;

    menu = XmCreateMenuBar( parent, "menu", NULL, 0 );

    for ( ptr=main_menu; ptr->title != NULL; ptr++ ) {

	pull = XmCreatePulldownMenu( menu, "pull", NULL, 0 );
	add_entries( pull, ptr->actions );
	button = XmCreateCascadeButton( menu, ptr->title, NULL, 0 );
	XtVaSetValues( button, XmNsubMenuId, pull, 0 );

	if ( strcmp( ptr->title, "Help" ) == 0 )
	    XtVaSetValues( menu, XmNmenuHelpWidget, button, 0 );

	if ( ptr->mnemonic )
	    XtVaSetValues( button, XmNmnemonic, ptr->mnemonic, 0 );

	XtManageChild( button );
    }

    return menu;
}
