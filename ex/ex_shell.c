/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shell.c,v 10.14 1995/09/30 10:39:38 bostic Exp $ (Berkeley) $Date: 1995/09/30 10:39:38 $";
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
	char buf[MAXPATHLEN];

	(void)snprintf(buf, sizeof(buf), "%s -i", O_STR(sp, O_SHELL));
	return (ex_exec_proc(sp, cmdp, buf, NULL));
}

/*
 * ex_exec_proc --
 *	Run a separate process.
 *
 * PUBLIC: int ex_exec_proc __P((SCR *, EXCMD *, char *, char *));
 */
int
ex_exec_proc(sp, cmdp, cmd, msg)
	SCR *sp;
	EXCMD *cmdp;
	char *cmd, *msg;
{
	const char *name;
	pid_t pid;
	int nf;
	char *p;

	/* Flush messages and enter canonical mode. */
	if (F_ISSET(sp, S_VI)) {
		if (sp->gp->scr_screen(sp, S_EX)) {
			ex_message(sp, cmdp->cmd->name, EXM_NOCANON);
			return (1);
		}
		F_SET(sp, S_SCREEN_READY);
	}

	/* Put out additional message. */
	if (msg != NULL)
		(void)ex_puts(sp, msg);
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
		p = msg_print(sp, O_STR(sp, O_SHELL), &nf);
		msgq(sp, M_SYSERR, "execl: %s", p);
		if (nf)
			FREE_SPACE(sp, p, 0);
		_exit(127);
		/* NOTREACHED */
	default:			/* Parent. */
		return (proc_wait(sp, (long)pid, cmd, 0, 0));
	}
	/* NOTREACHED */
}
