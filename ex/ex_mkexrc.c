/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mkexrc.c,v 5.16 1993/02/28 14:00:38 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:00:38 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "seq.h"
#include "pathnames.h"

/*
 * ex_mkexrc -- (:mkexrc[!] [file])
 *	Create (or overwrite) a .exrc file with the current info.
 */
int
ex_mkexrc(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	FILE *fp;
	int fd;
	char *fname;

	switch (cmdp->argc) {
	case 0:
		fname = _PATH_EXRC;
		break;
	case 1:
		fname = (char *)cmdp->argv[0];
		break;
	default:
		abort();
	}

	/* Create with max permissions of rw-r--r--. */
	if ((fd = open(fname,
	    (cmdp->flags & E_FORCE ? 0 : O_EXCL)|O_CREAT|O_TRUNC|O_WRONLY,
	    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
		ep->msg(ep, M_ERROR, "%s: %s", fname, strerror(errno));
		return (1);
	}

	/* In case it already existed, set the permissions. */
	(void)fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

	if ((fp = fdopen(fd, "w")) == NULL) {
		ep->msg(ep, M_ERROR, "%s: %s", fname, strerror(errno));
		return (1);
	}

	if (abbr_save(fp) || ferror(fp))
		goto err;
	if (map_save(fp) || ferror(fp))
		goto err;
	opts_save(fp);
	if (ferror(fp))
		goto err;
	if (fclose(fp)) {
err:		ep->msg(ep, M_ERROR,
		    "%s: incomplete: %s", fname, strerror(errno));
		return (1);
	}
#ifndef NO_DIGRAPH
	digraph_save(fd);
#endif
	ep->msg(ep, M_DISPLAY, "New .exrc file: %s. ", fname);
	return (0);
}
