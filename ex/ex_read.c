/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_read.c,v 5.17 1992/11/07 13:42:14 bostic Exp $ (Berkeley) $Date: 1992/11/07 13:42:14 $";
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
#include "extern.h"

/*
 * ex_read --	:read[!] [file]
 *		:read [! cmd]
 *	Read from a file or utility.
 */
int
ex_read(cmdp)
	EXCMDARG *cmdp;
{
	register u_char *p;
	struct stat sb;
	FILE *fp;
	int force;
	char *fname;

	/* If nothing, just read the file. */
	if ((p = cmdp->string) == NULL) {
		if (FF_ISSET(curf, F_NONAME)) {
			msg("No filename from which to read.");
			return (1);
		}
		fname = curf->name;
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
			msg("Usage: %s.", cmdp->cmd->usage);
			return (1);
		}
		return (filter(&cmdp->addr1, NULL, ++p, NOINPUT));
	}

	/* Build an argv. */
	if (buildargv(p, 1, cmdp))
		return (1);

	switch(cmdp->argc) {
	case 0:
		fname = curf->name;
		break;
	case 1:
		fname = (char *)cmdp->argv[0];
		break;
	default:
		msg("Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	/* Open the file. */
noargs:	if ((fp = fopen(fname, "r")) == NULL || fstat(fileno(fp), &sb)) {
		msg("%s: %s", fname, strerror(errno));
		return (1);
	}

	/* If not a regular file, force must be set. */
	if (!force && !S_ISREG(sb.st_mode)) {
		msg("%s is not a regular file -- use ! to read it.", fname);
		return (1);
	}

	if (ex_readfp(fname, fp, &cmdp->addr1, &curf->rptlines))
		return (1);

	autoprint = 1;

	curf->rptlabel = "read";
	return (0);
}

/*
 * ex_readfp --
 *	Read lines into the file.
 *	Two side effects:
 *		The cursor position gets set, and autoprint gets set.
 */
int
ex_readfp(fname, fp, fm, cntp)
	char *fname;
	FILE *fp;
	MARK *fm;
	recno_t *cntp;
{
	size_t len;
	long lno;
	int rval;
	char *p;

	/*
	 * Add in the lines from the output.  Insertion starts at the line
	 * following the address.
	 *
	 * XXX
	 * This code doesn't check for lines that are too long.  Also, the
	 * current add module requires both a newline and a terminating NULL.
	 * This is, of course, stupid.
	 */
	rval = 0;
	for (lno = fm->lno; p = fgetline(fp, &len); ++lno)
		if (file_aline(curf, lno, (u_char *)p, len)) {
			rval = 1;
			break;
		}

	if (ferror(fp)) {
		msg("%s: %s", strerror(errno));
		rval = 1;
	}

	if (fclose(fp)) {
		msg("%s: %s", strerror(errno));
		return (1);
	}

	if (rval)
		return (1);

	/* Return the number of lines read in. */
	if (cntp)
		*cntp = lno - fm->lno;

	return (0);
}
