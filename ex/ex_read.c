/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_read.c,v 8.7 1993/08/25 16:44:58 bostic Exp $ (Berkeley) $Date: 1993/08/25 16:44:58 $";
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
 * ex_read --	:read[!] [file]
 *		:read [! cmd]
 *	Read from a file or utility.
 */
int
ex_read(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	register char *p;
	struct stat sb;
	FILE *fp;
	MARK rm;
	recno_t nlines;
	int force, rval;
	char *fname;

	/* If nothing, just read the file. */
	if ((p = cmdp->argv[0]) == NULL) {
		if (F_ISSET(sp->frp, FR_NONAME)) {
			msgq(sp, M_ERR, "No filename from which to read.");
			return (1);
		}
		fname = sp->frp->fname;
		force = 0;
		goto noargs;
	}

	/* If "read!" it's a force from a file. */
	if (*p == '!') {
		force = 1;
		++p;
	} else
		force = 0;

	/* Skip whitespace. */
	for (; *p && isblank(*p); ++p);

	/* If "read !" it's a pipe from a utility. */
	if (*p == '!') {
		for (; *p && isblank(*p); ++p);
		if (*p == '\0') {
			msgq(sp, M_ERR, "Usage: %s.", cmdp->cmd->usage);
			return (1);
		}
		if (filtercmd(sp, ep, &cmdp->addr1, NULL, &rm, ++p, NOINPUT))
			return (1);
		sp->lno = rm.lno;
		return (0);
	}

	/* Build an argv. */
	if (file_argv(sp, ep, p, &cmdp->argc, &cmdp->argv))
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

	/* Open the file. */
noargs:	if ((fp = fopen(fname, "r")) == NULL || fstat(fileno(fp), &sb)) {
		msgq(sp, M_ERR, "%s: %s", fname, strerror(errno));
		return (1);
	}

	/* If not a regular file, force must be set. */
	if (!force && !S_ISREG(sb.st_mode)) {
		msgq(sp, M_ERR,
		    "%s is not a regular file -- use ! to read it.", fname);
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
	register u_long ccnt;
	size_t len;
	recno_t lno, nlines;
	int rval;

	/*
	 * Add in the lines from the output.  Insertion starts at the line
	 * following the address.
	 */
	ccnt = 0;
	rval = 0;
	for (lno = fm->lno; !ex_getline(sp, fp, &len); ++lno) {
		if (file_aline(sp, ep, 1, lno, sp->ibp, len)) {
			rval = 1;
			break;
		}
		ccnt += len;
	}

	if (ferror(fp)) {
		msgq(sp, M_ERR, "%s: %s", fname, strerror(errno));
		rval = 1;
	}

	if (fclose(fp)) {
		msgq(sp, M_ERR, "%s: %s", fname, strerror(errno));
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
