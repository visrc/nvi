/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shell.c,v 9.3 1994/11/10 16:22:35 bostic Exp $ (Berkeley) $Date: 1994/11/10 16:22:35 $";
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

#include "vi.h"
#include "excmd.h"
#include "../sex/sex_screen.h"
#include "../svi/svi_screen.h"

/*
 * ex_shell -- :sh[ell]
 *	Invoke the program named in the SHELL environment variable
 *	with the argument -i.
 */
int
ex_shell(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	char buf[MAXPATHLEN];

	(void)snprintf(buf, sizeof(buf), "%s -i", O_STR(sp, O_SHELL));
	return (ex_exec_proc(sp, buf, "\n", NULL));
}

/*
 * ex_exec_proc --
 *	Run a separate process.
 */
int
ex_exec_proc(sp, cmd, p1, p2)
	SCR *sp;
	char *cmd, *p1, *p2;
{
	const char *name;
	pid_t pid;
	int nf, rval, teardown;
	char *p;

	/* Clear the rest of the screen. */
	if (sp->s_clear(sp))
		return (1);

	/* Save ex/vi terminal settings, and restore the original ones. */
	teardown = !ex_sleave(sp);

	/*
	 * Flush waiting messages (autowrite, for example) so the output
	 * matches historic practice.
	 */
	(void)sex_refresh(sp);

	/* Put out various messages. */
	if (p1 != NULL)
		(void)write(STDOUT_FILENO, p1, strlen(p1));
	if (p2 != NULL)
		(void)write(STDOUT_FILENO, p2, strlen(p2));

	SIGBLOCK(sp->gp);
	switch (pid = vfork()) {
	case -1:			/* Error. */
		SIGUNBLOCK(sp->gp);

		msgq(sp, M_SYSERR, "vfork");
		rval = 1;
		break;
	case 0:				/* Utility. */
		/* The utility has default signal behavior. */
		sig_end();

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
		SIGUNBLOCK(sp->gp);

		rval = proc_wait(sp, (long)pid, cmd, 0);
		break;
	}

	/* Restore ex/vi terminal settings. */
	if (teardown)
		ex_rleave(sp);

	/*
	 * XXX
	 * Stat of the tty structures (see ex_sleave, ex_rleave) only give
	 * us 1-second resolution on the tty changes.  A fast '!' command,
	 * e.g. ":!pwd" can beat us to the refresh.  When there's better
	 * resolution from the stat(2) timers, this can and should go away,
	 * we're repainting the screen unnecessarily.
	 */
	F_SET(sp, S_SCR_REFRESH);

	return (rval);
}
