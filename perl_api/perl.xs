/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil. All rights reserved.
 * Copyright (c) 1996
 *	Sven Verdoolaege. All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: perl.xs,v 8.8 1996/03/21 08:59:47 bostic Exp $ (Berkeley) $Date: 1996/03/21 08:59:47 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "perl_extern.h"

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

static void noscreen __P((int, char *));
static void msghandler __P((SCR *, mtype_t, char *, size_t));

extern GS *__global_list;			/* XXX */

static char *errmsg = 0;

/*
 * INITMESSAGE --
 *	Macros to point messages at the Tcl message handler.
 */
#define	INITMESSAGE							\
	scr_msg = __global_list->scr_msg;				\
	__global_list->scr_msg = msghandler;
#define	ENDMESSAGE							\
	__global_list->scr_msg = scr_msg;				\
	if (rval) croak(errmsg);

/*
 * GETSCREENID --
 *	Macro to get the specified screen pointer.
 *
 * XXX
 * This is fatal.  We can't post a message into vi that we're unable to find
 * the screen without first finding the screen... So, this must be the first
 * thing a Perl routine does, and, if it fails, the last as well.
 */
#define	GETSCREENID(sp, id, name) {					\
	int __id = id;							\
	char *__name = name;						\
	if (((sp) = api_fscreen(__id, __name)) == NULL) {		\
		noscreen(__id, __name);					\
		return;							\
	}								\
}

/*
 * XS_VI_fscreen --
 *	Return the screen id associated with file name.
 *
 * Perl Command: VI::FindScreen
 * Usage: VI::FindScreen file
 */
XS(XS_VI_fscreen)
{
	dXSARGS;
	SCR *screen;

	if (items != 1)
		croak("Usage: VI::FindScreen file");
	SP -= items;

	GETSCREENID(screen, 0, (char *)SvPV(ST(0),na));

	EXTEND(sp, 1);
	PUSHs(sv_2mortal(newSViv(screen->id)));

	PUTBACK;
	return;
}

/*
 * XS_VI_aline --
 *	-- Append the string text after the line in lineNumber.
 *
 * Perl Command: VI::AppendLine
 * Usage: VI::AppendLine screenId lineNumber text
 */
XS(XS_VI_aline)
{
	dXSARGS;
	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *line;
	STRLEN length;

	if (items != 3)
		croak("Usage: VI::AppendLine screenId lineNumber text");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	line = (char *)SvPV(ST(2), length);
	INITMESSAGE;
	rval = api_aline(screen, (int)SvIV(ST(1)),
	                     line, length);
	ENDMESSAGE;

	PUTBACK;
	return;
}

/*
 * XS_VI_dline --
 *	Delete lineNum.
 *
 * Perl Command: VI::DelLine
 * Usage: VI::DelLine screenId lineNum
 */
XS(XS_VI_dline)
{
	dXSARGS;

	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (items != 2)
		croak("Usage: VI::DelLine screenId lineNumber");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	INITMESSAGE;
	rval = api_dline(screen, (recno_t)(int)SvIV(ST(1)));
	ENDMESSAGE;

	PUTBACK;
	return;
}

/*
 * XS_VI_gline --
 *	Return lineNumber.
 *
 * Perl Command: VI::GetLine
 * Usage: VI::GetLine screenId lineNumber
 */
XS(XS_VI_gline)
{
	dXSARGS;

	SCR *screen;
	size_t len;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *line, *p;

	if (items != 2)
		croak("Usage: VI::GetLine screenId lineNumber");
	SP -= items;
	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	INITMESSAGE;
	rval = api_gline(screen, (recno_t)(int)SvIV(ST(1)), &p, &len);
	ENDMESSAGE;

	EXTEND(sp,1);
        PUSHs(sv_2mortal(newSVpv(p, len)));

	PUTBACK;
	return;
}

/*
 * XS_VI_iline --
 *	Insert the string text before the line in lineNumber.
 *
 * Perl Command: VI::InsertLine
 * Usage: VI::InsertLine screenId lineNumber text
 */
XS(XS_VI_iline)
{
	dXSARGS;

	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *line;
	STRLEN length;

	if (items != 3)
		croak("Usage: VI::InsertLine screenId lineNumber text");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	line = (char *)SvPV(ST(2), length);
	INITMESSAGE;
	rval = api_iline(screen, (int)SvIV(ST(1)),
	                     line, length);
	ENDMESSAGE;

	PUTBACK;
	return;
}

/*
 * XS_VI_lline --
 *	Return the last line in the screen.
 *
 * Perl Command: VI::LastLine
 * Usage: VI::LastLine screenId
 */
XS(XS_VI_lline) 
{
	dXSARGS;
	SCR *screen;
	recno_t last;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (items != 1) 
		croak("Usage: VI::LastLine(screenId)");
	SP -= items;
	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);

	INITMESSAGE;
	rval = api_lline(screen, &last);
	ENDMESSAGE;

	EXTEND(sp,1);
        PUSHs(sv_2mortal(newSViv(last)));

	PUTBACK;
	return;
}

/*
 * XS_VI_sline --
 *	Set lineNumber to the text supplied.
 *
 * Perl Command: VI::SetLine
 * Usage: VI::SetLine screenId lineNumber text
 */
XS(XS_VI_sline)
{
	dXSARGS;
	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	STRLEN length;
	char *line;

	if (items != 3) 
		croak("Usage: VI::SetLine(screenid, linenumber, text)");

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	line = (char *)SvPV(ST(2), length);
	INITMESSAGE;
	rval = api_sline(screen, (int)SvIV(ST(1)),
	                     line, length);
	ENDMESSAGE;

	PUTBACK;
	return;
}

/*
 * XS_VI_getmark --
 *	Return the mark's cursor position as a list with two elements.
 *	{line, column}.
 *
 * Perl Command: VI::GetMark
 * Usage: VI::GetMark screenId mark
 */
XS(XS_VI_getmark)
{
	dXSARGS;

	struct _mark cursor;
	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char buf[20];

	if (items != 2)
		croak("Usage: VI::GetMark screenId mark");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	INITMESSAGE;
	rval = api_getmark(screen, (int)*(char *)SvPV(ST(1),na), &cursor);
	ENDMESSAGE;

	EXTEND(sp,2);
        PUSHs(sv_2mortal(newSViv(cursor.lno)));
        PUSHs(sv_2mortal(newSViv(cursor.cno)));
	PUTBACK;
	return;
}

/*
 * XS_VI_setmark --
 *	Set the mark to the line and column numbers supplied.
 *
 * Perl Command: VI::SetMark
 * Usage: VI::SetMark screenId mark line column
 */
XS(XS_VI_setmark)
{
	dXSARGS;

	struct _mark cursor;
	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (items != 5)
		croak("Usage: VI::SetMark screenId mark line column");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	INITMESSAGE;
	cursor.lno = (int)SvIV(ST(2));
	cursor.cno = (int)SvIV(ST(3));
	rval = api_setmark(screen, (int)*(char *)SvPV(ST(1),na), &cursor);
	ENDMESSAGE;

	PUTBACK;
	return;
}

/*
 * XS_VI_getcursor --
 *	Return the current cursor position as a list with two elements.
 *	{line, column}.
 *
 * Perl Command: VI::GetCursor
 * Usage: VI::GetCursor screenId
 */
XS(XS_VI_getcursor)
{
	dXSARGS;

	struct _mark cursor;
	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char buf[20];

	if (items != 1)
		croak("Usage: VI::GetCursor screenId");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	INITMESSAGE;
	rval = api_getcursor(screen, &cursor);
	ENDMESSAGE;

	EXTEND(sp,2);
        PUSHs(sv_2mortal(newSViv(cursor.lno)));
        PUSHs(sv_2mortal(newSViv(cursor.cno)));
	PUTBACK;
	return;
}

/*
 * XS_VI_setcursor --
 *	Set the cursor to the line and column numbers supplied.
 *
 * Perl Command: VI::SetCursor
 * Usage: VI::SetCursor screenId line column
 */
XS(XS_VI_setcursor)
{
	dXSARGS;

	struct _mark cursor;
	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (items != 3)
		croak("Usage: VI::SetCursor screenId line column");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	INITMESSAGE;
	cursor.lno = (int)SvIV(ST(1));
	cursor.cno = (int)SvIV(ST(2));
	rval = api_setcursor(screen, &cursor);
	ENDMESSAGE;

	PUTBACK;
	return;
}

/*
 * XS_VI_msg --
 *	Set the message line to text.
 *
 * Perl Command: VI::msg
 * Usage: VI::msg screenId text
 */
XS(XS_VI_msg) 
{
	dXSARGS;
	SCR *screen;

	if (items != 2) 
		croak("Usage: VI::Msg(screenId, text)");
	SP -= items;
	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	api_imessage(screen, (char *)SvPV(ST(1), na));
	PUTBACK;
	return;
}

/*
 * XS_VI_iscreen --
 *	Create a new screen.  If a filename is specified then the screen
 *	is opened with that file.
 *
 * Perl Command: VI::NewScreen
 * Usage: VI::NewScreen screenId [file]
 */
XS(XS_VI_iscreen)
{
	dXSARGS;

	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int id, rval;

	if (items != 2)
		croak("Usage: VI::NewScreen screenId [file]");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	INITMESSAGE;
	rval = api_iscreen(screen, (char *)SvPV(ST(1),na), &id);
	ENDMESSAGE;

	EXTEND(sp,1);
        PUSHs(sv_2mortal(newSViv(id)));
	PUTBACK;
	return;
}

/*
 * XS_VI_escreen --
 *	End a screen.
 *
 * Perl Command: VI::EndScreen
 * Usage: VI::EndScreen screenId
 */
XS(XS_VI_escreen)
{
	dXSARGS;

	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (items != 1)
		croak("Usage: VI::EndScreen screenId");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	INITMESSAGE;
	rval = api_escreen(screen);
	ENDMESSAGE;

	PUTBACK;
	return;
}

/*
 * XS_VI_swscreen --
 *	Change the current focus to screen.
 *
 * Perl Command: VI::SwitchScreen
 * Usage: VI::SwitchScreen screenId screenId
 */
XS(XS_VI_swscreen)
{
	dXSARGS;

	SCR *screen, *new;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (items != 2)
		croak("Usage: VI::SwitchScreen cur_screenId new_screenId");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	INITMESSAGE;
	GETSCREENID(new, (int)SvIV(ST(1)), NULL);
	rval = api_swscreen(screen, new);
	ENDMESSAGE;

	PUTBACK;
	return;
}

/*
 * XS_VI_map --
 *	Associate a key with a perl procedure.
 *
 * Perl Command: VI::MapKey
 * Usage: VI::MapKey screenId key perlproc
 */
XS(XS_VI_map)
{
	dXSARGS;

	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char command[256];

	if (items != 3)
		croak("Usage: VI::MapKey screenId key perlproc");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	INITMESSAGE;
	(void)snprintf(command, sizeof(command), ":perl %s\n", 
	                                         (char *)SvPV(ST(2),na));
	rval = api_map(screen, (char *)SvPV(ST(1),na), command, 
	               strlen(command));
	ENDMESSAGE;

	PUTBACK;
	return;
}

/*
 * XS_VI_unmap --
 *	Unmap a key.
 *
 * Perl Command: VI::UnmapKey
 * Usage: VI::UnmmapKey screenId key
 */
XS(XS_VI_unmap)
{
	dXSARGS;

	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (items != 2)
		croak("Usage: VI::UnmapKey screenId key");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	INITMESSAGE;
	rval = api_unmap(screen, (char *)SvPV(ST(1),na));
	ENDMESSAGE;

	PUTBACK;
	return;
}

/*
 * XS_VI_opts_set --
 *	Set an option.
 *
 * Perl Command: VI::SetOpt
 * Usage: VI::SetOpt screenId command
 */
XS(XS_VI_opts_set)
{
	dXSARGS;

	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (items != 2)
		croak("Usage: VI::SetOpt screenId command");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	INITMESSAGE;
	rval = api_opts_set(screen, (char *)SvPV(ST(1),na));
	ENDMESSAGE;

	PUTBACK;
	return;
}

/*
 * XS_VI_opts_get --
 	Return the value of an option.
 *	
 * Perl Command: VI::GetOpt
 * Usage: VI::GetOpt screenId option
 */
XS(XS_VI_opts_get)
{
	dXSARGS;

	SCR *screen;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *value;

	if (items != 2)
		croak("Usage: VI::GetOpt screenId option");
	SP -= items;

	GETSCREENID(screen, (int)SvIV(ST(0)), NULL);
	INITMESSAGE;
	rval = api_opts_get(screen, (char *)SvPV(ST(1),na), &value);
	ENDMESSAGE;

	EXTEND(SP,1);
        PUSHs(sv_2mortal(newSVpv(value, na)));
	free(value);
	PUTBACK;
	return;
}

/*
 * perl_init --
 *	Create the perl commands used by nvi.
 *
 * PUBLIC: int perl_init __P((GS *));
 */
int
perl_init(gp)
	GS *gp;
{
        char buf[64];

	static char *embedding[] = { "", "-e", "sub _eval_ { eval $_[0] }" };
	STRLEN length;
	char *file = __FILE__;

	gp->perl_interp = perl_alloc();
  	perl_construct(gp->perl_interp);
	perl_parse(gp->perl_interp, NULL, 3, embedding, 0);
	newXS("VI::AppendLine", XS_VI_aline, file);
	newXS("VI::DelLine", XS_VI_dline, file);
	newXS("VI::EndScreen", XS_VI_escreen, file);
	newXS("VI::GetLine", XS_VI_gline, file);
	newXS("VI::FindScreen", XS_VI_fscreen, file);
	newXS("VI::GetCursor", XS_VI_getcursor, file);
	newXS("VI::GetMark", XS_VI_getmark, file);
	newXS("VI::GetOpt", XS_VI_opts_get, file);
	newXS("VI::InsertLine", XS_VI_iline, file);
	newXS("VI::LastLine", XS_VI_lline, file);
	newXS("VI::MapKey", XS_VI_map, file);
	newXS("VI::Msg", XS_VI_msg, file);
	newXS("VI::NewScreen", XS_VI_iscreen, file);
	newXS("VI::SetCursor", XS_VI_setcursor, file);
	newXS("VI::SetLine", XS_VI_sline, file);
	newXS("VI::SetMark", XS_VI_setmark, file);
	newXS("VI::SetOpt", XS_VI_opts_set, file);
	newXS("VI::SwitchScreen", XS_VI_swscreen, file);
	newXS("VI::UnmapKey", XS_VI_unmap, file);

	return (0);
}

/*
 * msghandler --
 *	Perl message routine so that error messages are processed in
 *	Perl, not in nvi.
 */
static void
msghandler(sp, mtype, msg, len)
	SCR *sp;
	mtype_t mtype;
	char *msg;
	size_t len;
{
	/* Replace the trailing <newline> with an EOS. */
	/* Let's do that later instead */
	if (errmsg) free (errmsg);
	errmsg = malloc(len + 1);
	memcpy(errmsg, msg, len);
	errmsg[len] = '\0';
}

/*
 * noscreen --
 *	Perl message if can't find the requested screen.
 */
static void
noscreen(id, name)
	int id;
	char *name;
{
	char buf[256];

	if (name == NULL)
		(void)snprintf(buf, sizeof(buf), "unknown screen id: %d", id);
	else
		(void)snprintf(buf, sizeof(buf), "unknown screen: %s", name);
	croak(buf);
}
