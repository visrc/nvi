/*-
 * Copyright (c) 1996
 *	Rob Zimmermann.  All rights reserved.
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ip_run.c,v 8.1 1996/11/27 10:09:02 bostic Exp $ (Berkeley) $Date: 1996/11/27 10:09:02 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "../common/common.h"
#include "../ip_vi/ip.h"
#include "ipc_extern.h"
#include "pathnames.h"

static void arg_format __P((int *, char **[], int, int));
#ifdef DEBUG
static void attach __P((void));
#endif

/*
 * run_vi --
 *	Run the vi program.
 *
 * PUBLIC: int run_vi __P((char *, int, char *[], int *, int *));
 */
int
run_vi(name, argc, argv, ip, op)
	char *name;
	int argc, *ip, *op;
	char *argv[];
{
	struct stat sb;
	pid_t pid;
	int ch, rpipe[2], wpipe[2];

#ifdef DEBUG
	/* If the first argument is -D, pause and let the debugger attach. */
	if (argv[1] != NULL && strcmp(argv[1], "-D") == 0) {
		char **p_av, **t_av;

		attach();
		for (p_av = argv + 1, t_av = argv + 2;;)
			if ((*p_av++ = *t_av++) == NULL)
				break;
	}
#endif

	/*
	 * Open the communications channels.  The pipes are named from the
	 * parent's viewpoint, meaning we read from rpipe[0] and write to
	 * wpipe[1].  The vi process reads from wpipe[0], and it writes to
	 * rpipe[1].
	 */
	if (pipe(rpipe) == -1 || pipe(wpipe) == -1) {
		(void)fprintf(stderr, "%s: %s\n", name, strerror(errno));
		exit (1);
	}
	*ip = rpipe[0];
	*op = wpipe[1];

	/*
	 * Reformat our arguments, adding a -I to the list.  The first file
	 * descriptor for the -I argument is vi's input, and the second is
	 * vi's output.
	 */
	arg_format(&argc, &argv, wpipe[0], rpipe[1]);

	/* Run vi. */
	switch (pid = fork()) {
	case -1:				/* Error. */
		perror("ip_cl: fork");
		exit (1);
	case 0:					/* Vi. */
		/*
		 * If there's a local (debugging) nvi, run it, otherwise
		 * run the compiled in path.
		 */
		if (stat("nvi", &sb) == 0)
			execv("nvi", argv);
		execv(VI, argv);
		(void)fprintf(stderr, "%s: %s %s\n", name, VI, strerror(errno));
		exit (1);
	default:				/* Ip_cl. */
		break;
	}
	return (0);
}

/*
 * arg_format --
 *	Reformat our arguments to add the -I argument for vi.
 */
static void
arg_format(argcp, argvp, i_fd, o_fd)
	int *argcp, i_fd, o_fd;
	char **argvp[];
{
	char *iarg, **largv, *p, **p_av, **t_av;

	/* Get space for the argument array and the -I argument. */
	if ((iarg = malloc(64)) == NULL ||
	    (largv = malloc((*argcp + 3) * sizeof(char *))) == NULL) {
		perror("ip_cl");
		exit (1);
	}
	memcpy(largv + 2, *argvp, *argcp * sizeof(char *) + 1);

	/* Reset argv[0] to be the exec'd program. */
	if ((p = strrchr(VI, '/')) == NULL)
		largv[0] = VI;
	else
		largv[0] = p + 1;

	/* Create the -I argument. */
	(void)sprintf(iarg, "-I%d%s%d", i_fd, ".", o_fd);
	largv[1] = iarg;

	/* Copy any remaining arguments into the array. */
	for (p_av = (*argvp) + 1, t_av = largv + 2;;)
		if ((*t_av++ = *p_av++) == NULL)
			break;

	/* Reset the argument array. */
	*argvp = largv;
}

#ifdef DEBUG
/*
 * attach --
 *	Pause and let the user attach a debugger.
 */
static void
attach()
{
	int fd;
	char ch;

	(void)printf("process %lu waiting, enter <CR> to continue: ",
	    (u_long)getpid());
	(void)fflush(stdout);

	if ((fd = open(_PATH_TTY, O_RDONLY, 0)) < 0) {
		perror(_PATH_TTY);
		exit (1);;
	}
	do {
		if (read(fd, &ch, 1) != 1) {
			(void)close(fd);
			return;
		}
	} while (ch != '\n' && ch != '\r');
	(void)close(fd);
}
#endif /* DEBUG */

#ifdef TR
/*
 * trace --
 *	Debugging trace routine.
 *
 * PUBLIC: void trace __P((const char *, ...));
 */
void
#ifdef __STDC__
trace(const char *fmt, ...)
#else
trace(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	static FILE *tfp;
	va_list ap;

	if (tfp == NULL && (tfp = fopen(TR, "w")) == NULL)
		tfp = stderr;
	
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(tfp, fmt, ap);
	va_end(ap);

	(void)fflush(tfp);
}
#endif /* TR */
