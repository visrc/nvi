/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shell.c,v 8.8 1993/11/03 17:18:48 bostic Exp $ (Berkeley) $Date: 1993/11/03 17:18:48 $";
#endif /* not lint */

#include <sys/param.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_shell -- :sh[ell]
 *	Invoke the program named in the SHELL environment variable
 *	with the argument -i.
 */
int
ex_shell(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	char buf[MAXPATHLEN];

	(void)snprintf(buf, sizeof(buf), "%s -i", O_STR(sp, O_SHELL));
	return (ex_exec_proc(sp, O_STR(sp, O_SHELL), buf, "\n"));
}

/*
 * ex_exec_proc --
 *	Run a separate process.
 */
int
ex_exec_proc(sp, shell, cmd, msg)
	SCR *sp;
	const u_char *shell, *cmd;
	char *msg;
{
	const char *name;
	struct termios t;
	pid_t pid;
	int rval;

	/* Save ex/vi terminal settings, and restore the original ones. */
	if (F_ISSET(sp->gp, G_ISFROMTTY)) {
		if (tcgetattr(STDIN_FILENO, &t)) {
			msgq(sp, M_ERR,
			    "Error: tcgetattr: %s", strerror(errno));
			return (1);
		}
		if (tcsetattr(STDIN_FILENO,
		    TCSADRAIN, &sp->gp->original_termios)) {
			msgq(sp, M_ERR,
			    "Error: tcsetattr: %s", strerror(errno));
			return (1);
		}
	}

	(void)write(STDOUT_FILENO, msg, strlen(msg));

	switch (pid = vfork()) {
	case -1:			/* Error. */
		msgq(sp, M_ERR, "vfork: %s", strerror(errno));
		return (1);
	case 0:				/* Utility. */
		/*
		 * The utility has default signal behavior.  Don't bother
		 * using sigaction(2) 'cause we want the default behavior.
		 */
		(void)signal(SIGINT, SIG_DFL);
		(void)signal(SIGQUIT, SIG_DFL);

		if ((name = strrchr((char *)shell, '/')) == NULL)
			name = (char *)shell;
		else
			++name;
		execl((char *)shell, name, "-c", cmd, NULL);
		msgq(sp, M_ERR,
		    "Error: execl: %s: %s", shell, strerror(errno));
		_exit(127);
		/* NOTREACHED */
	}

	rval = proc_wait(sp, (long)pid, cmd, 0);

	/* Restore ex/vi terminal settings. */
	if (F_ISSET(sp->gp, G_ISFROMTTY))
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &t)) {
			msgq(sp, M_ERR,
			    "Error: tcsetattr: %s", strerror(errno));
			rval = 1;
		}
	return (rval);
}
