/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_tag.c,v 5.1 1992/04/02 11:21:16 bostic Exp $ (Berkeley) $Date: 1992/04/02 11:21:16 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "pathnames.h"
#include "extern.h"

void
ex_tag(cmdp)
	CMDARG *cmdp;
{
	int	fd;	/* file descriptor used to read the file */
	char	*scan;	/* used to scan through the tmpblk.c */
#ifndef EXTERNAL_TAGS
	char	*cmp;	/* char of tag name we're comparing, or NULL */
	char	*end;	/* marks the end of chars in tmpblk.c */
#else
	int	i;
#endif
#ifndef NO_MAGIC
	char	wasmagic; /* preserves the original state of O_MAGIC */
#endif
	static char prevtag[30];
	char *extra;

	extra = cmdp->argv[0];
	/* if no tag is given, use the previous tag */
	if (!extra || !*extra)
	{
		if (!*prevtag)
		{
			msg("No previous tag");
			return;
		}
		extra = prevtag;
	}
	else
	{
		strncpy(prevtag, extra, sizeof prevtag);
		prevtag[sizeof prevtag - 1] = '\0';
	}

	/* open the tags file */
	fd = open(_NAME_TAGS, O_RDONLY);
	if (fd < 0)
	{
		msg("No tags file");
		return;
	}

	/* Hmmm... this would have been a lot easier with <stdio.h> */

	/* find the line with our tag in it */
	for(scan = end = tmpblk.c, cmp = extra; ; scan++)
	{
		/* read a block, if necessary */
		if (scan >= end)
		{
			end = tmpblk.c + read(fd, tmpblk.c, BLKSIZE);
			scan = tmpblk.c;
			if (scan >= end)
			{
				msg("tag \"%s\" not found", extra);
				close(fd);
				return;
			}
		}

		/* if we're comparing, compare... */
		if (cmp)
		{
			/* matched??? wow! */
			if (!*cmp && *scan == '\t')
			{
				break;
			}
			if (*cmp++ != *scan)
			{
				/* failed! skip to newline */
				cmp = (char *)0;
			}
		}

		/* if we're skipping to newline, do it fast! */
		if (!cmp)
		{
			while (scan < end && *scan != '\n')
			{
				scan++;
			}
			if (scan < end)
			{
				cmp = extra;
			}
		}
	}

	/* found it! get the rest of the line into memory */
	for (cmp = tmpblk.c, scan++; scan < end && *scan != '\n'; )
	{
		*cmp++ = *scan++;
	}
	if (scan == end)
	{
		read(fd, cmp, BLKSIZE - (int)(cmp - tmpblk.c));
	}

	/* we can close the tags file now */
	close(fd);

	/* extract the filename from the line, and edit the file */
	for (scan = tmpblk.c; *scan != '\t'; scan++)
	{
	}
	*scan++ = '\0';
	if (strcmp(origname, tmpblk.c) != 0)
	{
		if (!tmpabort(cmdp->flags & E_FORCE))
		{
			msg("Use :tag! to abort changes, or :w to save changes");
			return;
		}
		tmpstart(tmpblk.c);
	}

	/* move to the desired line (or to line 1 if that fails) */
#ifndef NO_MAGIC
	wasmagic = ISSET(O_MAGIC);
	UNSET(O_MAGIC);
#endif
	cursor = MARK_FIRST;
{
	CMDARG __xxx;
	linespec(scan, &__xxx);
	cursor = __xxx.addr1;
}
	if (cursor == MARK_UNSET)
	{
		cursor = MARK_FIRST;
		msg("Tag's address is out of date");
	}
#ifndef NO_MAGIC
	if (wasmagic)
		SET(O_MAGIC);
#endif
}
