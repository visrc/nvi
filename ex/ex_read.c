/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_read.c,v 8.18 1993/12/19 12:59:19 bostic Exp $ (Berkeley) $Date: 1993/12/19 12:59:19 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_read --	:read [file]
 *		:read [! cmd]
 *	Read from a file or utility.
 */
int
ex_read(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	struct stat sb;
	FILE *fp;
	MARK rm;
	recno_t nlines;
	size_t blen, len;
	int rval;
	char *bp, *name;

	/*
	 * If "read !", it's a pipe from a utility.
	 *
	 * !!!
	 * Historical vi wouldn't undo a filter read, for no apparent
	 * reason.
	 */
	if (F_ISSET(cmdp, E_FORCE)) {
		/* Expand the user's argument. */
		if (argv_exp1(sp, ep,
		    cmdp, cmdp->argv[0]->bp, cmdp->argv[0]->len, 0))
			return (1);

		/* If argc still 1, there wasn't anything to expand. */
		if (cmdp->argc == 1) {
			msgq(sp, M_ERR, "Usage: %s.", cmdp->cmd->usage);
			return (1);
		}

		/* Redisplay the user's argument if it's changed. */
		if (F_ISSET(cmdp, E_MODIFY) && IN_VI_MODE(sp)) {
			len = cmdp->argv[1]->len;
			GET_SPACE_RET(sp, bp, blen, len + 2);
			bp[0] = '!';
			memmove(bp + 1, cmdp->argv[1], cmdp->argv[1]->len + 1);
			(void)sp->s_busy(sp, bp);
			FREE_SPACE(sp, bp, blen);
		}

		if (filtercmd(sp, ep,
		    &cmdp->addr1, NULL, &rm, cmdp->argv[1]->bp, FILTER_READ))
			return (1);
		sp->lno = rm.lno;
		return (0);
	}

	/* Expand the user's argument. */
	if (argv_exp2(sp, ep,
	    cmdp, cmdp->argv[0]->bp, cmdp->argv[0]->len, 0))
		return (1);

	switch (cmdp->argc) {
	case 1:
		/* Nothing to expand, read the current file. */
		name = FILENAME(sp->frp);
		if (sp->alt_name == NULL)
			set_alt_name(sp, name);
		break;
	case 2:
		/* One new argument, read it. */
		name = cmdp->argv[1]->bp;
		break;
	default:
		/* If expanded to more than one argument, object. */
		msgq(sp, M_ERR, "%s expanded into too many file names",
		    cmdp->argv[0]->bp);
		msgq(sp, M_ERR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	/*
	 * !!!
	 * Historically, vi did not permit reads from non-regular files,
	 * nor did it distinguish between "read !" and "read!", so there
	 * was no way to "force" it.
	 */
	if ((fp = fopen(name, "r")) == NULL || fstat(fileno(fp), &sb)) {
		msgq(sp, M_SYSERR, name);
		return (1);
	}
	if (!S_ISREG(sb.st_mode)) {
		(void)fclose(fp);
		msgq(sp, M_ERR, "Only regular files may be read.");
		return (1);
	}

	rval = ex_readfp(sp, ep, name, fp, &cmdp->addr1, &nlines, 1);

	/*
	 * Set the cursor to the first line read in, if anything read
	 * in, otherwise, the address.  (Historic vi set it to the
	 * line after the address regardless, but since that line may
	 * not exist we don't bother.)
	 */
	sp->lno = cmdp->addr1.lno;
	if (nlines)
		++sp->lno;
	
	/* Set autoprint. */
	F_SET(sp, S_AUTOPRINT);

	return (rval);
}

/*
 * ex_readfp --
 *	Read lines into the file.
 */
int
ex_readfp(sp, ep, name, fp, fm, nlinesp, success_msg)
	SCR *sp;
	EXF *ep;
	char *name;
	FILE *fp;
	MARK *fm;
	recno_t *nlinesp;
	int success_msg;
{
	EX_PRIVATE *exp;
	u_long ccnt;
	size_t len;
	recno_t lno, nlines;
	int rval;

	/*
	 * Add in the lines from the output.  Insertion starts at the line
	 * following the address.
	 */
	ccnt = 0;
	rval = 0;
	exp = EXP(sp);
	for (lno = fm->lno; !ex_getline(sp, fp, &len); ++lno) {
		if (file_aline(sp, ep, 1, lno, exp->ibp, len)) {
			rval = 1;
			break;
		}
		ccnt += len;
	}

	if (ferror(fp)) {
		msgq(sp, M_SYSERR, name);
		rval = 1;
	}

	if (fclose(fp)) {
		msgq(sp, M_SYSERR, name);
		return (1);
	}

	if (rval)
		return (1);

	/* Return the number of lines read in. */
	nlines = lno - fm->lno;
	if (nlinesp != NULL)
		*nlinesp = nlines;

	if (success_msg)
		msgq(sp, M_INFO, "%s: %lu line%s, %lu characters.",
		    name, nlines, nlines == 1 ? "" : "s", ccnt);

	return (0);
}
