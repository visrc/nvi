/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_filter.c,v 5.25 1993/02/24 12:47:03 bostic Exp $ (Berkeley) $Date: 1993/02/24 12:47:03 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "screen.h"

/*
 * filtercmd --
 *	Run a range of lines through a filter program and replace the
 *	original text with the stdout/stderr output of the filter.
 */
int
filtercmd(ep, fm, tm, rp, cmd, ftype)
	EXF *ep;
	MARK *fm, *tm, *rp;
	u_char *cmd;
	enum filtertype ftype;
{
	FILE *ifp, *ofp;			/* Can't be uninitialized. */
	pid_t pid;
	sig_ret_t intsave, quitsave;
	sigset_t bmask, omask;
	recno_t dlines, ilines, lno;
	int input[2], output[2], pstat, rval;
	char *name;

	/* Input and output are named from the child's point of view. */
	input[0] = input[1] = output[0] = output[1] = -1;

	/* Set default cursor position. */
	*rp = *fm;

	/*
	 * If child isn't supposed to want input or send output, redirect
	 * from or to /dev/null.  Otherwise open up the pipe and get a stdio
	 * buffer for it.
	 */
	if (ftype == NOINPUT) {
		if ((input[0] = open(_PATH_DEVNULL, O_RDONLY, 0)) < 0) {
			msg(ep, M_ERROR,
			    "filter: %s: %s", _PATH_DEVNULL, strerror(errno));
			goto err;
		}
	} else
		if (pipe(input) < 0 ||
		    (ifp = fdopen(input[1], "w")) == NULL) {
			msg(ep, M_ERROR, "filter: %s", strerror(errno));
			goto err;
		}

	if (ftype == NOOUTPUT) {
		if ((output[1] = open(_PATH_DEVNULL, O_WRONLY, 0)) < 0) {
			msg(ep, M_ERROR,
			    "filter: %s: %s", _PATH_DEVNULL, strerror(errno));
			goto err;
		}
	} else
		if (pipe(output) < 0 ||
		    (ofp = fdopen(output[0], "r")) == NULL) {
			msg(ep, M_ERROR, "filter: %s", strerror(errno));
			goto err;
		}

	sigemptyset(&bmask);
	sigaddset(&bmask, SIGCHLD);
	(void)sigprocmask(SIG_BLOCK, &bmask, &omask);

	switch (pid = vfork()) {
	case -1:			/* Error. */
		(void)sigprocmask(SIG_SETMASK, &omask, NULL);
		msg(ep, M_ERROR, "filter: %s", strerror(errno));
err:		if (input[0] != -1)
			(void)close(input[0]);
		if (input[0] != -1)
			(void)close(input[0]);
		if (output[0] != -1)
			(void)close(output[0]);
		if (output[1] != -1)
			(void)close(input[1]);
		return (1);
		/* NOTREACHED */
	case 0:				/* Child. */
		(void)sigprocmask(SIG_SETMASK, &omask, NULL);

		/*
		 * Redirect stdin from the read end of the input pipe,
		 * and redirect stdout/stderr to the write end of the
		 * output pipe.
		 */
		(void)dup2(input[0], STDIN_FILENO);
		(void)dup2(output[1], STDOUT_FILENO);
		(void)dup2(output[1], STDERR_FILENO);

		/* Close the child's pipe file descriptors. */
		(void)close(input[0]);
		if (ftype != NOINPUT)
			(void)close(input[1]);
		if (ftype != NOOUTPUT)
			(void)close(output[0]);
		(void)close(output[1]);

		if ((name = strrchr((char *)PVAL(O_SHELL), '/')) == NULL)
			name = (char *)PVAL(O_SHELL);
		else
			++name;
		execl((char *)PVAL(O_SHELL), name, "-c", cmd, NULL);
		msg(ep, M_ERROR,
		    "exec: %s: %s", PVAL(O_SHELL), strerror(errno));
		_exit (1);
		/* NOTREACHED */
	}

	/* Close the pipe ends the parent won't use. */
	(void)close(input[0]);
	(void)close(output[1]);

	/*
	 * Write the selected lines to the write end of the input pipe.
	 * Ifp is closed by ex_writefp.
	 */
	dlines = 0;
	rval = 0;
	if (ftype != NOINPUT)
		if (ex_writefp(ep, "filter", ifp, fm, tm, 0))
			rval = 1;
		else {
			/* Delete old text, if any. */
			for (lno = tm->lno; lno >= fm->lno; --lno)
				if (file_dline(ep, lno))
					rval = 1;
			dlines = tm->lno - fm->lno;
		}

	if (rval == 0) {
		/*
		 * Read the output from the read end of the output pipe.
		 * Decrement the line number, ex_readfp appends to the
		 * MARK.  Ofp is closed by ex_readfp.  Set the cursor to
		 * the first line read in.
		 */
		if (ftype != NOOUTPUT) {
			rp->lno = fm->lno;
			--fm->lno;
			rval = ex_readfp(ep, "filter", ofp, fm, &ilines);
		}

		/* Reporting. */
		if (ilines == dlines) {
			if (ilines != 0) {
				ep->rptlines = ilines;
				ep->rptlabel = "modified";
			}
		} else if (dlines == 0) {
			ep->rptlines = ilines;
			ep->rptlabel = "added";
		} else if (ilines == 0) {
			ep->rptlines = dlines;
			ep->rptlabel = "deleted";
		} else if (ilines > dlines) {
			ep->rptlines = ilines - dlines;
			ep->rptlabel = "added";
		} else {
			ep->rptlines = dlines - ilines;
			ep->rptlabel = "deleted";
		}
	}
			
	/* Wait for the child to finish. */
	intsave = signal(SIGINT, SIG_IGN);
	quitsave = signal(SIGQUIT, SIG_IGN);
	(void)waitpid(pid, &pstat, 0);
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	(void)signal(SIGINT, intsave);
	(void)signal(SIGQUIT, quitsave);

	if (WIFSIGNALED(pstat)) {
		msg(ep, M_DISPLAY,
		    "filter: exited with signal %d%s.", WTERMSIG(pstat),
		    WCOREDUMP(pstat) ? "; core dumped" : "");
		return (1);
	}
	else if (WIFEXITED(pstat) && WEXITSTATUS(pstat)) {
		msg(ep, M_DISPLAY,
		    "filter: exited with status %d.", WEXITSTATUS(pstat));
		return (1);
	}
	return (0);
}
