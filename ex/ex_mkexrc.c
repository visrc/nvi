/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mkexrc.c,v 5.1 1992/04/02 11:21:05 bostic Exp $ (Berkeley) $Date: 1992/04/02 11:21:05 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include "vi.h"
#include "excmd.h"
#include "pathnames.h"
#include "extern.h"

#ifndef NO_MKEXRC
void
ex_mkexrc(cmdp)
	CMDARG *cmdp;
{
	int	fd;
	char *extra;

	extra = cmdp->argv[0];

	/* the default name for the .exrc file EXRC */
	if (!*extra)
	{
		extra = _NAME_EXRC;
	}

	/* create the .exrc file */
	fd = creat(extra, DEFFILEMODE);
	if (fd < 0)
	{
		msg("Couldn't create a new \"%s\" file", extra);
		return;
	}

	/* save stuff */
	savekeys(fd);
	opts_save(fd);
#ifndef NO_DIGRAPH
	digraph_save(fd);
#endif
#ifndef	NO_ABBR
	abbr_save(fd);
#endif
#ifndef NO_COLOR
	color_save(fd);
#endif

	/* close the file */
	close(fd);
	msg("Created a new \"%s\" file", extra);
}
#endif
