/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shell.c,v 10.26 1995/11/25 11:19:47 bostic Exp $ (Berkeley) $Date: 1995/11/25 11:19:47 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"

/*
 * ex_shell -- :sh[ell]
 *	Invoke the program named in the SHELL environment variable
 *	with the argument -i.
 *
 * PUBLIC: int ex_shell __P((SCR *, EXCMD *));
 */
int
ex_shell(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	int rval;
	char buf[MAXPATHLEN];

	(void)snprintf(buf, sizeof(buf), "%s -i", O_STR(sp, O_SHELL));

	/* If we're stil in a vi screen, move out explicitly. */
	rval = ex_exec_proc(sp, cmdp, buf, NULL, !F_ISSET(sp, S_SCR_EXWROTE));

	/*
	 * !!!
	 * Historically, vi didn't require a continue message after the
	 * return of the shell.  Match it.
	 */
	F_SET(sp, S_EX_DONTWAIT);

	return (rval);
}

/*
 * ex_exec_proc --
 *	Run a separate process.
 *
 * PUBLIC: int ex_exec_proc __P((SCR *, EXCMD *, char *, const char *, int));
 */
int
ex_exec_proc(sp, cmdp, cmd, msg, need_newline)
	SCR *sp;
	EXCMD *cmdp;
	char *cmd;
	const char *msg;
	int need_newline;
{
	const char *name;
	pid_t pid;

	/* Enter ex mode. */
	if (F_ISSET(sp, S_VI)) {
		if (sp->gp->scr_screen(sp, S_EX)) {
			ex_emsg(sp, cmdp->cmd->name, EXM_NOCANON);
			return (1);
		}
		F_SET(sp, S_SCR_EX | S_SCR_EXWROTE);
	}

	/* Put out additional newline, message. */
	if (need_newline)
		(void)ex_puts(sp, "\n");
	if (msg != NULL) {
		(void)ex_puts(sp, msg);
		(void)ex_puts(sp, "\n");
	}
	(void)ex_fflush(sp);

	switch (pid = vfork()) {
	case -1:			/* Error. */
		msgq(sp, M_SYSERR, "vfork");
		return (1);
	case 0:				/* Utility. */
		if ((name = strrchr(O_STR(sp, O_SHELL), '/')) == NULL)
			name = O_STR(sp, O_SHELL);
		else
			++name;
		execl(O_STR(sp, O_SHELL), name, "-c", cmd, NULL);
		msgq_str(sp, M_SYSERR, O_STR(sp, O_SHELL), "execl: %s");
		_exit(127);
		/* NOTREACHED */
	default:			/* Parent. */
		return (proc_wait(sp, (long)pid, cmd, 0, 0));
	}
	/* NOTREACHED */
}
