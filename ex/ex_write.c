/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_write.c,v 10.2 1995/05/05 18:53:20 bostic Exp $ (Berkeley) $Date: 1995/05/05 18:53:20 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"

enum which {WN, WQ, WRITE, XIT};
static int exwr __P((SCR *, EXCMD *, enum which));

/*
 * ex_wn --	:wn[!] [>>] [file]
 *	Write to a file and switch to the next one.
 *
 * PUBLIC: int ex_wn __P((SCR *, EXCMD *));
 */
int
ex_wn(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	if (exwr(sp, cmdp, WN))
		return (1);
	if (file_m3(sp, 0))
		return (1);

	/* The file name isn't a new file to edit. */
	cmdp->argc = 0;

	return (ex_next(sp, cmdp));
}

/*
 * ex_wq --	:wq[!] [>>] [file]
 *	Write to a file and quit.
 *
 * PUBLIC: int ex_wq __P((SCR *, EXCMD *));
 */
int
ex_wq(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	int force;

	if (exwr(sp, cmdp, WQ))
		return (1);
	if (file_m3(sp, 0))
		return (1);

	force = FL_ISSET(cmdp->iflags, E_C_FORCE);

	if (ex_ncheck(sp, force))
		return (1);

	F_SET(sp, force ? S_EXIT_FORCE : S_EXIT);
	return (0);
}

/*
 * ex_write --	:write[!] [>>] [file]
 *		:write [!] [cmd]
 *	Write to a file.
 *
 * PUBLIC: int ex_write __P((SCR *, EXCMD *));
 */
int
ex_write(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	return (exwr(sp, cmdp, WRITE));
}


/*
 * ex_xit -- :x[it]! [file]
 *	Write out any modifications and quit.
 *
 * PUBLIC: int ex_xit __P((SCR *, EXCMD *));
 */
int
ex_xit(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	int force;

	NEEDFILE(sp, cmdp->cmd);

	if (F_ISSET(sp->ep, F_MODIFIED) && exwr(sp, cmdp, XIT))
		return (1);
	if (file_m3(sp, 0))
		return (1);

	force = FL_ISSET(cmdp->iflags, E_C_FORCE);

	if (ex_ncheck(sp, force))
		return (1);

	F_SET(sp, force ? S_EXIT_FORCE : S_EXIT);
	return (0);
}

/*
 * exwr --
 *	The guts of the ex write commands.
 */
static int
exwr(sp, cmdp, cmd)
	SCR *sp;
	EXCMD *cmdp;
	enum which cmd;
{
	MARK rm;
	int flags, nf;
	char *name, *p;

	NEEDFILE(sp, cmdp->cmd);

	/* All write commands can have an associated '!'. */
	LF_INIT(FS_POSSIBLE);
	if (FL_ISSET(cmdp->iflags, E_C_FORCE))
		LF_SET(FS_FORCE);

	/* Skip any leading whitespace. */
	if (cmdp->argc != 0)
		for (p = cmdp->argv[0]->bp; *p && isblank(*p); ++p);

	/* If no arguments, just write the file back. */
	if (cmdp->argc == 0 || *p == '\0') {
		if (F_ISSET(cmdp, E_ADDR2_ALL))
			LF_SET(FS_ALL);
		return (file_write(sp,
		    &cmdp->addr1, &cmdp->addr2, NULL, flags));
	}

	/* If "write !" it's a pipe to a utility. */
	if (cmd == WRITE && *p == '!') {
		for (++p; *p && isblank(*p); ++p);
		if (*p == '\0') {
			ex_message(sp, cmdp->cmd->usage, EXM_USAGE);
			return (1);
		}
		/* Expand the argument. */
		if (argv_exp1(sp, cmdp, p, strlen(p), 1))
			return (1);
		if (filtercmd(sp, &cmdp->addr1, &cmdp->addr2,
		    &rm, cmdp->argv[1]->bp, FILTER_WRITE))
			return (1);
		sp->lno = rm.lno;
		return (0);
	}

	/* If "write >>" it's an append to a file. */
	if (cmd != XIT && p[0] == '>' && p[1] == '>') {
		LF_SET(FS_APPEND);

		/* Skip ">>" and whitespace. */
		for (p += 2; *p && isblank(*p); ++p);
	}

	/* Build an argv so we get an argument count and file expansion. */
	if (argv_exp2(sp, cmdp, p, strlen(p)))
		return (1);

	switch (cmdp->argc) {
	case 1:
		/*
		 * Nothing to expand, write the current file.
		 * XXX
		 * Should never happen, already checked this case.
		 */
		name = NULL;
		break;
	case 2:
		/* One new argument, write it. */
		name = cmdp->argv[EXP(sp)->argsoff - 1]->bp;

		/*
		 * !!!
		 * Historically, the read and write commands renamed
		 * "unnamed" files, or, if the file had a name, set
		 * the alternate file name.
		 */
		if (F_ISSET(sp->frp, FR_TMPFILE) &&
		    !F_ISSET(sp->frp, FR_EXNAMED)) {
			if ((p = v_strdup(sp,
			    cmdp->argv[1]->bp, cmdp->argv[1]->len)) != NULL) {
				free(sp->frp->name);
				sp->frp->name = p;
			}
			/*
			 * The file has a real name, it's no longer a
			 * temporary, clear the temporary file flags.
			 * The read-only flag follows the file name,
			 * clear it as well.
			 *
			 * !!!
			 * If we're writing the whole file, FR_NAMECHANGE
			 * will be cleared by the write routine -- this is
			 * historic practice.
			 */
			F_CLR(sp->frp, FR_RDONLY | FR_TMPEXIT | FR_TMPFILE);
			F_SET(sp->frp, FR_NAMECHANGE | FR_EXNAMED);
		} else
			set_alt_name(sp, name);
		break;
	default:
		/* If expanded to more than one argument, object. */
		p = msg_print(sp, cmdp->argv[0]->bp, &nf);
		msgq(sp, M_ERR, "172|%s expanded into too many file names", p);
		if (nf)
			FREE_SPACE(sp, p, 0);
		ex_message(sp, cmdp->cmd->usage, EXM_USAGE);
		return (1);
	}

	if (F_ISSET(cmdp, E_ADDR2_ALL))
		LF_SET(FS_ALL);
	return (file_write(sp, &cmdp->addr1, &cmdp->addr2, name, flags));
}

/*
 * ex_writefp --
 *	Write a range of lines to a FILE *.
 *
 * PUBLIC: int ex_writefp
 * PUBLIC:    __P((SCR *, char *, FILE *, MARK *, MARK *, u_long *, u_long *));
 */
int
ex_writefp(sp, name, fp, fm, tm, nlno, nch)
	SCR *sp;
	char *name;
	FILE *fp;
	MARK *fm, *tm;
	u_long *nlno, *nch;
{
	struct stat sb;
	u_long ccnt;			/* XXX: can't print off_t portably. */
	recno_t fline, tline, lcnt;
	size_t len;
	int nf, sv_errno;
	char *p;

	fline = fm->lno;
	tline = tm->lno;

	if (nlno != NULL) {
		*nch = 0;
		*nlno = 0;
	}

	/*
	 * The vi filter code has multiple processes running simultaneously,
	 * and one of them calls ex_writefp().  The "unsafe" function calls
	 * in this code are to file_gline() and msgq().  File_gline() is safe,
	 * see the comment in filter.c:filtercmd() for details.  We don't call
	 * msgq if the multiple process bit in the EXF is set.
	 *
	 * !!!
	 * Historic vi permitted files of 0 length to be written.  However,
	 * since the way vi got around dealing with "empty" files was to
	 * always have a line in the file no matter what, it wrote them as
	 * files of a single, empty line.  We write empty files.
	 *
	 * "Alex, I'll take vi trivia for $1000."
	 */
	ccnt = 0;
	lcnt = 0;
	if (tline != 0) {
		for (; fline <= tline; ++fline, ++lcnt) {
			/* Caller has to provide any interrupt message. */
			if (INTERRUPTED(sp))
				break;
			if ((p = file_gline(sp, fline, &len)) == NULL)
				break;
			if (fwrite(p, 1, len, fp) != len) {
				p = msg_print(sp, name, &nf);
				msgq(sp, M_SYSERR, "%s", p);
				if (nf)
					FREE_SPACE(sp, p, 0);
				(void)fclose(fp);
				return (1);
			}
			ccnt += len;
			if (putc('\n', fp) != '\n')
				break;
			++ccnt;
		}
	}

	/* If it's a regular file, sync it so that NFS is forced to flush. */
	if (!fstat(fileno(fp), &sb) &&
	    S_ISREG(sb.st_mode) && fsync(fileno(fp))) {
		sv_errno = errno;
		(void)fclose(fp);
		errno = sv_errno;
		goto err;
	}
	if (fclose(fp))
		goto err;
	if (nlno != NULL) {
		*nch = ccnt;
		*nlno = lcnt;
	}
	return (0);

err:	if (!F_ISSET(sp->ep, F_MULTILOCK)) {
		p = msg_print(sp, name, &nf);
		msgq(sp, M_SYSERR, "%s", p);
		if (nf)
			FREE_SPACE(sp, p, 0);
	}
	return (1);
}
