/* This file contains a new version of the system() function and related stuff.
 *
 * Entry points are:
 *	filter(m,n,cmd)	- run text lines through a filter program
 *
 * This is probably the single least portable file in the program.  The code
 * shown here should work correctly if it links at all; it will work on UNIX
 * and any O.S./Compiler combination which adheres to UNIX forking conventions.
 */

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "pathnames.h"
#include "extern.h"

extern char	**environ;

static int rpipe __P((char *, int));

/*
 * This private function opens a pipe from a filter.  It is similar to the
 * system() function, and to popen(cmd, "r").
 */
int
rpipe(cmd, in)
	char *cmd;		/* The filter command to use. */
	int in;			/* The fd to use for stdin. */
{
	int	r0w1[2];/* the pipe fd's */

	/* make the pipe */
	if (pipe(r0w1) < 0)
	{
		return -1;	/* pipe failed */
	}

	/* The parent process (elvis) ignores signals while the filter runs.
	 * The child (the filter program) will reset this, so that it can
	 * catch the signal.
	 */
	signal(SIGINT, SIG_IGN);

	switch (fork())
	{
	  case -1:						/* error */
		return -1;

	  case 0:						/* child */
		/* close the "read" end of the pipe */
		close(r0w1[0]);

		/* redirect stdout to go to the "write" end of the pipe */
		close(1);
		dup(r0w1[1]);
		close(2);
		dup(r0w1[1]);
		close(r0w1[1]);

		/* redirect stdin */
		if (in != 0)
		{
			close(0);
			dup(in);
			close(in);
		}

		/* the filter should accept SIGINT signals */
		signal(SIGINT, SIG_DFL);

		/* exec the shell to run the command */
		execle(PVAL(O_SHELL), PVAL(O_SHELL), "-c", cmd, NULL, environ);
		exit(1); /* if we get here, exec failed */

	  default:						/* parent */
		/* close the "write" end of the pipe */	
		close(r0w1[1]);

		return r0w1[0];
	}
}

/* This function closes the pipe opened by rpipe(), and returns 0 for success */
int rpclose(fd)
	int	fd;
{
	int	status;

	close(fd);
	wait(&status);
	signal(SIGINT, trapint);
	return status;
}

/* This function runs a range of lines through a filter program, and replaces
 * the original text with the filtered version.  As a special case, if "to"
 * is MARK_UNSET, then it runs the filter program with stdin coming from
 * /dev/null, and inserts any output lines.
 */
int filter(from, to, cmd)
	MARK	from, to;	/* the range of lines to filter */
	char	*cmd;		/* the filter command */
{
	CMDARG cmdarg;
	int	scratch;	/* fd of the scratch file */
	int	fd;		/* fd of the pipe from the filter */
	char	scrout[50];	/* name of the scratch out file */
	MARK	new;		/* place where new text should go */
	int	i;

	/* write the lines (if specified) to a temp file */
	if (to)
	{
		/* we have lines */
		(void)sprintf(scrout, _PATH_SCRATCH, PVAL(O_DIRECTORY));
		mktemp(scrout);
		SETCMDARG(cmdarg, C_WRITE, 2, from, to, 0, scrout);
		ex_write(&cmdarg);

		/* use those lines as stdin */
		scratch = open(scrout, O_RDONLY);
		if (scratch < 0)
		{
			unlink(scrout);
			return -1;
		}
	}
	else
	{
		scratch = 0;
	}

	/* start the filter program */
	fd = rpipe(cmd, scratch);
	if (fd < 0)
	{
		if (to)
		{
			close(scratch);
			unlink(scrout);
		}
		return -1;
	}

	ChangeText
	{
		/* adjust MARKs for whole lines, and set "new" */
		from &= ~(BLKSIZE - 1);
		if (to)
		{
			to &= ~(BLKSIZE - 1);
			to += BLKSIZE;
			new = to;
		}
		else
		{
			new = from + BLKSIZE;
		}

		/* repeatedly read in new text and add it */
		while ((i = read(fd, tmpblk.c, BLKSIZE - 1)) > 0)
		{
			tmpblk.c[i] = '\0';
			add(new, tmpblk.c);
			for (i = 0; tmpblk.c[i]; i++)
			{
				if (tmpblk.c[i] == '\n')
				{
					new = (new & ~(BLKSIZE - 1)) + BLKSIZE;
				}
				else
				{
					new++;
				}
			}
		}
	}

	/* delete old text, if any */
	if (to)
	{
		delete(from, to);
	}

	/* Reporting... */
	rptlabel = "more";
	if (rptlines < 0)
	{
		rptlines = -rptlines;
		rptlabel = "less";
	}

	/* cleanup */
	rpclose(fd);
	if (to)
	{
		close(scratch);
		unlink(scrout);
	}
	return 0;
}
