/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_read.c,v 5.24 1993/02/16 20:10:21 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:10:21 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
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
ex_read(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	register u_char *p;
	struct stat sb;
	FILE *fp;
	int force;
	char *fname;

	/* If nothing, just read the file. */
	if ((p = cmdp->string) == NULL) {
		if (FF_ISSET(ep, F_NONAME)) {
			msg(ep, M_ERROR, "No filename from which to read.");
			return (1);
		}
		fname = ep->name;
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
	for (; *p && isspace(*p); ++p);

	/* If "read !" it's a pipe from a utility. */
	if (*p == '!') {
		for (; *p && isspace(*p); ++p);
		if (*p == '\0') {
			msg(ep, M_ERROR, "Usage: %s.", cmdp->cmd->usage);
			return (1);
		}
		return (filtercmd(ep, &cmdp->addr1, NULL, ++p, NOINPUT));
	}

	/* Build an argv. */
	if (buildargv(ep, p, 1, cmdp))
		return (1);

	switch(cmdp->argc) {
	case 0:
		fname = ep->name;
		break;
	case 1:
		fname = (char *)cmdp->argv[0];
		break;
	default:
		msg(ep, M_ERROR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	/* Open the file. */
noargs:	if ((fp = fopen(fname, "r")) == NULL || fstat(fileno(fp), &sb)) {
		msg(ep, M_ERROR, "%s: %s", fname, strerror(errno));
		return (1);
	}

	/* If not a regular file, force must be set. */
	if (!force && !S_ISREG(sb.st_mode)) {
		msg(ep, M_ERROR,
		    "%s is not a regular file -- use ! to read it.", fname);
		return (1);
	}

	if (ex_readfp(ep, fname, fp, &cmdp->addr1, &ep->rptlines))
		return (1);

	/* Set the cursor. */
	ep->lno = cmdp->addr1.lno + 1;
	ep->cno = 0;
	
	/* Set autoprint. */
	FF_SET(ep, F_AUTOPRINT);

	/* Set reporting. */
	ep->rptlabel = "read";
	return (0);
}

/*
 * ex_readfp --
 *	Read lines into the file.
 */
int
ex_readfp(ep, fname, fp, fm, cntp)
	EXF *ep;
	char *fname;
	FILE *fp;
	MARK *fm;
	recno_t *cntp;
{
	size_t len;
	recno_t lno;
	int rval;
	u_char *p;

	/*
	 * Add in the lines from the output.  Insertion starts at the line
	 * following the address.
	 */
	rval = 0;
	for (lno = fm->lno; p = ex_getline(ep, fp, &len); ++lno)
		if (file_aline(ep, lno, p, len)) {
			rval = 1;
			break;
		}

	if (ferror(fp)) {
		msg(ep, M_ERROR, "%s: %s", strerror(errno));
		rval = 1;
	}

	if (fclose(fp)) {
		msg(ep, M_ERROR, "%s: %s", strerror(errno));
		return (1);
	}

	if (rval)
		return (1);

	/* Return the number of lines read in. */
	if (cntp)
		*cntp = lno - fm->lno;

	return (0);
}
