/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_read.c,v 10.15 1995/10/17 08:08:40 bostic Exp $ (Berkeley) $Date: 1995/10/17 08:08:40 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/common.h"
#include "../vi/vi.h"

/*
 * ex_read --	:read [file]
 *		:read [!cmd]
 *	Read from a file or utility.
 *
 * !!!
 * Historical vi wouldn't undo a filter read, for no apparent reason.
 *
 * PUBLIC: int ex_read __P((SCR *, EXCMD *));
 */
int
ex_read(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	enum { R_ARG, R_EXPANDARG, R_FILTER } which;
	struct stat sb;
	CHAR_T *arg, *name;
	EX_PRIVATE *exp;
	FILE *fp;
	FREF *frp;
	GS *gp;
	MARK rm;
	recno_t nlines;
	size_t arglen;
	int argc, rval;
	char *p;

	gp = sp->gp;

	/*
	 * 0 args: read the current pathname.
	 * 1 args: check for "read !arg".
	 */
	switch (cmdp->argc) {
	case 0:
		which = R_ARG;
		break;
	case 1:
		arg = cmdp->argv[0]->bp;
		arglen = cmdp->argv[0]->len;
		if (*arg == '!') {
			++arg;
			--arglen;
			which = R_FILTER;
		} else
			which = R_EXPANDARG;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	/* Load a temporary file if no file being edited. */
	if (sp->ep == NULL) {
		if ((frp = file_add(sp, NULL)) == NULL)
			return (1);
		if (file_init(sp, frp, NULL, 0))
			return (1);
	}

	switch (which) {
	case R_FILTER:
		/*
		 * File name and bang expand the user's argument.  If
		 * we don't get an additional argument, it's illegal.
		 */
		argc = cmdp->argc;
		if (argv_exp1(sp, cmdp, arg, arglen, 1))
			return (1);
		if (argc == cmdp->argc)
			goto usage;
		argc = cmdp->argc - 1;

		/* Set the last bang command. */
		exp = EXP(sp);
		if (exp->lastbcomm != NULL)
			free(exp->lastbcomm);
		if ((exp->lastbcomm =
		    strdup(cmdp->argv[argc]->bp)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}

		/* Redisplay the user's argument if it's changed. */
		if (F_ISSET(cmdp, E_MODIFY))
			if (F_ISSET(sp, S_VI))
				(void)vs_update(sp, "!", cmdp->argv[argc]->bp);
			else {
				(void)ex_printf(sp,
				    "!%s\n", cmdp->argv[argc]->bp);
				(void)ex_fflush(sp);
			}

		if (filtercmd(sp, cmdp, &cmdp->addr1,
		    NULL, &rm, cmdp->argv[argc]->bp, FILTER_READ))
			return (1);

		/* The filter version of read set the autoprint flag. */
		F_SET(cmdp, E_AUTOPRINT);

		/* If in vi mode, move to the first nonblank. */
		sp->lno = rm.lno;
		if (F_ISSET(sp, S_VI)) {
			sp->cno = 0;
			(void)nonblank(sp, sp->lno, &sp->cno);
		}
		return (0);
	case R_ARG:
		name = sp->frp->name;
		break;
	case R_EXPANDARG:
		if (argv_exp2(sp, cmdp, arg, arglen))
			return (1);
		/*
		 * 0/1 args: impossible.
		 *   2 args: read it.
		 *  >2 args: object, too many args.
		 */
		switch (cmdp->argc) {
		case 0:
		case 1:
			abort();
			/* NOTREACHED */
		case 2:
			name = cmdp->argv[1]->bp;
			/*
			 * !!!
			 * Historically, the read and write commands renamed
			 * "unnamed" files, or, if the file had a name, set
			 * the alternate file name.
			 */
			if (F_ISSET(sp->frp, FR_TMPFILE) &&
			    !F_ISSET(sp->frp, FR_EXNAMED)) {
				if ((p = v_strdup(sp, cmdp->argv[1]->bp,
				    cmdp->argv[1]->len)) != NULL) {
					free(sp->frp->name);
					sp->frp->name = p;
				}
				/*
				 * The file has a real name, it's no longer a
				 * temporary, clear the temporary file flags.
				 * The read-only flag follows the file name,
				 * clear it as well.
				 */
				F_CLR(sp->frp,
				    FR_RDONLY | FR_TMPEXIT | FR_TMPFILE);
				F_SET(sp->frp, FR_NAMECHANGE | FR_EXNAMED);

				/* Notify the screen. */
				(void)gp->scr_rename(sp);
			} else
				set_alt_name(sp, name);
			break;
		default:
			msgq_str(sp, M_ERR, cmdp->argv[0]->bp,
			    "144|%s expanded into too many file names");
usage:			ex_emsg(sp, cmdp->cmd->usage, EXM_USAGE);
			return (1);
		
		}
		break;
	}

	/*
	 * !!!
	 * Historically, vi did not permit reads from non-regular files,
	 * nor did it distinguish between "read !" and "read!", so there
	 * was no way to "force" it.
	 */
	if ((fp = fopen(name, "r")) == NULL || fstat(fileno(fp), &sb)) {
		msgq_str(sp, M_SYSERR, name, "%s");
		return (1);
	}
	if (!S_ISREG(sb.st_mode)) {
		(void)fclose(fp);
		msgq(sp, M_ERR, "145|Only regular files may be read");
		return (1);
	}

	/* Try and get a lock. */
	if (file_lock(sp, NULL, NULL, fileno(fp), 0) == LOCK_UNAVAIL)
		msgq(sp, M_ERR, "146|%s: read lock was unavailable", name);

	/* Turn on busy message. */
	(void)gp->scr_busy(sp, "147|Reading...", BUSY_ON);
	rval = ex_readfp(sp, name, fp, &cmdp->addr1, &nlines, 1);
	(void)gp->scr_busy(sp, NULL, BUSY_OFF);

	/*
	 * Set the cursor to the first line read in, if anything read
	 * in, otherwise, the address.  (Historic vi set it to the
	 * line after the address regardless, but since that line may
	 * not exist we don't bother.)
	 */
	sp->lno = cmdp->addr1.lno;
	if (nlines)
		++sp->lno;

	return (rval);
}

/*
 * ex_readfp --
 *	Read lines into the file.
 *
 * PUBLIC: int ex_readfp __P((SCR *, char *, FILE *, MARK *, recno_t *, int));
 */
int
ex_readfp(sp, name, fp, fm, nlinesp, success_msg)
	SCR *sp;
	char *name;
	FILE *fp;
	MARK *fm;
	recno_t *nlinesp;
	int success_msg;
{
	EX_PRIVATE *exp;
	recno_t lcnt, lno;
	size_t len;
	u_long ccnt;			/* XXX: can't print off_t portably. */
	int nf;
	char *p;

	exp = EXP(sp);

	/*
	 * Add in the lines from the output.  Insertion starts at the line
	 * following the address.
	 */
	ccnt = 0;
	lcnt = 0;
	for (lno = fm->lno; !ex_getline(sp, fp, &len); ++lno, ++lcnt) {
		if ((lcnt % INTERRUPT_CHECK) == 0) {
			if (INTERRUPTED(sp))
				break;
			(void)sp->gp->scr_busy(sp, NULL, BUSY_UPDATE);
		}
		if (db_append(sp, 1, lno, exp->ibp, len))
			goto err;
		ccnt += len;
	}

	if (ferror(fp) || fclose(fp))
		goto err;

	/* Return the number of lines read in. */
	if (nlinesp != NULL)
		*nlinesp = lcnt;

	if (success_msg) {
		p = msg_print(sp, name, &nf);
		msgq(sp, M_INFO, "148|%s: %lu lines, %lu characters",
		    p, lcnt, ccnt);
		if (nf)
			FREE_SPACE(sp, p, 0);
	}
	return (0);

err:	msgq_str(sp, M_SYSERR, name, "%s");
	(void)fclose(fp);
	return (1);
}
