/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_filter.c,v 5.12 1992/05/04 09:25:05 bostic Exp $ (Berkeley) $Date: 1992/05/04 09:25:05 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <paths.h>

#include "vi.h"
#include "options.h"
#include "excmd.h"
#include "extern.h"

/*
 * filter --
 *	Run a range of lines through a filter program and replace the original
 *	text with the stdout/stderr output of the filter.
 */
int
filter(from, to, cmd, ftype)
	MARK from, to;
	char *cmd;
	enum filtertype ftype;
{
	union wait pstat;
	FILE *ifp, *ofp;
	pid_t pid;
	sig_t intsave, quitsave;
	sigset_t omask;
	long dlines, ilines;
	int input[2], output[2], rval;
	char *name;

	/* Input and output are named from the child's point of view. */
	input[0] = input[1] = output[0] = output[1] = -1;

	/*
	 * If child isn't supposed to want input or send output, redirect
	 * from or to /dev/null.  Otherwise open up the pipe and get a stdio
	 * buffer for it.
	 */
	if (ftype == NOINPUT) {
		if ((input[0] = open(_PATH_DEVNULL, O_RDONLY, 0)) < 0) {
			msg("filter: %s: %s", _PATH_DEVNULL, strerror(errno));
			goto err;
		}
	} else
		if (pipe(input) < 0 ||
		    (ifp = fdopen(input[1], "w")) == NULL) {
			msg("filter: %s", strerror(errno));
			goto err;
		}

	if (ftype == NOOUTPUT) {
		if ((output[1] = open(_PATH_DEVNULL, O_WRONLY, 0)) < 0) {
			msg("filter: %s: %s", _PATH_DEVNULL, strerror(errno));
			goto err;
		}
	} else
		if (pipe(output) < 0 ||
		    (ofp = fdopen(output[0], "r")) == NULL) {
			msg("filter: %s", strerror(errno));
			goto err;
		}

	omask = sigblock(sigmask(SIGCHLD));
	switch (pid = vfork()) {
	case -1:			/* Error. */
		(void)sigsetmask(omask);
		msg("filter: %s", strerror(errno));
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
		(void)sigsetmask(omask);

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

		if ((name = rindex(PVAL(O_SHELL), '/')) == NULL)
			name = PVAL(O_SHELL);
		else
			++name;
		execl(PVAL(O_SHELL), name, "-c", cmd, NULL);
		msg("exec: %s: %s", PVAL(O_SHELL), strerror(errno));
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
	rval = ftype != NOINPUT ? ex_writefp("filter", ifp, from, to, 0) : 0;

	/*
 	 * Read the output from the read end of the outupt pipe.
	 * Ofp is closed by ex_readfp.
	 */
	if (rval == 0 && ftype != NOOUTPUT)
		rval = ex_readfp("filter", ofp, from, &ilines);

	if (rval == 0) {
		/* Delete old text, if any. */
		if (ftype != NOINPUT) {
			delete(from, to);		/* XXX check error. */
			dlines = markline(to) - markline(from);
		} else
			dlines = 0;

		/* Reporting. */
		if (ilines == dlines) {
			if (ilines != 0) {
				rptlines = ilines;
				rptlabel = "modified";
			}
		} else if (dlines == 0) {
			rptlines = ilines;
			rptlabel = "added";
		} else if (ilines == 0) {
			rptlines = dlines;
			rptlabel = "deleted";
		} else if (ilines > dlines) {
			rptlines = ilines - dlines;
			rptlabel = "added";
		} else {
			rptlines = dlines - ilines;
			rptlabel = "deleted";
		}
	}
			
	/* Wait for the child to finish. */
	intsave = signal(SIGINT, SIG_IGN);
	quitsave = signal(SIGQUIT, SIG_IGN);
	(void)waitpid(pid, (int *)&pstat, 0);
	(void)sigsetmask(omask);
	(void)signal(SIGINT, intsave);
	(void)signal(SIGQUIT, quitsave);

	if (WIFSIGNALED(pstat)) {
		msg("filter: exited with signal %d%s.", WTERMSIG(pstat),
		    WCOREDUMP(pstat) ? "; core dumped" : "");
		return (1);
	}
	else if (WIFEXITED(pstat) && WEXITSTATUS(pstat)) {
		msg("filter: exited with status %d.", WEXITSTATUS(pstat));
		return (1);
	}
	return (0);
}
