/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil. All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: tcl.c,v 8.8 1996/03/06 19:53:50 bostic Exp $ (Berkeley) $Date: 1996/03/06 19:53:50 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "tcl_extern.h"

static void msghandler __P((SCR *, mtype_t, char *, size_t));
static void noscreen __P((Tcl_Interp *, int, char *));

extern GS *__global_list;			/* XXX */

/*
 * INITMESSAGE --
 *	Macros to point messages at the Tcl message handler.
 */
#define	INITMESSAGE							\
	scr_msg = __global_list->scr_msg;				\
	__global_list->scr_msg = msghandler;
#define	ENDMESSAGE							\
	__global_list->scr_msg = scr_msg;

/*
 * GETSCREENID --
 *	Macro to get the specified screen pointer.
 *
 * XXX
 * This is fatal.  We can't post a message into vi that we're unable to find
 * the screen without first finding the screen... So, this must be the first
 * thing a Tcl routine does, and, if it fails, the last as well.
 */
#define	GETSCREENID(sp, id, name) {					\
	int __id = id;							\
	char *__name = name;						\
	if (((sp) = api_fscreen(__id, __name)) == NULL) {		\
		noscreen(interp, __id, __name);				\
		return (TCL_ERROR);					\
	}								\
}

/*
 * tcl_fscreen --
 *	Return the screen id associated with file name.
 *
 * Tcl Command: viFindScreen
 * Usage: viFindScreen file
 */
static int
tcl_fscreen(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: viFindScreen file", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, 0, argv[1]);

	(void)sprintf(interp->result, "%d", sp->id);
	return (TCL_OK);
}

/*
 * tcl_aline --
 *	-- Append the string text after the line in lineNumber.
 *
 * Tcl Command: viAppendLine
 * Usage: viAppendLine screenId lineNumber text
 */
static int
tcl_aline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 4) {
		Tcl_SetResult(interp,
		    "Usage: viAppendLine screenId lineNumber text", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	rval = api_aline(sp,
	    (recno_t)strtoul(argv[2], NULL, 10), argv[3], strlen(argv[3]));
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_dline --
 *	Delete lineNum.
 *
 * Tcl Command: viDelLine
 * Usage: viDelLine screenId lineNum
 */
static int
tcl_dline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viDelLine screenId lineNumber", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	rval = api_dline(sp, (recno_t)strtoul(argv[2], NULL, 10));
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_gline --
 *	Return lineNumber.
 *
 * Tcl Command: viGetLine
 * Usage: viGetLine screenId lineNumber
 */
static int
tcl_gline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	size_t len;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *line, *p;

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viGetLine screenId lineNumber", TCL_STATIC);
		return (TCL_ERROR);
	}
	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	rval = api_gline(sp, (recno_t)strtoul(argv[2], NULL, 10), &p, &len);
	ENDMESSAGE;

	if (rval)
		return (TCL_ERROR);

	if ((line = malloc(len + 1)) == NULL)
		exit(1);				/* XXX */
	memmove(line, p, len);
	line[len] = NULL;
	Tcl_SetResult(interp, line, TCL_DYNAMIC);
	free(line);
	return (TCL_OK);
}

/*
 * tcl_iline --
 *	Insert the string text after the line in lineNumber.
 *
 * Tcl Command: viInsertLine
 * Usage: viInsertLine screenId lineNumber text
 */
static int
tcl_iline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 4) {
		Tcl_SetResult(interp,
		    "Usage: viInsertLine screenId lineNumber text", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	rval = api_iline(sp,
	    (recno_t)strtoul(argv[2], NULL, 10), argv[3], strlen(argv[3]));
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_lline --
 *	Return the last line in the screen.
 *
 * Tcl Command: viLastLine
 * Usage: viLastLine screenId
 */
static int
tcl_lline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	recno_t last;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: viLastLine screenId", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	rval = api_lline(sp, &last);
	ENDMESSAGE;
	if (rval)
		return (TCL_ERROR);

	(void)sprintf(interp->result, "%lu", (unsigned long)last);
	return (TCL_OK);
}

/*
 * tcl_sline --
 *	Set lineNumber to the text supplied.
 *
 * Tcl Command: viSetLine
 * Usage: viSetLine screenId lineNumber text
 */
static int
tcl_sline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 4) {
		Tcl_SetResult(interp,
		    "Usage: viSetLine screenId lineNumber text", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	rval = api_sline(sp,
	    (recno_t)strtoul(argv[2], NULL, 10), argv[3], strlen(argv[3]));
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_getmark --
 *	Return the mark's cursor position as a list with two elements.
 *	{line, column}.
 *
 * Tcl Command: viGetMark
 * Usage: viGetMark screenId mark
 */
static int
tcl_getmark(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	MARK cursor;
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char buf[20];

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viGetMark screenId mark", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	rval = api_getmark(sp, (int)argv[2][0], &cursor);
	ENDMESSAGE;

	if (rval)
		return (TCL_ERROR);

	(void)snprintf(buf, sizeof(buf), "%d", cursor.lno);
	Tcl_AppendElement(interp, buf);
	(void)snprintf(buf, sizeof(buf), "%d", cursor.cno);
	Tcl_AppendElement(interp, buf);
	return (TCL_OK);
}

/*
 * tcl_setmark --
 *	Set the mark to the line and column numbers supplied.
 *
 * Tcl Command: viSetMark
 * Usage: viSetMark screenId mark line column
 */
static int
tcl_setmark(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	MARK cursor;
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 5) {
		Tcl_SetResult(interp,
		    "Usage: viSetMark screenId mark line column", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	cursor.lno = atoi(argv[3]);
	cursor.cno = atoi(argv[4]);
	rval = api_setmark(sp, (int)argv[2][0], &cursor);
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_getcursor --
 *	Return the current cursor position as a list with two elements.
 *	{line, column}.
 *
 * Tcl Command: viGetCursor
 * Usage: viGetCursor screenId
 */
static int
tcl_getcursor(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	MARK cursor;
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char buf[20];

	if (argc != 2) {
		Tcl_SetResult(interp,
		    "Usage: viGetCursor screenId", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	rval = api_getcursor(sp, &cursor);
	ENDMESSAGE;

	if (rval)
		return (TCL_ERROR);

	(void)snprintf(buf, sizeof(buf), "%d", cursor.lno);
	Tcl_AppendElement(interp, buf);
	(void)snprintf(buf, sizeof(buf), "%d", cursor.cno);
	Tcl_AppendElement(interp, buf);
	return (TCL_OK);
}

/*
 * tcl_setcursor --
 *	Set the cursor to the line and column numbers supplied.
 *
 * Tcl Command: viSetCursor
 * Usage: viSetCursor screenId line column
 */
static int
tcl_setcursor(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	MARK cursor;
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 4) {
		Tcl_SetResult(interp,
		    "Usage: viSetCursor screenId line column", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	cursor.lno = atoi(argv[2]);
	cursor.cno = atoi(argv[3]);
	rval = api_setcursor(sp, &cursor);
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_msg --
 *	Set the message line to text.
 *
 * Tcl Command: viMsg
 * Usage: viMsg screenId text
 */
static int
tcl_msg(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;

	if (argc != 3) {
		Tcl_SetResult(interp, "Usage: viMsg screenId text", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	api_imessage(sp, argv[2]);

	return (TCL_OK);
}

/*
 * tcl_iscreen --
 *	Create a new screen.  If a filename is specified then the screen
 *	is opened with that file.
 *
 * Tcl Command: viNewScreen
 * Usage: viNewScreen screenId [file]
 */
static int
tcl_iscreen(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int id, rval;

	if (argc != 2 && argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viNewScreen screenId [file]", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	rval = api_iscreen(sp, argv[2], &id);
	ENDMESSAGE;

	if (rval)
		return (TCL_ERROR);

	(void)sprintf(interp->result, "%d", id);
	return (TCL_OK);
}

/*
 * tcl_escreen --
 *	End a screen.
 *
 * Tcl Command: viEndScreen
 * Usage: viEndScreen screenId
 */
static int
tcl_escreen(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 2) {
		Tcl_SetResult(interp,
		     "Usage: viEndScreen screenId", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	rval = api_escreen(sp);
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_swscreen --
 *	Change the current focus to screen.
 *
 * Tcl Command: viSwitchScreen
 * Usage: viSwitchScreen screenId screenId
 */
static int
tcl_swscreen(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp, *new;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viSwitchScreen cur_screenId new_screenId",
		    TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	GETSCREENID(new, atoi(argv[2]), NULL);
	rval = api_swscreen(sp, new);
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_map --
 *	Associate a key with a tcl procedure.
 *
 * Tcl Command: viMapKey
 * Usage: viMapKey screenId key tclproc
 */
static int
tcl_map(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char command[256];

	if (argc != 4) {
		Tcl_SetResult(interp,
		    "Usage: viMapKey screenId key tclproc", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	(void)snprintf(command, sizeof(command), ":tcl %s\n", argv[3]);
	rval = api_map(sp, argv[2], command, strlen(command));
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_unmap --
 *	Unmap a key.
 *
 * Tcl Command: viUnmapKey
 * Usage: viUnmMapKey screenId key
 */
static int
tcl_unmap(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viUnmapKey screenId key", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	rval = api_unmap(sp, argv[2]);
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_opts_set --
 *	Set an option.
 *
 * Tcl Command: viSetOpt
 * Usage: viSetOpt screenId command
 */
static int
tcl_opts_set(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viSetOpt screenId command", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	rval = api_opts_set(sp, argv[2]);
	ENDMESSAGE;

	return (rval ? TCL_ERROR : TCL_OK);
}

/*
 * tcl_opts_get --
 	Return the value of an option.
 *	
 * Tcl Command: viGetOpt
 * Usage: viGetOpt screenId option
 */
static int
tcl_opts_get(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *value;

	if (argc != 3) {
		Tcl_SetResult(interp,
		    "Usage: viGetOpt screenId option", TCL_STATIC);
		return (TCL_ERROR);
	}

	GETSCREENID(sp, atoi(argv[1]), NULL);
	INITMESSAGE;
	rval = api_opts_get(sp, argv[2], &value);
	ENDMESSAGE;
	if (rval)
		return (TCL_ERROR);

	Tcl_SetResult(interp, value, TCL_DYNAMIC);
	free(value);
	return (TCL_OK);
}

/*
 * tcl_init --
 *	Create the TCL commands used by nvi.
 *
 * PUBLIC: int tcl_init __P((GS *));
 */
int
tcl_init(gp)
	GS *gp;
{
	gp->tcl_interp = Tcl_CreateInterp();
	if (Tcl_Init(gp->tcl_interp) == TCL_ERROR)
		return (1);

#define	TCC(name, function) {						\
	Tcl_CreateCommand(gp->tcl_interp, name, function,		\
	    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);		\
}
	TCC("viAppendLine", tcl_aline);
	TCC("viDelLine", tcl_dline);
	TCC("viEndScreen", tcl_escreen);
	TCC("viFindScreen", tcl_fscreen);
	TCC("viGetCursor", tcl_getcursor);
	TCC("viGetLine", tcl_gline);
	TCC("viGetMark", tcl_getmark);
	TCC("viGetOpt", tcl_opts_get);
	TCC("viInsertLine", tcl_iline);
	TCC("viLastLine", tcl_lline);
	TCC("viMapKey", tcl_map);
	TCC("viMsg", tcl_msg);
	TCC("viNewScreen", tcl_iscreen);
	TCC("viSetCursor", tcl_setcursor);
	TCC("viSetLine", tcl_sline);
	TCC("viSetMark", tcl_setmark);
	TCC("viSetOpt", tcl_opts_set);
	TCC("viSwitchScreen", tcl_swscreen);
	TCC("viUnmapKey", tcl_unmap);

	return (0);
}

/*
 * msghandler --
 *	Tcl message routine so that error messages are processed in
 *	Tcl, not in nvi.
 */
static void
msghandler(sp, mtype, msg, len)
	SCR *sp;
	mtype_t mtype;
	char *msg;
	size_t len;
{
	/* Replace the trailing <newline> with an EOS. */
	msg[len - 1] = '\0';

	Tcl_SetResult(sp->gp->tcl_interp, msg, TCL_VOLATILE);
}

/*
 * noscreen --
 *	Tcl message if can't find the requested screen.
 */
static void
noscreen(interp, id, name)
	Tcl_Interp *interp;
	int id;
	char *name;
{
	char buf[256];

	if (name == NULL)
		(void)snprintf(buf, sizeof(buf), "unknown screen id: %d", id);
	else
		(void)snprintf(buf, sizeof(buf), "unknown screen: %s", name);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
}
