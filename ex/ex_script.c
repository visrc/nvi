/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_script.c,v 8.5 1993/11/03 17:24:24 bostic Exp $ (Berkeley) $Date: 1993/11/03 17:24:24 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

static int sscr_init __P((SCR *, EXF *));
static int sscr_matchprompt __P((SCR *, char *, size_t, size_t *));

/*
 * ex_script -- : sc[ript][!] [file]
 *
 *	Switch to visual mode.
 */
int
ex_script(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	/* Vi only command. */
	if (!F_ISSET(sp, S_MODE_VI)) {
		msgq(sp, M_ERR,
		    "The script command is only available in vi mode.");
		return (1);
	}

	/* Switch to the new file. */
	if (ex_edit(sp, ep, cmdp))
		return (1);

	/*
	 * Create the shell, figure out the prompt.
	 *
	 * !!!
	 * The files just switched, use sp->ep.
	 */
	if (sscr_init(sp, sp->ep))
		return (1);

	return (0);
}

/*
 * sscr_init --
 *	Create a pipe to a shell.
 */
static int
sscr_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	struct timeval tv;
	fd_set fdset;
	recno_t lline;
	size_t llen, len;
	int cnt, nr;
	char *endp, *p, *t, *sh, *sh_path, buf[1024];

	/*
	 * There are two different processes running through this code.
	 * They are the shell and the parent.
	 *
	 * Input and output are named from the shell's point of view.  The
	 * shell reads from sh_in[0] and the parent writes to sh_in[1].
	 * The parent reads from sh_out[0] and the shell writes to sh_out[1].
	 */
	sp->sh_in[0] = sp->sh_in[1] = sp->sh_out[0] = sp->sh_out[1] = -1;
	if (pipe(sp->sh_in) < 0 || pipe(sp->sh_out) < 0) {
		msgq(sp, M_ERR, "Error: pipe: %s", strerror(errno));
		goto err;
	}

	switch (sp->sh_pid = vfork()) {
	case -1:			/* Error. */
		msgq(sp, M_ERR, "Error: vfork: %s", strerror(errno));
err:		if (sp->sh_in[0] != -1)
			(void)close(sp->sh_in[0]);
		if (sp->sh_in[1] != -1)
			(void)close(sp->sh_in[1]);
		if (sp->sh_out[0] != -1)
			(void)close(sp->sh_out[0]);
		if (sp->sh_out[1] != -1)
			(void)close(sp->sh_out[1]);
		return (1);
	case 0:				/* Utility. */
		/*
		 * The utility has default signal behavior.  Don't bother
		 * using sigaction(2) 'cause we want the default behavior.
		 */
		(void)signal(SIGINT, SIG_DFL);
		(void)signal(SIGQUIT, SIG_DFL);

		/*
		 * Redirect stdin from the read end of the sh_in pipe,
		 * and redirect stdout/stderr to the write end of the
		 * sh_out pipe.
		 */
		(void)dup2(sp->sh_in[0], STDIN_FILENO);
		(void)dup2(sp->sh_out[1], STDOUT_FILENO);
		(void)dup2(sp->sh_out[1], STDERR_FILENO);

		/* Close the utility's file descriptors. */
		(void)close(sp->sh_in[0]);
		(void)close(sp->sh_in[1]);
		(void)close(sp->sh_out[0]);
		(void)close(sp->sh_out[1]);

		/* Assumes that all shells have -i. */
		sh_path = O_STR(sp, O_SHELL);
		if ((sh = strrchr(sh_path, '/')) == NULL)
			sh = sh_path;
		else
			++sh;
		execl(sh_path, sh, "-i", NULL);
		msgq(sp, M_ERR,
		    "Error: execl: %s: %s", sh_path, strerror(errno));
		_exit(127);
	default:
		/* Close the pipe ends the parent won't use. */
		(void)close(sp->sh_in[0]);
		(void)close(sp->sh_out[1]);
		break;
	}

	/*
	 * Figure out the prompt.  The scheme here is pretty simple.  Take
	 * the first three lines, i.e. lines that don't end with a newline,
	 * and see what they share.  If they don't share anything, we quit.
	 * Otherwise, we mark what they share and use it for comparison later.
	 * There are a lot of built-in assumptions about how shells behave.
	 */
	FD_ZERO(&fdset);
	for (endp = buf, len = sizeof(buf), cnt = 0;; ++cnt) {
		/* Wait up to a second for characters to read. */
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		FD_SET(sp->sh_out[0], &fdset);
		switch (select(sp->sh_out[0] + 1, &fdset, NULL, NULL, &tv)) {
		case -1:		/* Error or interrupt. */
			msgq(sp, M_ERR, "Error: select: %s.", strerror(errno));
			goto prompterr;
		case  0:		/* Timeout */
			msgq(sp, M_ERR, "Error: timed out.");
			goto prompterr;
		case  1:		/* Characters to read. */
			break;
		}

		/* Read the characters. */
more:		len = sizeof(buf) - (endp - buf);
		switch (nr = read(sp->sh_out[0], endp, len)) {
		case  0:			/* EOF. */
			msgq(sp, M_ERR, "Error: shell: EOF");
			goto prompterr;
		case -1:			/* Error or interrupt. */
			msgq(sp, M_ERR, "Error: shell: %s", strerror(errno));
			goto prompterr;
		default:
			endp += nr;
			break;
		}

		/* If any complete lines, push them into the file. */
		for (p = t = buf; t < endp; ++t)
			if (sp->special[*t] == K_CR ||
			    sp->special[*t] == K_NL) {
				if (file_lline(sp, ep, &lline) ||
				    file_aline(sp, ep, 0, lline, buf, t - p))
					goto prompterr;
				p = ++t;
			}
		if (p > buf) {
			memmove(buf, p, endp - p);
			endp = buf + (endp - p);
		}
		if (endp == buf)
			goto more;

		/* Wait up 1/10 of a second to make sure that we got it all. */
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		switch (select(sp->sh_out[0] + 1, &fdset, NULL, NULL, &tv)) {
		case -1:		/* Error or interrupt. */
			msgq(sp, M_ERR, "Error: select: %s", strerror(errno));
			goto prompterr;
		case  0:		/* Timeout */
			break;
		case  1:		/* Characters to read. */
			goto more;
		}

		/* Timed out, so theoretically we have a prompt. */
		llen = endp - buf;
		endp = buf;

		/* Append the line into the file. */
		if (file_lline(sp, ep, &lline) ||
		    file_aline(sp, ep, 0, lline, buf, llen)) {
prompterr:		sscr_end(sp);
			return (1);
		}

		if (cnt == 0) {
			if ((sp->sh_prompt = malloc(llen)) == NULL) {
				msgq(sp, M_ERR, "Error: %s", strerror(errno));
				goto prompterr;
			}
			memmove(sp->sh_prompt, buf, llen);
			sp->sh_prompt_len = llen;
		} else {
			if (sp->sh_prompt_len < llen)
				llen = sp->sh_prompt_len;
			else
				sp->sh_prompt_len = llen;
			/* Nul's in the prompt are "don't match" characters. */
			for (t = sp->sh_prompt, p = buf; --llen; ++p, ++t)
				if (*p != *t)
					*t = '\0';
		}
#define	TESTCNT	3
		if (cnt == TESTCNT)
			break;

		/* Write a command to the shell. */
		(void)write(sp->sh_in[1], "date\n", 5);
	}
		
	/* Turn on the script. */
	F_SET(sp, S_SCRIPT);

	return (0);
}

/*
 * sscr_exec --
 *	Take a line and hand it off to the shell.
 */
int
sscr_exec(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	recno_t last_lno;
	size_t blen, len, last_len, tlen;
	int matchprompt, nw;
	char *bp, *p;

	/* If there's a prompt on the last line, append the command. */
	if (file_lline(sp, ep, &last_lno))
		return (1);
	if ((p = file_gline(sp, ep, last_lno, &last_len)) == NULL) {
		GETLINE_ERR(sp, last_lno);
		return (1);
	}
	if (sscr_matchprompt(sp, p, last_len, &tlen) && tlen == 0) {
		matchprompt = 1;
		GET_SPACE(sp, bp, blen, 128 + last_len);
		memmove(bp, p, last_len);
	} else
		matchprompt = 0;

	/* Get something to execute. */
	if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			goto err1;
		if (lno == 0)
			goto empty;
		else
			GETLINE_ERR(sp, lno);
		goto err1;
	}

	/* Empty lines aren't interesting. */
	if (len == 0)
		goto empty;

	/* Delete any prompt. */
	if (sscr_matchprompt(sp, p, len, &tlen)) {
		if (tlen == len) {
empty:			msgq(sp, M_BERR, "Nothing to execute.");
			goto err1;
		}
		p += (len - tlen);
		len = tlen;
	}

	/* Push the line to the shell. */
	if ((nw = write(sp->sh_in[1], p, len)) != len)
		goto err2;
	if (write(sp->sh_in[1], "\n", 1) != 1) {
err2:		msgq(sp, M_ERR,
		    "Error: shell: %s", strerror(nw == 0 ? EIO : errno));
		goto err1;
	}

	if (matchprompt) {
		ADD_SPACE(sp, bp, blen, last_len + len);
		memmove(bp + last_len, p, len);
		if (file_sline(sp, ep, last_lno, bp, last_len + len)) {
err1:			if (matchprompt)
				FREE_SPACE(sp, bp, blen);
			return (1);
		}
	}
	return (0);
}

/*
 * sscr_input --
 *	Take a line from the shell and insert it into the file.
 */
int
sscr_input(sp)
	SCR *sp;
{
	struct timeval tv;
	EXF *ep;
	recno_t lno;
	size_t blen, len, tlen;
	int nr, rval;
	char *bp, *endp, *p, *t;

	/* Find out where the end of the file is. */
	ep = sp->ep;
	if (file_lline(sp, ep, &lno))
		return (1);

#define	MINREAD	1024
	GET_SPACE(sp, bp, blen, MINREAD);
	endp = bp;

	/* Read the characters. */
	rval = 1;
more:	switch (nr = read(sp->sh_out[0], endp, MINREAD)) {
	case  0:			/* EOF; shell just exited. */
		sscr_end(sp);
		F_CLR(sp, S_SCRIPT);
		rval = 0;
		goto ret;
	case -1:			/* Error or interrupt. */
		msgq(sp, M_ERR, "Error: shell: %s", strerror(errno));
		goto ret;
	default:
		endp += nr;
		break;
	}

	/* Append the lines into the file. */
	for (p = t = bp; p < endp; ++p)
		if (sp->special[*p] == K_CR || sp->special[*p] == K_NL) {
			len = p - t;
			if (file_aline(sp, ep, 1, lno++, t, len))
				goto ret;
			t = p + 1;
		}
	if (p > t) {
		len = p - t;
		/*
		 * If the last thing from the shell isn't another prompt, wait
		 * up to 1/10 of a second for more stuff to show up, so that
		 * we don't break the output into two separate lines.  Don't
		 * want to hang indefinitely because some program is hanging,
		 * confused the shell, or whatever.
		 */
		if (!sscr_matchprompt(sp, t, len, &tlen) || tlen != 0) {
			tv.tv_sec = 0;
			tv.tv_usec = 100000;
			FD_SET(sp->sh_out[0], &sp->rdfd);
			FD_CLR(STDIN_FILENO, &sp->rdfd);
			if (select(sp->sh_out[0] + 1,
			    &sp->rdfd, NULL, NULL, &tv) == 1) {
				memmove(bp, t, len);
				endp = bp + len;
				goto more;
			}
		}
		if (file_aline(sp, ep, 1, lno++, t, len))
			goto ret;
	}

	/* The cursor moves to EOF. */
	sp->lno = lno;
	sp->cno = len ? len - 1 : 0;
	rval = sp->s_refresh(sp, ep);

ret:	FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * sscr_matchprompt --
 *	Check to see if a line matches the prompt.  Nul's indicate
 *	parts that can change, in both content and size.
 */
static int
sscr_matchprompt(sp, lp, line_len, lenp)
	SCR *sp;
	char *lp;
	size_t line_len, *lenp;
{
	char *pp;
	size_t prompt_len;

	if (line_len < (prompt_len = sp->sh_prompt_len))
		return (0);

	for (pp = sp->sh_prompt;
	    prompt_len && line_len; --prompt_len, --line_len) {
		if (*pp == '\0') {
			for (; prompt_len && *pp == '\0'; --prompt_len, ++pp);
			if (!prompt_len)
				return (0);
			for (; line_len && *lp != *pp; --line_len, ++lp);
			if (!line_len)
				return (0);
		}
		if (*pp++ != *lp++)
			break;
	}

	if (prompt_len)
		return (0);
	if (lenp != NULL)
		*lenp = line_len;
	return (1);
}

/*
 * sscr_end --
 *	End the pipe to a shell.
 */
int
sscr_end(sp)
	SCR *sp;
{
	int pstat;

	/* Close down the parent's file descriptors. */
	(void)close(sp->sh_in[1]);
	(void)close(sp->sh_out[0]);

	/* This should have killed the child. */
	(void)waitpid(sp->sh_pid, &pstat, 0);

	/* Free memory. */
	FREE(sp->sh_prompt, sp->sh_prompt_len);

	/* Turn off the script. */
	F_CLR(sp, S_SCRIPT);

	return (0);
}
