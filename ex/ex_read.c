/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_read.c,v 5.27 1993/02/24 12:56:14 bostic Exp $ (Berkeley) $Date: 1993/02/24 12:56:14 $";
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
#include "screen.h"

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
	MARK rm;
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
		if (filtercmd(ep, &cmdp->addr1, NULL, &rm, ++p, NOINPUT))
			return (1);
		SCRLNO(ep) = rm.lno;
		return (0);
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
	SCRLNO(ep) = cmdp->addr1.lno + 1;
	SCRCNO(ep) = 0;
	
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
	 * There is one very nasty special case.  The historic vi code displays
	 * a single space (or a '$' if the list option is set) for the first
	 * line in an "empty" file.  If we "insert" a line, that line gets
	 * scrolled down, not repainted, so it's incorrect when we refresh the
	 * the screen.  This is really hard to find and fix in the vi code --
	 * the text input functions detect it explicitly and don't insert a new
	 * line.  The hack here is to repaint the screen if we're appending to
	 * an empty file.
	 */
	if (file_lline(ep) == 0)
		SF_SET(ep, S_REDRAW);

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
		msg(ep, M_ERROR, "%s: %s", fname, strerror(errno));
		rval = 1;
	}

	if (fclose(fp)) {
		msg(ep, M_ERROR, "%s: %s", fname, strerror(errno));
		return (1);
	}

	if (rval)
		return (1);

	/* Return the number of lines read in. */
	if (cntp)
		*cntp = lno - fm->lno;

	return (0);
}
