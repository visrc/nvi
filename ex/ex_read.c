/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_read.c,v 5.7 1992/04/22 08:08:03 bostic Exp $ (Berkeley) $Date: 1992/04/22 08:08:03 $";
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
	int force, rval;
	char *fname;

	/* If nothing, just read the file. */
	if ((p = cmdp->string) == NULL) {
		if (!*origname) {
			msg("No filename from which to read.");
			return (1);
		}
		fname = origname;
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
		return (filter(cmdp->addr1, MARK_UNSET, ++p, NOINPUT));
	}

	/* Build an argv. */
	if (buildargv(p, 1, cmdp))
		return (1);

	switch(cmdp->argc) {
	case 0:
		fname = origname;
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

	rval = ex_readfp(fname, fp, cmdp->addr1, &rptlines);

	(void)fclose(fp);

	if (rval)
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
ex_readfp(fname, fp, addr, cntp)
	char *fname;
	FILE *fp;
	MARK addr;
	long *cntp;
{
	size_t len;
	long lno, startlno;
	char *p;

	/* Add in the lines from the output. */
	ChangeText
	{
		/* Insertion starts at the line following the address. */
		addr = (addr | (BLKSIZE - 1L)) + 1L;

		/*
		 * XXX
		 * This code doesn't check for lines that are too long.
		 * Also, the current add module requires both a newline
		 * and a terminating NULL.  This is, of course, stupid.
		 */
		startlno = lno = markline(addr);
		while (p = fgetline(fp, &len)) {
			char __buf[8 * 1024];		/* XXX */
			bcopy(p, __buf, len);		/* XXX */
			__buf[len] = '\n';		/* XXX */
			__buf[len + 1] = '\0';		/* XXX */

			add(addr, __buf);		/* XXX */
			addr = MARK_AT_LINE(++lno);
		}
		if (ferror(fp)) {
			msg("%s: %s", strerror(errno));
			return (1);
		}
	}

	/* Set cursor to first line read in. */
	cursor = MARK_AT_LINE(startlno);

	/* Return the number of lines read in. */
	if (cntp)
		*cntp = lno - startlno;

	return (0);
}
