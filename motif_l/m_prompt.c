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
static const char sccsid[] = "$Id: m_prompt.c,v 8.4 1996/12/11 20:58:24 bostic Exp $ (Berkeley) $Date: 1996/12/11 20:58:24 $";
#endif /* not lint */

#include <X11/X.h>
#include <X11/Intrinsic.h>
#include <Xm/MessageB.h>

#include "m_motif.h"
#include "m_extern.h"


void	vi_fatal_message( parent, str )
Widget	parent;
String	str;
{
    Widget	db = XmCreateErrorDialog( parent, "Fatal", NULL, 0 );
    XmString	msg = XmStringCreateSimple( str );

    XtVaSetValues( XtParent(db),
		   XmNtitle,		"Fatal",
		   0
		   );
    XtVaSetValues( db,
		   XmNmessageString,	msg,
		   0
		   );
    XtAddCallback( XtParent(db), XmNpopdownCallback, __vi_cancel_cb, 0 );

    XtUnmanageChild( XmMessageBoxGetChild( db, XmDIALOG_CANCEL_BUTTON ) );
    XtUnmanageChild( XmMessageBoxGetChild( db, XmDIALOG_HELP_BUTTON ) );

    __vi_modal_dialog( db );

    exit(0);
}


void	vi_info_message( parent, str )
Widget	parent;
String	str;
{
    static	Widget	db = NULL;
    XmString	msg = XmStringCreateSimple( str );

    if ( db == NULL )
	db = XmCreateInformationDialog( parent, "Information", NULL, 0 );

    XtVaSetValues( XtParent(db),
		   XmNtitle,		"Information",
		   0
		   );
    XtVaSetValues( db,
		   XmNmessageString,	msg,
		   0
		   );
    XtAddCallback( XtParent(db), XmNpopdownCallback, __vi_cancel_cb, 0 );

    XtUnmanageChild( XmMessageBoxGetChild( db, XmDIALOG_CANCEL_BUTTON ) );
    XtUnmanageChild( XmMessageBoxGetChild( db, XmDIALOG_HELP_BUTTON ) );

    __vi_modal_dialog( db );
}
