/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_filter.c,v 5.8 1992/04/16 09:50:25 bostic Exp $ (Berkeley) $Date: 1992/04/16 09:50:25 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "options.h"
#include "extern.h"

/*
 * filter --
 *	Run a range of lines through a filter program and replace the original
 *	text with the stdout/stderr output of the filter.  One special case,
 *	if "to" is MARK_UNSET, then it runs the filter program with stdin
 *	coming from /dev/null, and inserts any output lines.
 */
int
filter(from, to, cmd)
	MARK from, to;
	char *cmd;
{
	FILE *fp;
	MARK new;
	pid_t pid;
	sig_t intsave, quitsave;
	sigset_t omask;
	union wait pstat;
	int i, input[2], output[2];
	char *name;

	if (pipe(input) < 0)
		goto err1;
	if ((fp = fdopen(input[1], "w")) == NULL || pipe(output) < 0)
		goto err2;

	omask = sigblock(sigmask(SIGCHLD));
	switch (pid = vfork()) {
	case -1:			/* Error. */
		(void)sigsetmask(omask);
		(void)close(output[0]);
		(void)close(output[1]);
err2:		(void)close(input[0]);
		(void)close(input[1]);
err1:		msg("filter: %s", strerror(errno));
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
		(void)close(input[1]);
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
	 * Close to flush when done.
	 */
	if (to != MARK_UNSET)
		ex_writerange("filter", fp, from, to, 0);
	(void)fclose(fp);
		
	ChangeText
	{
		/* Adjust MARKs for whole lines, and set "new". */
		from &= ~(BLKSIZE - 1);
		if (to) {
			to &= ~(BLKSIZE - 1);
			to += BLKSIZE;
			new = to;
		} else
			new = from + BLKSIZE;

		/* Repeatedly read in new text and add it. */
		while ((i = read(output[0], tmpblk.c, BLKSIZE - 1)) > 0) {
			tmpblk.c[i] = '\0';
			add(new, tmpblk.c);
			for (i = 0; tmpblk.c[i]; i++)
				if (tmpblk.c[i] == '\n')
					new = (new & ~(BLKSIZE - 1)) + BLKSIZE;
				else
					++new;
		}
		if (i < 0) {
			msg("filter: read: %s.", strerror(errno));
			(void)close(output[0]);
			return (1);
		}
	}

	/* Close the read end of the output pipe. */
	(void)close(output[0]);

	/* Wait for the child to finish. */
	intsave = signal(SIGINT, SIG_IGN);
	quitsave = signal(SIGQUIT, SIG_IGN);
	(void)waitpid(pid, (int *)&pstat, 0);
	(void)sigsetmask(omask);
	(void)signal(SIGINT, intsave);
	(void)signal(SIGQUIT, quitsave);

	/* Delete old text, if any. */
	if (to != MARK_UNSET)
		delete(from, to);

	/*
	 * Reporting...
	 * XXX
	 * This is wrong at the moment.
	 */
	if (rptlines < 0) {
		rptlines = -rptlines;
		rptlabel = "less";
	} else
		rptlabel = "more";

	if (WIFSIGNALED(pstat))
		msg("filter: exited with signal %d%s.", WTERMSIG(pstat),
		    WCOREDUMP(pstat) ? "; core dumped" : "");
	else if (WIFEXITED(pstat) && WEXITSTATUS(pstat))
		msg("filter: exited with status %d.", WEXITSTATUS(pstat));
	return (0);
}
