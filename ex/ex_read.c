/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_read.c,v 5.11 1992/06/07 13:46:58 bostic Exp $ (Berkeley) $Date: 1992/06/07 13:46:58 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
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
	register char *p;
	struct stat sb;
	FILE *fp;
	int force;
	char *fname;

	/* If nothing, just read the file. */
	if ((p = cmdp->string) == NULL) {
		if (curf->flags & F_NONAME) {
			msg("No filename from which to read.");
			return (1);
		}
		fname = curf->name;
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
		fname = cmdp->argv[0];
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

	if (ex_readfp(fname, fp, &cmdp->addr1, &rptlines))
		return (1);

	autoprint = 1;

	rptlabel = "read";
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
	long *cntp;
{
	MARK m;
	size_t len;
	long lno;
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
	m = *fm;
	for (lno = ++m.lno; p = fgetline(fp, &len); ++m.lno)
		add(&m, p, len);

	if (ferror(fp)) {
		msg("%s: %s", strerror(errno));
		(void)fclose(fp);
		return (1);
	}
	if (fclose(fp)) {
		msg("%s: %s", strerror(errno));
		return (1);
	}

	/*
	 * Set cursor to first line read in.
	 * XXX
	 * Should probably be set by the caller.
	 */
	curf->lno = lno;

	/* Return the number of lines read in. */
	if (cntp)
		*cntp = m.lno - fm->lno;

	return (0);
}
