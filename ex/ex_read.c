/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_read.c,v 5.1 1992/04/02 11:21:09 bostic Exp $ (Berkeley) $Date: 1992/04/02 11:21:09 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

void
ex_read(cmdp)
	CMDARG *cmdp;
{
	int	fd, rc;	/* used while reading from the file */
	char	*scan;	/* used for finding NUL characters */
	int	hadnul;	/* boolean: any NULs found? */
	int	addnl;	/* boolean: forced to add newlines? */
	int	len;	/* number of chars in current line */
	long	lines;	/* number of lines in current block */
	struct stat statb;

	/* special case: if ":r !cmd" then let the filter() function do it */
	char *extra;
	extra = cmdp->argv[0];
	if (extra[0] == '!')
	{
		filter(cmdp->addr1, MARK_UNSET, extra + 1);
		return;
	}

	/* open the file */
	fd = open(extra, O_RDONLY);
	if (fd < 0)
	{
		msg("Can't open \"%s\"", extra);
		return;
	}

	if (stat(extra, &statb) < 0)
	{
		msg("Can't stat \"%s\"", extra);
	}
	if ((statb.st_mode & S_IFMT) != S_IFREG)
	{
		msg("\"%s\" is not a regular file", extra);
		return;
	}

	/* get blocks from the file, and add them */
	ChangeText
	{
		/* insertion starts at the line following frommark */
		cmdp->addr2 = cmdp->addr1 = (cmdp->addr1 | (BLKSIZE - 1L)) + 1L;
		len = 0;
		hadnul = addnl = FALSE;

		/* add an extra newline, so partial lines at the end of
		 * the file don't trip us up
		 */
		add(cmdp->addr2, "\n");

		/* for each chunk of text... */
		while ((rc = read(fd, tmpblk.c, BLKSIZE - 1)) > 0)
		{
			/* count newlines, convert NULs, etc. ... */
			for (lines = 0, scan = tmpblk.c; rc > 0; rc--, scan++)
			{
				/* break up long lines */
				if (*scan != '\n' && len + 2 > BLKSIZE)
				{
					*scan = '\n';
					addnl = TRUE;
				}

				/* protect against NUL chars in file */
				if (!*scan)
				{
					*scan = 0x80;
					hadnul = TRUE;
				}

				/* starting a new line? */
				if (*scan == '\n')
				{
					/* reset length at newline */
					len = 0;
					lines++;
				}
				else
				{
					len++;
				}
			}

			/* add the text */
			*scan = '\0';
			add(cmdp->addr2, tmpblk.c);
			cmdp->addr2 += MARK_AT_LINE(lines) + len - markidx(cmdp->addr2);
		}

		/* if partial last line, then retain that first newline */
		if (len > 0)
		{
			msg("Last line had no newline");
			cmdp->addr2 += BLKSIZE; /* <- for the rptlines calc */
		}
		else /* delete that first newline */
		{
			delete(cmdp->addr2, (cmdp->addr2 | (BLKSIZE - 1L)) + 1L);
		}
	}

	/* close the file */
	close(fd);

	/* Reporting... */
	rptlines = markline(cmdp->addr2) - markline(cmdp->addr1);
	rptlabel = "read";
	cursor = (cmdp->addr2 & ~BLKSIZE) - BLKSIZE;

	if (addnl)
		msg("Newlines were added to break up long lines");
	if (hadnul)
		msg("NULs were converted to 0x80");
}
