/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_write.c,v 5.2 1992/04/04 10:02:50 bostic Exp $ (Berkeley) $Date: 1992/04/04 10:02:50 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

void
ex_write(cmdp)
	CMDARG *cmdp;
{
	int		fd;
	int		append;	/* boolean: write in "append" mode? */
	REG long	l;
	REG char	*scan;
	REG int		i;
	char	*extra;
/*
 * XXX
 * If no address, do all addresses.
 */
	extra = cmdp->argv[0];

	/* if all lines are to be written, use tmpsave() */
	if (cmdp->addr1 == MARK_FIRST && cmdp->addr2 == MARK_LAST)
	{
		tmpsave(extra, cmdp->flags & E_FORCE);
		return;
	}

	/* see if we're going to do this in append mode or not */
	append = FALSE;
	if (extra[0] == '>' && extra[1] == '>')
	{
		extra += 2;
		append = TRUE;
	}

	/* either the file must not exist, or we must have a ! or be appending */
	if (access(extra, 0) == 0 && !cmdp->flags & E_FORCE && !append)
	{
		msg("File already exists - Use :w! to overwrite");
		return;
	}

	/* else do it line-by-line, like ex_print() */
	if (append)
	{
#ifdef O_APPEND
		fd = open(extra, O_WRONLY|O_APPEND);
#else
		fd = open(extra, O_WRONLY);
		if (fd >= 0)
		{
			lseek(fd, 0L, 2);
		}
#endif
	}
	else
	{
		fd = -1; /* so we know the file isn't open yet */
	}

	if (fd < 0)
	{
		fd = creat(extra, DEFFILEMODE);
		if (fd < 0)
		{
			msg("Can't write to \"%s\"", extra);
			return;
		}
	}
	for (l = markline(cmdp->addr1); l <= markline(cmdp->addr2); l++)
	{
		/* get the next line */
		scan = fetchline(l);
		i = strlen(scan);
		scan[i++] = '\n';

		/* print the line */
		write(fd, scan, i);
	}
	close(fd);
}	
