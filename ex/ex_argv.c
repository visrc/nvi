/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_argv.c,v 8.14 1993/11/13 18:02:18 bostic Exp $ (Berkeley) $Date: 1993/11/13 18:02:18 $";
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

static int argv_allocate __P((SCR *, int));
static int argv_fexp __P((SCR *, EXCMDARG *,
	       char *, char *, size_t *, char **, size_t *, int));
static int argv_sexp __P((SCR *, char *, size_t, size_t *));

#define	ARGALLOC(off, len)						\
	if (exp->args[off].len < len) {					\
		if ((exp->args[off].bp =				\
		   realloc(exp->args[off].bp, len)) == NULL) {		\
			exp->args[off].bp = NULL;			\
			exp->args[off].len = 0;				\
			msgq(sp, M_SYSERR, NULL);			\
			return (1);					\
		}							\
		exp->args[off].len = len;				\
		exp->args[off].flags |= A_ALLOCATED;			\
	}

/*
 * argv_exp1 --
 *	Do file name expansion on a string, and leave it in a string.
 */
int
argv_exp1(sp, ep, cmdp, s, is_bang)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	char *s;
	int is_bang;
{
	EX_PRIVATE *exp;
	size_t blen, len;
	char *bp;

	GET_SPACE(sp, bp, blen, 512);

	len = 0;
	exp = EXP(sp);
	if (argv_fexp(sp, cmdp, s, bp, &len, &bp, &blen, is_bang) ||
	    exp->argscnt < 2 && argv_allocate(sp, 0)) {
		FREE_SPACE(sp, bp, blen);
		return (1);
	}

	ARGALLOC(0, len);
	memmove(exp->args[0].bp, bp, len);
	exp->argv[0] = exp->args[0].bp;
	exp->argv[1] = NULL;

	FREE_SPACE(sp, bp, blen);

	cmdp->argv = exp->argv;
	cmdp->argc = 1;
	return (0);
}

/*
 * argv_exp2 --
 *	Do file name and shell expansion on a string, and break
 *	it up into an argv.
 */
int
argv_exp2(sp, ep, cmdp, s, is_bang)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	char *s;
	int is_bang;
{
	size_t len, blen;
	int rval;
	char *bp, *p;

	GET_SPACE(sp, bp, blen, 512);

#define	SHELLECHO	"echo "
#define	SHELLOFFSET	(sizeof(SHELLECHO) - 1)
	memmove(bp, SHELLECHO, SHELLOFFSET);
	p = bp + SHELLOFFSET;
	len = SHELLOFFSET;

#if defined(DEBUG) && 0
	TRACE(sp, "file_argv: {%s}\n", s);
#endif

	if (argv_fexp(sp, cmdp, s, p, &len, &bp, &blen, is_bang)) {
		rval = 1;
		goto err;
	}

#if defined(DEBUG) && 0
	TRACE(sp, "before shell: %d: {%s}\n", len, bp);
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
		if (argv_sexp(sp, bp, blen, &len))
			return (1);
		p = bp;
	} else
		p = bp + SHELLOFFSET;

#if defined(DEBUG) && 0
	TRACE(sp, "after shell: %d: {%s}\n", len, bp);
#endif

	rval = argv_exp3(sp, ep, cmdp, p);

err:	FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * argv_exp3 --
 *	Take a string and break it up into an argv.
 */
int
argv_exp3(sp, ep, cmdp, p)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	char *p;
{
	EX_PRIVATE *exp;
	size_t len;
	int done, off;
	char *ap, *t;

	exp = EXP(sp);
	for (done = off = 0;; ++p) {
		/* Skip any leading whitespace. */
		for (; isblank(p[0]); ++p);
		if (*p == '\0')
			break;

		/*
		 * QUOTING NOTE:
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
		 * Allocate more pointer space if necessary, making
		 * sure there's space for a trailing NULL pointer.
		 */
		if (off + 2 >= exp->argscnt - 1 && argv_allocate(sp, off))
			return (1);

		/* Allocate more argument space if necessary. */
		len = (p - ap) + 1;
		ARGALLOC(off, len);
		exp->argv[off] = exp->args[off].bp;

		/*
		 * QUOTING NOTE:
		 *
		 * Copy the argument into place, losing quote chars.
		 */
		for (t = exp->args[off].bp; len; *t++ = *ap++, --len)
			if (sp->special[*ap] == K_VLNEXT && len) {
				++ap;
				--len;
			}
		++off;

		if (done)
			break;
	}
	exp->argv[off] = NULL;
	cmdp->argv = exp->argv;
	cmdp->argc = off;

#if defined(DEBUG) && 0
	for (cnt = 0; cnt < off; ++cnt)
		TRACE(sp, "arg %d: {%s}\n", cnt, exp->argv[cnt]);
#endif
	return (0);
}

/*
 * argv_fexp --
 *	Do file name and bang command expansion.
 */
static int
argv_fexp(sp, cmdp, s, p, lenp, bpp, blenp, is_bang)
	SCR *sp;
	EXCMDARG *cmdp;
	char *s, *p, **bpp;
	size_t *lenp, *blenp;
	int is_bang;
{
	EX_PRIVATE *exp;
	char *bp, *t;
	size_t blen, len, tlen;

	/* Replace file name characters. */
	for (bp = *bpp, blen = *blenp, len = *lenp; *s; ++s)
		switch (*s) {
		case '!':
			if (!is_bang)
				goto ins_ch;
			exp = EXP(sp);
			if (exp->lastbcomm == NULL) {
				msgq(sp, M_ERR,
				    "No previous command to replace \"!\".");
				return (1);
			}
			len += tlen = strlen(exp->lastbcomm);
			ADD_SPACE(sp, bp, blen, len);
			memmove(p, exp->lastbcomm, tlen);
			p += tlen;
			F_SET(cmdp, E_MODIFY);
			break;
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
			F_SET(cmdp, E_MODIFY);
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
			F_SET(cmdp, E_MODIFY);
			break;
		case '\\':
			/*
			 * QUOTING NOTE:
			 *
			 * Strip any backslashes that protected the file
			 * expansion characters.
			 */
			if (s[1] == '%' || s[1] == '#')
				++s;
			/* FALLTHROUGH */
		default:
ins_ch:			++len;
			ADD_SPACE(sp, bp, blen, len);
			*p++ = *s;
		}

	/* Nul termination. */
	++len;
	ADD_SPACE(sp, bp, blen, len);
	*p = '\0';

	/* Return the new string length, buffer, buffer length. */
	*lenp = len;
	*bpp = bp;
	*blenp = blen;
	return (0);
}

/*
 * argv_allocate --
 *	Make more space for arguments.
 */
static int
argv_allocate(sp, off)
	SCR *sp;
	int off;
{
	EX_PRIVATE *exp;
	int cnt;

#define	INCREMENT	20
	exp = EXP(sp);
	cnt = exp->argscnt + INCREMENT;
	if ((exp->args = realloc(exp->args, cnt * sizeof(ARGS))) == NULL) {
		(void)argv_free(sp);
		goto mem;
	}
	if ((exp->argv =
	    realloc(exp->argv, cnt * sizeof(char *))) == NULL) {
		(void)argv_free(sp);
mem:		msgq(sp, M_SYSERR, NULL);
		return (1);
	}
	memset(&exp->args[off], 0, INCREMENT * sizeof(ARGS));
	exp->argscnt = cnt;
	return (0);
}

/*
 * argv_free --
 *	Free up argument structures.
 */
int
argv_free(sp)
	SCR *sp;
{
	EX_PRIVATE *exp;
	int off;

	exp = EXP(sp);
	if (exp->args != NULL) {
		for (off = 0; off < exp->argscnt; ++off)
			if (F_ISSET(&exp->args[off], A_ALLOCATED))
				FREE(exp->args[off].bp, exp->args[off].len);
		FREE(exp->args, exp->argscnt * sizeof(ARGS *));
	}
	if (exp->argv != NULL)
		FREE(exp->argv, exp->argscnt * sizeof(char *));
	exp->argscnt = 0;
	return (0);
}

/*
 * argv_sexp --
 *	Fork a shell, pipe a command through it, and read the output into
 *	a buffer.
 */
static int
argv_sexp(sp, bp, blen, lenp)
	SCR *sp;
	char *bp;
	size_t *lenp, blen;
{
	FILE *ifp;
	pid_t pid;
	size_t len;
	int ch, rval, output[2];
	char *p, *sh, *sh_path;

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
		msgq(sp, M_SYSERR, "pipe");
		return (1);
	}
	if ((ifp = fdopen(output[0], "r")) == NULL) {
		msgq(sp, M_SYSERR, "fdopen");
		goto err;
	}
		
	/*
	 * Do the minimal amount of work possible, the shell is going
	 * to run briefly and then exit.  Hopefully.
	 */
	switch (pid = vfork()) {
	case -1:			/* Error. */
		msgq(sp, M_SYSERR, "vfork");
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
		execl(sh_path, sh, "-c", bp, NULL);
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
