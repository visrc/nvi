/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "compat.h"
#include <db.h>
#include <regex.h>

#if 0
#include <malloc.h>
#endif

#include "common.h"

#include "../api/api.h"

#include <tcl.h>
/* This file contains the TCL commands that are implemented in the api.c */
/* for use by the interpreter.  */

/*
 * Tcl Command: viDelLine
 * Usage: viDelLine [screen] lineNum
 *
 * Deletes the line in lineNum in the current screen, unless another
 * screen is first.
 */

int 
tcl_dline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{

	SCR *sp;
	int lineNum, last; 

	if (argc < 2 || argc > 3) {
		Tcl_SetResult(interp, "Usage: viDelLine [screen] lineNumber",
				TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 2) {
	    lineNum = atoi(argv[1]);
	    sp = api_gscr();
	} else {
	    lineNum = atoi(argv[2]);
	    sp = (SCR *)atol(argv[1]);
	}

	if (api_lline(sp, &last)) {
		Tcl_SetResult(interp, "NVI internal error.", TCL_STATIC);
		return TCL_ERROR;
	}

	if (lineNum < 1) {
		Tcl_SetResult(interp, "Error: lineNum must be > 0",
				TCL_STATIC);
		return TCL_ERROR;
	}
	if (lineNum > last) {	
		Tcl_SetResult(interp, 
			    "Error: lineNum must be less than the number of lines in the screen.",
				TCL_STATIC);
		return TCL_ERROR;
	}

	if (api_dline(sp, lineNum)) {
		Tcl_SetResult(interp,
			"Error: line could not be deleted.",
			TCL_STATIC);
		return (TCL_ERROR);
	}

	return (TCL_OK);
}

/*
 * Tcl Command: viLastLine
 * Usage: viLastLine [screen]
 *
 * Returns the last line in a screen.  If no scren is specified then the
 * default screen is used.
 */

int 
tcl_lline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	recno_t last;

	if (argc > 2) {
		Tcl_SetResult(interp, "Usage: viLastLine [screen]", TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 1) {
	    sp = api_gscr();
	} else {
	    sp = (SCR *)atol(argv[1]);
	}

	if (api_lline(sp, &last)) {
		Tcl_SetResult(interp, "NVI internal error.", TCL_STATIC);
		return TCL_ERROR;
	} else {
		sprintf(interp->result, "%d", last);
		return TCL_OK;
	}

}

/*
 * Tcl Command: viAppendLine
 * Usage: viAppendLine [screen] lineNumber text
 *
 * Append the string text after the line in lineNumber.  If no screen
 * is supplied then the default one is used.
 *
 */

int 
tcl_aline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{

	SCR *sp;
	CHAR_T *text;
	int line;
	recno_t last;


	if (argc < 3 || argc > 4) {
		Tcl_SetResult(interp, 
				"Usage: viAppendLine [screen] lineNumber text",
				TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 3) {
		sp = api_gscr();
		line = atoi(argv[1]);
		text = argv[2];
	} else {
		sp = (SCR *)atol(argv[1]);
		line = atoi(argv[2]);
		text = argv[3];
	}

	if (api_lline(sp, &last)) {
		Tcl_SetResult(interp, "could not get last line", TCL_STATIC);
		return TCL_ERROR;
	}

	if (last < line) {
		Tcl_SetResult(interp, "Last line past the end of screen.", 
				TCL_STATIC);
		return TCL_ERROR;
	}

	if (api_aline(sp, line, text, strlen(text))) {
		Tcl_SetResult(interp, "could not append line", TCL_STATIC);
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 * Tcl Command: viInsertLine
 * Usage: viInsertLine [screen] lineNumber text
 *
 * Insert the string text after the line in lineNumber.  If no screen is
 * specified then use the default.
 *
 */

int 
tcl_iline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{

	SCR *sp;
	CHAR_T *text;
	recno_t last;
	int line;

	if (argc < 3 || argc > 4) {
		Tcl_SetResult(interp, 
				"Usage: viInsertLine [screen] lineNumber text",
				TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 3) {
		sp = api_gscr();
		line = atoi(argv[1]);
		text = argv[2];
	} else {
		sp = (SCR *)atol(argv[1]);
		line = atoi(argv[2]);
		text = argv[3];
	}

	if (api_lline(sp, &last)) {
		Tcl_SetResult(interp, "could not get last line", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (line > last) {
		Tcl_SetResult(interp, "Error: line out of range.",
		    TCL_STATIC);
		return (TCL_ERROR);
	}

	if (api_iline(sp, line, text, strlen(text))) {
		Tcl_SetResult(interp, "could not insert line", TCL_STATIC);
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 * Tcl Command: viGetLine
 * Usage: viGetLine [screen] lineNumber 
 *
 * Return the line in lineNumber.  If no screen is specified use the 
 * default.
 *
 */

int 
tcl_gline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	char *tmp, *line;
	size_t lenp;
	recno_t last;
	int lineNum;

	if (argc < 2 || argc > 3) {
		Tcl_SetResult(interp, "Usage: viGetLine [screen] lineNumber",
				TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 2) {
		sp = api_gscr();
		lineNum = atoi(argv[1]);
	} else {
		sp = (SCR *)atol(argv[1]);
		lineNum = atoi(argv[2]);
	}

	if (api_lline(sp, &last)) {
		Tcl_SetResult(interp, "could not get last line", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (lineNum > last) {
		Tcl_SetResult(interp, "Error: line out of range.",
		    TCL_STATIC);
		return (TCL_ERROR);
	}

	if (api_gline(sp, lineNum, &tmp, &lenp)) {
		Tcl_SetResult(interp, "could not get line", TCL_STATIC);
		return (TCL_ERROR);
	} else {
		line = malloc(lenp + 1);
		if (line == NULL)
			exit(1);
		memmove(line, tmp, lenp);
		line[lenp]=NULL;
		Tcl_SetResult(interp, line, TCL_DYNAMIC);
		free(line);
		return (TCL_OK);
	}
}


/*
 * Tcl Command: viSetLine
 * Usage: viSetLine [screen] lineNumber text
 *
 * Set the line in lineNumber to the text supplied.  If no screen is 
 * specified use the default.
 *
 *
 */

int 
tcl_sline(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{ 
	SCR *sp;
	CHAR_T *text;
	recno_t last;
	int line;

	if (argc < 3 || argc > 4) {
		Tcl_SetResult(interp, 
				"Usage: viSetLine [screen] lineNumber text",
				TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 3) {
		sp = api_gscr();
		line = atoi(argv[1]);
		text = argv[2];
	} else {
		sp = (SCR *)atol(argv[1]);
		line = atoi(argv[2]);
		text = argv[3];
	}

	if (api_lline(sp, &last)) {
		Tcl_SetResult(interp, "could not get last line", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (line > last) {
		Tcl_SetResult(interp, "Error: line out of range.",
		    TCL_STATIC);
		return (TCL_ERROR);
	}

	if (api_sline(sp, line, text, strlen(text))) {
		Tcl_SetResult(interp, "could not set line", TCL_STATIC);
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 * Tcl Command: viExCmd
 * Usage: viExCmd 
 *
 * Execute an ex command.
 *
 */

int 
tcl_ex_exec(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	int i;
	SCR *sp;
	Tcl_DString command;	

	sp = api_gscr();
	Tcl_DStringInit(&command);

	for (i = 1; i < argc; i++) {
		Tcl_DStringAppend(&command, argv[i], strlen(argv[i]));		
		Tcl_DStringAppend(&command, " ", 1);
	}

	if (api_ex_exec(sp, Tcl_DStringValue(&command), 
			Tcl_DStringLength(&command))) {

		Tcl_SetResult(interp, "Could not execute your command.",
				TCL_STATIC);
		Tcl_DStringFree(&command);
		return TCL_ERROR;
	}
	Tcl_DStringFree(&command);
	return TCL_OK;
}

/*
 * Tcl Command: viViCmd
 * Usage: viViCmd
 *
 * Execute a vi command.
 *
 */

int 
tcl_vi_exec(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{

	int i;
	SCR *sp;
	Tcl_DString command;	

	sp = api_gscr();
	Tcl_DStringInit(&command);

	for (i = 1; i < argc; i++)
		Tcl_DStringAppend(&command, argv[i], strlen(argv[i]));		

	if (api_vi_exec(sp, Tcl_DStringValue(&command), 
			Tcl_DStringLength(&command))) {

		Tcl_SetResult(interp, "Could not execute your command.",
				TCL_STATIC);
		Tcl_DStringFree(&command);
		return TCL_ERROR;
	}
	Tcl_DStringFree(&command);
	return TCL_OK;

}

/*
 * Tcl Command: viGetCursor
 * Usage: viGetCursor [screen]
 *
 * Return the current cursor position as a list with two elements.
 * {line, column}.  If no scren is specified use the default.
 *
 */

int 
tcl_getcur(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	MARK cursor;
	SCR *sp;
	char line[32], column[4];

	if (argc < 1 || argc > 2) {
		Tcl_SetResult(interp, "Usage: viGetCursor [screen]",
			TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 1) {
		sp = api_gscr();
	} else {
		sp = (SCR *)atol(argv[1]);
	}

	if (api_getcur(sp, &cursor)) {
		Tcl_SetResult(interp, "Could not get cursor.",
			TCL_STATIC);
		return TCL_ERROR;
	}

	sprintf(line, "%d", cursor.lno);
	sprintf(column, "%d", cursor.cno);
	Tcl_AppendElement(interp, line);
	Tcl_AppendElement(interp, column);

	return TCL_OK;
}

/*
 * Tcl Command: viMsg
 * Usage: viMsg text
 * 
 * Set the message line to text.
 *
 */

int 
tcl_msg(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{

	SCR *sp;

	sp = api_gscr();

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: viMsg text", TCL_STATIC);
		return TCL_ERROR;
	}

	api_msgq(sp, M_INFO, argv[1]);

	return TCL_OK;
}

/*
 * Tcl Command: viSetCursor
 * Usage: viSetCursor [screen] line column
 *
 * Set the cursor to the line and column numbers supplied.  If only one
 * argument is supplied take it as a line number.  If no screen is 
 * supplied then use the default.
 *
 *
 */

int 
tcl_setcur(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	MARK cursor;
	SCR *sp;
	recno_t last;

	if ((argc < 3) || (argc > 4)) {
		Tcl_SetResult(interp, "Usage: viSetCursor line column",
				TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 3) {
		sp = api_gscr();
		cursor.lno = atoi(argv[1]);
		cursor.cno = atoi(argv[2]);
	} else {
		sp = (SCR *)atol(argv[1]);
		cursor.lno = atoi(argv[2]);
		cursor.cno = atoi(argv[3]);
	}

	if (api_lline(sp, &last)) {
		Tcl_SetResult(interp, "could not get last line", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (cursor.lno <= 0 || cursor.lno > last) {
		Tcl_SetResult(interp, "Error: line out of range.", TCL_STATIC);
		return (TCL_ERROR);
	}

	if (cursor.cno < 0) {
		Tcl_SetResult(interp, "Error: column out of range.", 
				TCL_STATIC);
		return (TCL_ERROR);
	}

	if (api_setcur(sp, cursor)) {
		Tcl_SetResult(interp, "Could not set cursor.",
				TCL_STATIC);
		return TCL_ERROR;
	}

	return TCL_OK;
}
		

/*
 * Tcl Command: viGetMark
 * Usage: viGetMark [screen] name
 *
 * Get the named mark.
 *
 */

int 
tcl_getmark(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	MARK mp;
	char line[32], column[4], markName;

	if (argc < 2 || argc > 3) {
		Tcl_SetResult(interp, "Usage: viGetMark [screen] name",
				TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 2) {
		sp = api_gscr();
		markName = argv[1][0];
	} else {
		sp = (SCR *)atol(argv[1]);
		markName = argv[2][0];
	}

	if (api_getmark(sp, markName, &mp)) {
		Tcl_SetResult(interp, "Cannot get mark.", TCL_STATIC);
		return TCL_ERROR;
	}

	sprintf(line, "%d", mp.lno);
	sprintf(column, "%d", mp.cno);

	Tcl_AppendElement(interp, line);
	Tcl_AppendElement(interp, column);

	return TCL_OK;
}

/*
 * Tcl Command: viSetMark
 * Usage: viSetMark [screen] name line column
 *
 * Set a named mark at the line and column supplied.  If no screen is 
 * supplied the default is used.
 *
 */

int 
tcl_setmark(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	MARK mp;
	CHAR_T markName;

	/* Make sure that the name of the mark is only one character. */
	if ((argc < 3) || (argc > 4)) {
		Tcl_SetResult(interp, 
				"Usage: viSetMark [scren] name line column", 
				TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 3) {
		sp = api_gscr();
		markName = argv[1][0];
		mp.lno = atoi(argv[2]);
		mp.cno = atoi(argv[3]);
	} else {
		sp = (SCR *)atol(argv[1]);
		markName = argv[2][0];
		mp.lno = atoi(argv[3]);
		mp.cno = atoi(argv[4]);
	}

	if (api_setmark(sp, markName, &mp)) {
		Tcl_SetResult(interp, "Could not set mark.",
				TCL_STATIC);
		return TCL_ERROR;
	}

	return TCL_OK;

}

/*
 * Tcl Command: viNewScreen
 * Usage: viNewScreen [file]
 *
 * Create a new screen.  If a filename is specified then the screen
 * is opened with that file.
 */

int 
tcl_iscreen(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{

	SCR *sp, *new;
	char screen[32], *file;

	if (argc < 1 || argc > 2) {
		Tcl_SetResult(interp, "Usage: viNewScreen [file]", TCL_STATIC);
		return TCL_ERROR;
	}

	sp = api_gscr();

	if (argc == 2)
		file = argv[1];
	else
		file = NULL;

	if (api_iscreen(sp, &screen, file)) {
		Tcl_SetResult(interp, "Could not create new screen.",
			TCL_STATIC);
		return TCL_ERROR;
	}

	Tcl_SetResult(interp, screen, TCL_VOLATILE);
	return (TCL_OK);
}

/*
 * Tcl Command: viEndScreen 
 * Usage: viEndScreen screenObject
 *
 * End a screen.
 */

int 
tcl_escreen(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	int i;
	SCR *new, *scr;

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: viEndScreen screenObj",
				TCL_STATIC);
		return TCL_ERROR;
	}

	scr = (SCR *)atol(argv[1]);

	if (api_escreen(scr)) {
		Tcl_SetResult(interp, "Could not delete screen.", 
				TCL_STATIC);
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 * Tcl Command: viSwitchScreen
 *
 * Usage: viSwitchScreen screen
 *
 * Change the current focus to the screen passed as an argument.
 *
 */
int
tcl_swscreen(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *current, *new;

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: viSwitchScreen screen", 
			TCL_STATIC);
		return (TCL_ERROR);
	}

	current = (SCR *)api_gscr();

	new = (SCR *)atol(argv[1]);

	if (api_swscreen(current, new)) {
		Tcl_SetResult(interp, "Error: Could not switch screens.",
			TCL_STATIC);
		return (TCL_ERROR);
	}

	return (TCL_OK);
}

/*
 * Tcl Command: viFindScreen
 *
 * Usage: viFindScreen file
 *
 * Return the screen associated with a file.
 *
 */

int
tcl_fscreen(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	char screen[32];

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: viFindScreen file", TCL_STATIC);
		return TCL_ERROR;
	}

	if (api_fscreen(api_gscr(), argv[1], &screen)) {
		Tcl_SetResult(interp, "Could not find screen.",
		    TCL_STATIC);
		return (TCL_ERROR);
	}

	Tcl_SetResult(interp, screen, TCL_VOLATILE);
	return (TCL_OK);
}

/*
 * Tcl Command: viMapKey
 * Usage: viMapKey key tclproc
 *
 * Associate a key with a tcl procedure.
 */

int 
tcl_map(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	char command[256];

	if (argc != 3) {
		Tcl_SetResult(interp, "Usage: viMapKey key tclproc",
				TCL_STATIC);
		return TCL_ERROR;
	}

	sp = api_gscr();

	strcpy(command, ":tcl ");
	strcat(command, argv[2]);
	strcat(command, "\n");

	if (api_map(sp, argv[1], command)) {
		Tcl_SetResult(interp, "Could not remap key",
				TCL_STATIC);
		return TCL_ERROR;
	}

	return TCL_OK;

}

/*
 * Tcl Command: viUnmapKey
 * Usage: viUnmMapKey key 
 *
 * Dissociate a key from a tcl procedure.
 */

int 
tcl_unmap(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: viUnmapKey key",
				TCL_STATIC);
		return TCL_ERROR;
	}

	sp = api_gscr();

	if (api_unmap(sp, argv[1])) {
		Tcl_SetResult(interp, "Could not unmap key",
				TCL_STATIC);
		return TCL_ERROR;
	}

	return TCL_OK;

}

/*
 * Tcl Command: viSetOpt
 * Usage: viSetOpt value
 *
 * Set an option.
 */

int 
tcl_opts_set(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: viSetOpt value",
				TCL_STATIC);
		return TCL_ERROR;
	}

	sp = api_gscr();

	if (api_opts_set(sp, argv[1])) {
		Tcl_SetResult(interp, "Could not set option.",
				TCL_STATIC);
		return TCL_ERROR;
	}

	return TCL_OK;

}

/*
 * Tcl Command: viGetOpt
 * Usage: viGetOpt name 
 *
 * Return the value of an option.
 */

int 
tcl_opt_get(clientData, interp, argc, argv)
	ClientData clientData;
	Tcl_Interp *interp;
	int argc;
	char **argv;
{
	SCR *sp;
	CHAR_T *value;

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: viGetOpt name",
				TCL_STATIC);
		return TCL_ERROR;
	}

	sp = api_gscr();

	if (api_opt_get(sp, argv[1], &value)) {
		Tcl_SetResult(interp, "Option not found.",
				TCL_STATIC);
		return TCL_ERROR;
	} else {
		Tcl_SetResult(interp, value, TCL_DYNAMIC);
		free(value);
		return TCL_OK;
	}

}

/* The following is the generic routine that creates the TCL */
/* commands used by nvi. */

int 
Tcl_AppInit(interp) 
	Tcl_Interp *interp;	
{
	if (Tcl_Init(interp) == TCL_ERROR) 
		return TCL_ERROR;

	Tcl_CreateCommand(interp, "viDelLine", tcl_dline,
			    (ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viLastLine", tcl_lline,
			    (ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viAppendLine", tcl_aline,
			    (ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viInsertLine", tcl_iline,
			    (ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viGetLine", tcl_gline,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viSetLine", tcl_sline,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viSetCursor", tcl_setcur,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viGetCursor", tcl_getcur,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viExCmd", tcl_ex_exec,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viGetMark", tcl_getmark,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viSetMark", tcl_setmark,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viMsg", tcl_msg,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viNewScreen", tcl_iscreen,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viEndScreen", tcl_escreen,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viFindScreen", tcl_fscreen,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viSwitchScreen", tcl_swscreen,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viMapKey", tcl_map,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viUnmapKey", tcl_unmap,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viSetOpt", tcl_opts_set,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "viGetOpt", tcl_opt_get,
			(ClientData) NULL, (Tcl_CmdDeleteProc *)NULL);

	return TCL_OK;
}
