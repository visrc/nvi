/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_read.c,v 8.12 1993/11/13 18:02:26 bostic Exp $ (Berkeley) $Date: 1993/11/13 18:02:26 $";
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
	char *bp, *fname;

	/* If "read !", it's a pipe from a utility. */
	if (F_ISSET(cmdp, E_FORCE)) {
		if (cmdp->argv[0][0] == '\0') {
			msgq(sp, M_ERR, "Usage: %s.", cmdp->cmd->usage);
			return (1);
		}
		if (argv_exp1(sp, ep, cmdp, cmdp->argv[0], 0))
			return (1);
		if (F_ISSET(cmdp, E_MODIFY) && IN_VI_MODE(sp)) {
			len = strlen(cmdp->argv[0]);
			GET_SPACE(sp, bp, blen, len + 2);
			bp[0] = '!';
			memmove(bp + 1, cmdp->argv[0], len + 1);
			(void)sp->s_busy(sp, bp);
			FREE_SPACE(sp, bp, blen);
		}
		if (filtercmd(sp, ep,
		    &cmdp->addr1, NULL, &rm, cmdp->argv[0], FILTER_READ))
			return (1);
		sp->lno = rm.lno;
		return (0);
	}

	/* If no file arguments, read the alternate file. */
	if (cmdp->argv[0][0] == '\0') {
		if (sp->alt_fname == NULL) {
			msgq(sp, M_ERR,
			    "No default filename from which to read.");
			return (1);
		}
		fname = sp->alt_fname;
	} else {
		if (argv_exp2(sp, ep, cmdp, cmdp->argv[0], 0))
			return (1);

		switch (cmdp->argc) {
		case 0:
			fname = sp->frp->fname;
			break;
		case 1:
			fname = (char *)cmdp->argv[0];
			set_alt_fname(sp, fname);
			break;
		default:
			msgq(sp, M_ERR, "Usage: %s.", cmdp->cmd->usage);
			return (1);
		}
	}

	/*
	 * !!!
	 * Vi, historically did not permit reads from non-regular files,
	 * nor did it distinguish between "read !" and "read!", so there
	 * was no way to "force" it.
	 */
	if ((fp = fopen(fname, "r")) == NULL || fstat(fileno(fp), &sb)) {
		msgq(sp, M_SYSERR, fname);
		return (1);
	}
	if (!S_ISREG(sb.st_mode)) {
		(void)fclose(fp);
		msgq(sp, M_ERR, "Only regular files may be read.");
		return (1);
	}

	rval = ex_readfp(sp, ep, fname, fp, &cmdp->addr1, &nlines, 1);

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
ex_readfp(sp, ep, fname, fp, fm, nlinesp, success_msg)
	SCR *sp;
	EXF *ep;
	char *fname;
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
		msgq(sp, M_SYSERR, fname);
		rval = 1;
	}

	if (fclose(fp)) {
		msgq(sp, M_SYSERR, fname);
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
		    fname, nlines, nlines == 1 ? "" : "s", ccnt);

	return (0);
}
