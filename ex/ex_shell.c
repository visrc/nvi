/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shell.c,v 10.8 1995/07/06 11:50:20 bostic Exp $ (Berkeley) $Date: 1995/07/06 11:50:20 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"

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
	char buf[MAXPATHLEN];

	(void)snprintf(buf, sizeof(buf), "%s -i", O_STR(sp, O_SHELL));
	return (ex_exec_proc(sp, cmdp, buf, "\n", NULL));
}

/*
 * ex_exec_proc --
 *	Run a separate process.
 *
 * PUBLIC: int ex_exec_proc __P((SCR *, EXCMD *, char *, char *, char *));
 */
int
ex_exec_proc(sp, cmdp, cmd, p1, p2)
	SCR *sp;
	EXCMD *cmdp;
	char *cmd, *p1, *p2;
{
	const char *name;
	pid_t pid;
	int nf, rval;
	char *p;

	/* Flush messages and enter canonical mode. */
	if (sp->gp->scr_canon(sp, 1)) {
		ex_message(sp, cmdp->cmd->name, EXM_NOCANON);
		return (1);
	}

	/* Put out special messages. */
	if (p1 != NULL || p2 != NULL) {
		if (p1 != NULL)
			ex_puts(sp, p1);
		if (p2 != NULL)
			ex_puts(sp, p2);
	}

	switch (pid = vfork()) {
	case -1:			/* Error. */
		msgq(sp, M_SYSERR, "vfork");
		rval = 1;
		break;
	case 0:				/* Utility. */
		if ((name = strrchr(O_STR(sp, O_SHELL), '/')) == NULL)
			name = O_STR(sp, O_SHELL);
		else
			++name;
		execl(O_STR(sp, O_SHELL), name, "-c", cmd, NULL);
		p = msg_print(sp, O_STR(sp, O_SHELL), &nf);
		msgq(sp, M_SYSERR, "execl: %s", p);
		if (nf)
			FREE_SPACE(sp, p, 0);
		_exit(127);
		/* NOTREACHED */
	default:			/* Parent. */
		rval = proc_wait(sp, (long)pid, cmd, 0);
		break;
	}
	return (rval);
}
