/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shell.c,v 8.10 1993/11/12 16:43:09 bostic Exp $ (Berkeley) $Date: 1993/11/12 16:43:09 $";
#endif /* not lint */

#include <sys/param.h>

#include <curses.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "../svi/svi_screen.h"

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
	return (ex_exec_proc(sp, O_STR(sp, O_SHELL), buf, "\n", NULL));
}

/*
 * ex_exec_proc --
 *	Run a separate process.
 */
int
ex_exec_proc(sp, shell, cmd, p1, p2)
	SCR *sp;
	const u_char *shell, *cmd;
	char *p1, *p2;
{
	struct sigaction act, oact;
	struct termios term;
	const char *name;
	pid_t pid;
	int isig, rval;

	/* Clear the rest of the screen. */
	if (F_ISSET(sp, S_MODE_VI) && sp->s_clear(sp))
		return (1);

	/*
	 * Save ex/vi terminal settings, and restore the original ones.
	 *
	 * The old terminal values almost certainly turn on VINTR, VQUIT and
	 * VSUSP.  We don't want to interrupt the parent(s), so we ignore
	 * VINTR.  VQUIT is ignored by main() because nvi never wants to catch
	 * it.  A handler for VSUSP should have been installed by the screen
	 * code.
	 */
	if (F_ISSET(sp->gp, G_ISFROMTTY)) {
		act.sa_handler = SIG_IGN;;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		if (isig = !sigaction(SIGINT, &act, &oact)) {
			if (tcgetattr(STDIN_FILENO, &term)) {
				msgq(sp, M_ERR,
				    "Error: tcgetattr: %s", strerror(errno));
				rval = 1;
				goto ret;
			}
			if (tcsetattr(STDIN_FILENO,
			    TCSADRAIN, &sp->gp->original_termios)) {
				msgq(sp, M_ERR,
				    "Error: tcsetattr: %s", strerror(errno));
				rval = 1;
				goto ret;
			}
		}
	}

	/* Put out various messages. */
	if (p1 != NULL)
		(void)write(STDOUT_FILENO, p1, strlen(p1));
	if (p2 != NULL)
		(void)write(STDOUT_FILENO, p2, strlen(p2));


	switch (pid = vfork()) {
	case -1:			/* Error. */
		msgq(sp, M_ERR, "vfork: %s", strerror(errno));
		rval = 1;
		goto ret;
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
ret:	if (F_ISSET(sp->gp, G_ISFROMTTY) && isig) {
		if (sigaction(SIGINT, &oact, NULL)) {
			msgq(sp, M_ERR, "Error: signal: %s", strerror(errno));
			rval = 1;
		}
		if (tcsetattr(STDIN_FILENO, TCSANOW | TCSASOFT, &term)) {
			msgq(sp, M_ERR,
			    "Error: tcsetattr: %s", strerror(errno));
			rval = 1;
		}
	}

	/* If in vi mode, redraw. */
	if (F_ISSET(sp, S_MODE_VI))
		F_SET(sp, S_REDRAW);

	return (rval);
}
