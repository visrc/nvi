/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_tcl.c,v 8.1 1995/11/11 12:44:33 bostic Exp $ (Berkeley) $Date: 1995/11/11 12:44:33 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#ifdef TCL_INTERP
#include <tcl.h>
#endif
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"

/* 
 * ex_tcl -- :[line [,line]] tcl [command]
 *	Run a command through the tcl interpreter.
 *
 * PUBLIC: int ex_tcl __P((SCR*, EXCMD *));
 */
int 
ex_tcl(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
#ifndef TCL_INTERP
	msgq(sp, M_ERR, "302|Vi was not loaded with a Tcl interpreter");
	return (1);
#else
	Tcl_DString cmd;
	recno_t lno;
	size_t len;
	int result;
	char *lp;

	/*
	 * Set the current screen pointer for interpretation to the screen
	 * pointer that we were called with.
	 *
	 * XXX
	 * This is fairly awful, but I haven't thought of a better answer.
	 */
	sp->gp->interpScr = sp;

	/*
	 * This is the case where we have two addresses to mark out the code
	 * to execute.  Ignore any arguments to the tcl command.
	 */
	if (cmdp->addrcnt != 0) {
		Tcl_DStringInit(&cmd);
		for (lno = cmdp->addr1.lno; lno <= cmdp->addr2.lno; lno++) {
			if (db_get(sp, lno, DBG_FATAL, &lp, &len)) {
				result = TCL_ERR;
				break;
			}
			Tcl_DStringAppend(&cmd, lp, len);

/* Cast the interpreter cookie to get to the Tcl result. */
#define	RESULTP	((Tcl_Interp *)sp->gp->interp)->result

			/* 
			 * We don't get a \n from db_get so we must supply it
			 * or multiple commands within a loop (or any block)
			 * will be unhappy.
			 */
			Tcl_DStringAppend(&cmd, "\n", 1);
			if (Tcl_CommandComplete(Tcl_DStringValue(&cmd))) {
				result = Tcl_Eval(sp->gp->interp,
				    Tcl_DStringValue(&cmd));
				if (result != TCL_OK) {
					msgq(sp,M_ERR, "Tcl: %s", RESULTP);
					break;
				}
				Tcl_DStringInit(&cmd);
			}
		}
		Tcl_DStringFree(&cmd);
	} else
		result = Tcl_Eval(sp->gp->interp, cmdp->argv[0]->bp);

	/*
	 * Unset the interpreter's screen pointer.
	 *
	 * XXX
	 * This is fairly awful, but I haven't thought of a better answer.
	 */
	sp->gp->interpScr = NULL;

	if (result == TCL_OK)
		return (0);

	msgq(sp,M_ERR, "Tcl: %s", RESULTP);
	return (1);
#endif /* TCL_INTERP */
}
