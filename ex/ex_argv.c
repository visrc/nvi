/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_argv.c,v 8.20 1993/12/02 18:47:51 bostic Exp $ (Berkeley) $Date: 1993/12/02 18:47:51 $";
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
	       char *, size_t, char *, size_t *, char **, size_t *, int));
static int argv_sexp __P((SCR *, char *, size_t, size_t *));

/*
 * Allocate room for another argument, always leaving enough
 * room for a NULL pointer.
 */
#define	ARGALLOC(l) {							\
	ARGS *__ap;							\
	size_t __off = exp->argsoff;					\
	if ((exp->argscnt == 0 ||					\
	    __off + 2 >= exp->argscnt - 1) && argv_allocate(sp, __off))	\
		return (1);						\
	if (exp->args[__off] == NULL &&					\
	    (exp->args[__off] = calloc(1, sizeof(ARGS))) == NULL) {	\
		exp->args[__off] = NULL;				\
		goto mem;						\
	}								\
	__ap = exp->args[__off];					\
	if (__ap->blen < (l) + 1) {					\
		__ap->blen = (l) + 1;					\
		if ((__ap->bp = realloc(__ap->bp,			\
		    __ap->blen * sizeof(CHAR_T))) == NULL) {		\
			__ap->bp = NULL;				\
			__ap->blen = 0;					\
			F_CLR(__ap, A_ALLOCATED);			\
mem:			msgq(sp, M_SYSERR, NULL);			\
			return (1);					\
		}							\
		F_SET(__ap, A_ALLOCATED);				\
	}								\
}

/*
 * argv_exp0 --
 *	Put a string into an argv.
 */
int
argv_exp0(sp, ep, excp, cmd, cmdlen)
	SCR *sp;
	EXF *ep;
	EXCMDARG *excp;
	char *cmd;
	size_t cmdlen;
{
	EX_PRIVATE *exp;

	exp = EXP(sp);
	ARGALLOC(cmdlen);
	memmove(exp->args[exp->argsoff]->bp, cmd, cmdlen);
	exp->args[exp->argsoff]->bp[cmdlen] = '\0';
	exp->args[exp->argsoff]->len = cmdlen;
	++exp->argsoff;
	return (0);
}

/*
 * argv_exp1 --
 *	Do file name expansion on a string, and leave it in a string.
 */
int
argv_exp1(sp, ep, excp, cmd, cmdlen, is_bang)
	SCR *sp;
	EXF *ep;
	EXCMDARG *excp;
	char *cmd;
	size_t cmdlen;
	int is_bang;
{
	EX_PRIVATE *exp;
	size_t blen, len;
	char *bp;

	GET_SPACE(sp, bp, blen, 512);

	len = 0;
	exp = EXP(sp);
	if (argv_fexp(sp, excp, cmd, cmdlen, bp, &len, &bp, &blen, is_bang) ||
	    exp->argscnt < 2 && argv_allocate(sp, 0)) {
		FREE_SPACE(sp, bp, blen);
		return (1);
	}

	ARGALLOC(len);
	memmove(exp->args[exp->argsoff]->bp, bp, len);
	exp->args[exp->argsoff]->bp[len] = '\0';
	exp->args[exp->argsoff]->len = len;
	++exp->argsoff;

	FREE_SPACE(sp, bp, blen);
	return (0);
}

/*
 * argv_exp2 --
 *	Do file name and shell expansion on a string, and break
 *	it up into an argv.
 */
int
argv_exp2(sp, ep, excp, cmd, cmdlen, is_bang)
	SCR *sp;
	EXF *ep;
	EXCMDARG *excp;
	char *cmd;
	size_t cmdlen;
	int is_bang;
{
	size_t blen, len, n;
	int rval;
	char *bp, *p;

	GET_SPACE(sp, bp, blen, 512);

#define	SHELLECHO	"echo "
#define	SHELLOFFSET	(sizeof(SHELLECHO) - 1)
	memmove(bp, SHELLECHO, SHELLOFFSET);
	p = bp + SHELLOFFSET;
	len = SHELLOFFSET;

#if defined(DEBUG) && 0
	TRACE(sp, "file_argv: {%.*s}\n", (int)cmdlen, cmd);
#endif

	if (argv_fexp(sp, excp, cmd, cmdlen, p, &len, &bp, &blen, is_bang)) {
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
	for (p = bp, n = len; n > 0; --n, ++p)
		if (!isalnum(*p) && !isblank(*p) && *p != '/' && *p != '.')
			break;
	if (n > 0) {
		if (argv_sexp(sp, bp, blen, &len)) {
			rval = 1;
			goto err;
		}
		p = bp;
	} else {
		p = bp + SHELLOFFSET;
		len -= SHELLOFFSET;
	}

#if defined(DEBUG) && 0
	TRACE(sp, "after shell: %d: {%s}\n", len, bp);
#endif

	rval = argv_exp3(sp, ep, excp, p, len);

err:	FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * argv_exp3 --
 *	Take a string and break it up into an argv.
 */
int
argv_exp3(sp, ep, excp, cmd, cmdlen)
	SCR *sp;
	EXF *ep;
	EXCMDARG *excp;
	char *cmd;
	size_t cmdlen;
{
	CHAR_T vlit;
	EX_PRIVATE *exp;
	size_t len;
	int ch, off;
	char *ap, *p;

	(void)term_key_ch(sp, K_VLNEXT, &vlit);
	for (exp = EXP(sp); cmdlen > 0; ++exp->argsoff) {
		/* Skip any leading whitespace. */
		for (; cmdlen > 0; --cmdlen, ++cmd) {
			ch = *cmd;
			if (!isblank(ch))
				break;
		}
		if (cmdlen == 0)
			break;

		/*
		 * Determine the length of this whitespace delimited
		 * argument.  
		 *
		 * QUOTING NOTE:
		 *
		 * Skip any character preceded by the user's quoting
		 * character.
		 */
		for (ap = cmd, len = 0; cmdlen > 0; --cmdlen, ++cmd, ++len)
			if ((ch = *cmd) == vlit && cmdlen > 1)
				++cmd;
			else if (isblank(ch))
				break;
				
		/*
		 * Copy the argument into place.
		 *
		 * QUOTING NOTE:
		 *
		 * Lose quote chars.
		 */
		ARGALLOC(len);
		off = exp->argsoff;
		exp->args[off]->len = len;
		for (p = exp->args[off]->bp; len > 0; --len, *p++ = *ap++)
			if (*ap == vlit) {
				++ap;
				--len;
				--exp->args[off]->len;
			}
		*p = '\0';
	}

#if defined(DEBUG) && 0
	for (cnt = 0; cnt < exp->argsoff; ++cnt)
		TRACE(sp, "arg %d: {%s}\n", cnt, exp->argv[cnt]);
#endif
	return (0);
}

/*
 * argv_fexp --
 *	Do file name and bang command expansion.
 */
static int
argv_fexp(sp, excp, cmd, cmdlen, p, lenp, bpp, blenp, is_bang)
	SCR *sp;
	EXCMDARG *excp;
	char *cmd, *p, **bpp;
	size_t cmdlen, *lenp, *blenp;
	int is_bang;
{
	EX_PRIVATE *exp;
	char *bp, *t;
	size_t blen, len, tlen;

	/* Replace file name characters. */
	for (bp = *bpp, blen = *blenp, len = *lenp; cmdlen > 0; --cmdlen, ++cmd)
		switch (*cmd) {
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
			F_SET(excp, E_MODIFY);
			break;
		case '%':
			if (sp->frp->cname == NULL && sp->frp->name == NULL) {
				msgq(sp, M_ERR,
				    "No filename to substitute for %%.");
				return (1);
			}
			tlen = strlen(t = FILENAME(sp->frp));
			len += tlen;
			ADD_SPACE(sp, bp, blen, len);
			memmove(p, t, tlen);
			p += tlen;
			F_SET(excp, E_MODIFY);
			break;
		case '#':
			/*
			 * Try the alternate file name first, then the
			 * previously edited file.
			 */
			if (sp->alt_name == NULL && (sp->p_frp == NULL ||
			    sp->frp->cname == NULL && sp->frp->name == NULL)) {
				msgq(sp, M_ERR,
				    "No filename to substitute for #.");
				return (1);
			}
			if (sp->alt_name != NULL)
				t = sp->alt_name;
			else
				t = FILENAME(sp->frp);
			len += tlen = strlen(t);
			ADD_SPACE(sp, bp, blen, len);
			memmove(p, t, tlen);
			p += tlen;
			F_SET(excp, E_MODIFY);
			break;
		case '\\':
			/*
			 * QUOTING NOTE:
			 *
			 * Strip any backslashes that protected the file
			 * expansion characters.
			 */
			if (cmdlen > 1 && cmd[1] == '%' || cmd[1] == '#')
				++cmd;
			/* FALLTHROUGH */
		default:
ins_ch:			++len;
			ADD_SPACE(sp, bp, blen, len);
			*p++ = *cmd;
		}

	/* Nul termination. */
	++len;
	ADD_SPACE(sp, bp, blen, len);
	*p = '\0';

	/* Return the new string length, buffer, buffer length. */
	*lenp = len - 1;
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
	if ((exp->args = realloc(exp->args, cnt * sizeof(ARGS *))) == NULL) {
		(void)argv_free(sp);
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}
	memset(&exp->args[off], 0, INCREMENT * sizeof(ARGS *));
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
		for (off = 0; off < exp->argscnt; ++off) {
			if (exp->args[off] == NULL)
				continue;
			if (F_ISSET(exp->args[off], A_ALLOCATED))
				free(exp->args[off]->bp);
			FREE(exp->args[off], sizeof(ARGS));
		}
		FREE(exp->args, exp->argscnt * sizeof(ARGS *));
	}
	exp->args = NULL;
	exp->argscnt = 0;
	exp->argsoff = 0;
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
