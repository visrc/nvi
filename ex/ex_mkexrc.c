/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mkexrc.c,v 5.6 1992/04/05 15:48:19 bostic Exp $ (Berkeley) $Date: 1992/04/05 15:48:19 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "pathnames.h"
#include "extern.h"

/*
 * ex_mkexrc -- (:mkexrc[!] [file])
 *	Create (or overwrite) a .exrc file with the current info.
 */
int
ex_mkexrc(cmdp)
	CMDARG *cmdp;
{
	FILE *fp;
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
		return (1);
	}

	/* In case it already existed, set the permissions. */
	(void)fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

	if (abbr_save(fp) || ferror(fp))
		goto err;
	if (map_save(fp) || ferror(fp))
		goto err;
	if (opts_save(fp) || ferror(fp))
		goto err;
	fflush(fp);			/* XXX all should use fp. */
	if (ferror(fp)) {
err:		msg("%s: incomplete: %s", name, strerror(errno));
		return (1);
	}
#ifndef NO_DIGRAPH
	digraph_save(fd);
#endif
#ifndef NO_COLOR
	color_save(fd);
#endif

	(void)close(fd);
	msg("New .exrc file: %s. ", name);
	return (0);
}
