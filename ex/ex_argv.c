/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_argv.c,v 8.11 1993/10/06 17:18:20 bostic Exp $ (Berkeley) $Date: 1993/10/06 17:18:20 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

static int ex_pipe_process __P((SCR *, char *, size_t *, char *, size_t));

#define	SHELLECHO	"echo "
#define	SHELLOFFSET	(sizeof(SHELLECHO) - 1)

/* Structure for building argc/argv vector of ex arguments. */
typedef struct _args {
	char	*bp;			/* Buffer. */
	size_t	 len;			/* Buffer length. */

#define	A_ALLOCATED	0x01		/* If allocated space. */
	u_char	 flags;
} ARGS;

#define	ARGP	((ARGS *)(sp->args))
					/* Screen positions. */
/*
 * file_argv --
 *	Do file name and shell expansion on a string, then
 *	break it up into an argv.
 */
int
file_argv(sp, ep, s, argcp, argvp)
	SCR *sp;
	EXF *ep;
	char *s, ***argvp;
	int *argcp;
{
	size_t len, blen, tlen;
	int rval;
	char *bp, *p, *t;

	GET_SPACE(sp, bp, blen, 512);

	memmove(bp, SHELLECHO, SHELLOFFSET);
	p = bp + SHELLOFFSET;
	len = SHELLOFFSET;

#if defined(DEBUG) && 0
	TRACE(sp, "file_argv: {%s}\n", s);
#endif

	/* Replace file name characters. */
	for (; *s; ++s)
		switch (*s) {
		case '%':
			if (F_ISSET(sp->frp, FR_NONAME)) {
				msgq(sp, M_ERR,
				    "No filename to substitute for %%.");
				return (1);
			}
			len += sp->frp->nlen;
			ADD_SPACE(sp, bp, blen, len);
			memmove(p, sp->frp->fname, sp->frp->nlen);
			p += sp->frp->nlen;
			break;
		case '#':
			if (sp->alt_fname != NULL)
				tlen = strlen(t = sp->alt_fname);
			else if (sp->p_frp != NULL &&
			     !F_ISSET(sp->p_frp, FR_NONAME)) {
				t = sp->p_frp->fname;
				tlen = sp->p_frp->nlen;
			} else {
				msgq(sp, M_ERR,
				    "No filename to substitute for #.");
				return (1);
			}
			len += tlen;
			ADD_SPACE(sp, bp, blen, len);
			memmove(p, t, tlen);
			p += tlen;
			break;
		case '\\':
			/*
			 * QUOTING NOTE:
			 *
			 * Strip backslashes that protected the file
			 * expansion characters.
			 */
			if (s[1] == '%' || s[1] == '#')
				++s;
			/* FALLTHROUGH */
		default:
			++len;
			ADD_SPACE(sp, bp, blen, len);
			*p++ = *s;
		}

	/* Nul termination. */
	ADD_SPACE(sp, bp, blen, len + 1);
	*p = '\0';

#if defined(DEBUG) && 0
	TRACE(sp, "pre-shell: {%s}\n", bp);
#endif
	/*
	 * Do shell word expansion -- it's very, very hard to figure out
	 * what magic characters the user's shell expects.  If it's not
	 * pure vanilla, don't even try.
	 */
	for (p = bp; *p; ++p)
		if (!isalnum(*p) && !isblank(*p) && *p != '/' && *p != '.')
			break;
	if (*p) {
		if (ex_pipe_process(sp, bp, &len, bp, blen))
			return (1);
		p = bp;
	} else
		p = bp + SHELLOFFSET;

#if defined(DEBUG) && 0
	TRACE(sp, "post-shell: {%s}\n", bp);
#endif

	rval = word_argv(sp, ep, p, argcp, argvp);

	FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * word_argv --
 *	Build an argv from a string.
 */
int
word_argv(sp, ep, p, argcp, argvp)
	SCR *sp;
	EXF *ep;
	char *p, ***argvp;
	int *argcp;
{
	size_t len;
	int cnt, done, off;
	char *ap, *t;

	for (done = off = 0;; ++p) {
		/* Skip any leading whitespace. */
		for (; isblank(p[0]); ++p);
		if (*p == '\0')
			break;

		/*
		 * ESCAPE CHARACTER NOTE:
		 *
		 * New argument; NULL terminate, skipping anything
		 * that's preceded by the user's quoting character.
		 */
		for (ap = p; p[0] != '\0'; ++p) {
			if (sp->special[p[0]] == K_VLNEXT && p[1])
				p += 2;
			if (isblank(p[0]))
				break;
		}
		if (*p)
			*p = '\0';
		else
			done = 1;

		/*
		 * Allocate more pointer space if necessary; leave a space
		 * for a trailing NULL.
		 */
		len = (p - ap) + 1;
#define	INCREMENT	20
		if (off + 2 >= sp->argscnt - 1) {
			sp->argscnt += cnt = MAX(INCREMENT, 2);
			if ((ARGP = realloc(ARGP,
			    sp->argscnt * sizeof(ARGS))) == NULL) {
				free(sp->argv);
				goto mem1;
			}
			if ((sp->argv = realloc(sp->argv,
			    sp->argscnt * sizeof(char *))) == NULL) {
				free(ARGP);
mem1:				sp->argscnt = 0;
				ARGP = NULL;
				sp->argv = NULL;
				msgq(sp, M_ERR, "Error: %s.", strerror(errno));
				return (1);
			}
			memset(&ARGP[off], 0, cnt * sizeof(ARGS));
		}

		/*
		 * Copy the argument(s) into place, allocating space if
		 * necessary.
		 */
		if (ARGP[off].len < len) {
			if ((ARGP[off].bp =
			    realloc(ARGP[off].bp, len)) == NULL) {
				ARGP[off].bp = NULL;
				ARGP[off].len = 0;
				msgq(sp, M_ERR, "Error: %s.", strerror(errno));
				return (1);
			}
			ARGP[off].len = len;
		}
		sp->argv[off] = ARGP[off].bp;
		ARGP[off].flags |= A_ALLOCATED;

		/*
		 * ESCAPE CHARACTER NOTE:
		 *
		 * Copy the argument into place, losing quote chars.
		 */
		for (t = ARGP[off].bp; len; *t++ = *ap++, --len)
			if (sp->special[*ap] == K_VLNEXT && len) {
				++ap;
				--len;
			}
		++off;

		if (done)
			break;
	}
	sp->argv[off] = NULL;
	*argvp = sp->argv;
	*argcp = off;

#if defined(DEBUG) && 0
	for (cnt = 0; cnt < off; ++cnt)
		TRACE(sp, "arg %d: {%s}\n", cnt, sp->argv[cnt]);
#endif
	return (0);
}

int
free_argv(sp)
	SCR *sp;
{
	int off;

	if (sp->args != NULL) {
		for (off = 0; off < sp->argscnt; ++off)
			if (F_ISSET(&ARGP[off], A_ALLOCATED))
				FREE(ARGP[off].bp, ARGP[off].len);
		FREE(ARGP, sp->argscnt * sizeof(ARGS *));
		FREE(sp->argv, sp->argscnt * sizeof(char *));
	}
	return (0);
}

/*
 * ex_pipe_process --
 *	Fork a process and exec a program, reading the standard out of the
 *	program and piping it to the output of ex, whether that's really
 *	stdout, or the vi screen.
 */
static int
ex_pipe_process(sp, cmd, lenp, bp, blen)
	SCR *sp;
	char *cmd, *bp;
	size_t *lenp, blen;
{
	FILE *ifp;
	pid_t pid;
	size_t len;
	int ch, rval, output[2];
	char *p, *sh, *sh_path, buf[1024];

	sh_path = O_STR(sp, O_SHELL);
	if ((sh = strrchr(sh_path, '/')) == NULL)
		sh = sh_path;
	else
		++sh;

	/*
	 * There are two different processes running through this code.
	 * They are named the utility and the parent. The utility reads
	 * from standard input and writes to the parent.  The parent reads
	 * from the utility and writes into the buffer.  The parent reads
	 * from output[0], and the utility writes to output[1].
	 */
	if (pipe(output) < 0) {
		msgq(sp, M_ERR, "Error: pipe: %s", strerror(errno));
		return (1);
	}
	if ((ifp = fdopen(output[0], "r")) == NULL) {
		msgq(sp, M_ERR, "Error: fdopen: %s", strerror(errno));
		goto err;
	}
		
	/*
	 * Do the minimal amount of work possible, the shell is going
	 * to run briefly and then exit.  Hopefully.
	 */
	switch (pid = vfork()) {
	case -1:			/* Error. */
		msgq(sp, M_ERR, "Error: vfork: %s", strerror(errno));
err:		(void)close(output[0]);
		(void)close(output[1]);
		return (1);
	case 0:				/* Utility. */
		/* Redirect stdout/stderr to the write end of the pipe. */
		(void)dup2(output[1], STDOUT_FILENO);
		(void)dup2(output[1], STDERR_FILENO);

		/* Close the utility's file descriptors. */
		(void)close(output[0]);
		(void)close(output[1]);

		/* Assumes that all shells have -c. */
		execl(sh_path, sh, "-c", cmd, NULL);
		msgq(sp, M_ERR,
		    "Error: execl: %s: %s", sh_path, strerror(errno));
		_exit(127);
	default:
		/* Close the pipe end the parent won't use. */
		(void)close(output[1]);
		break;
	}

	rval = 0;

	/* Copy process output into a buffer. */
	for (p = bp, len = 0, ch = EOF;
	    --blen && (ch = getc(ifp)) != EOF; *p++ = ch, ++len);
	if (ch != EOF) {
		rval = 1;
		msgq(sp, M_ERR, "%s: output truncated", sh);
	}
	if (ferror(ifp)) {
		rval = 1;
		msgq(sp, M_ERR, "I/O error: %s", sh);
	}
	(void)fclose(ifp);

	/* Wait for the process. */
	rval |= proc_wait(sp, (long)pid, sh, 0);

	/* Delete the final newline, nul terminate the string. */
	if (p > bp && p[-1] == '\n' || p[-1] == '\r') {
		--len;
		*--p = '\0';
	} else
		*p = '\0';
	*lenp = len;

	return (rval);
}
