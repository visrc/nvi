/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mkexrc.c,v 5.3 1992/04/03 09:16:44 bostic Exp $ (Berkeley) $Date: 1992/04/03 09:16:44 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include "vi.h"
#include "excmd.h"
#include "pathnames.h"
#include "extern.h"

/*
 * ex_mkexrc -- (:mkexrc[!] [file])
 *	Create (or overwrite) a .exrc file with the current info.
 */
void
ex_mkexrc(cmdp)
	CMDARG *cmdp;
{
	int fd;
	char *name;

	switch (cmdp->argc) {
	case 0:
		name = _NAME_EXRC;
		break;
	case 1:
		name = cmdp->argv[0];
		break;
	}

	/* Create with max permissions of rw-r--r--. */
	if ((fd = open(name,
	    (cmdp->flags & E_FORCE ? 0 : O_EXCL)|O_CREAT|O_TRUNC|O_WRONLY,
	    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
		msg("%s: %s", name, strerror(errno));
		return;
	}

	/* In case it already existed, set the permissions. */
	(void)fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

	map_save(fd);
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

	(void)close(fd);
	msg("New .exrc file: %s. ", name);
}
