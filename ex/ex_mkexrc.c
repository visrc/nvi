/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mkexrc.c,v 8.4 1993/08/31 16:54:03 bostic Exp $ (Berkeley) $Date: 1993/08/31 16:54:03 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "pathnames.h"

/*
 * ex_mkexrc -- :mkexrc[!] [file]
 *	Create (or overwrite) a .exrc file with the current info.
 */
int
ex_mkexrc(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	struct stat sb;
	FILE *fp;
	int fd, sverrno;
	char *fname;

	switch (cmdp->argc) {
	case 0:
		fname = _PATH_EXRC;
		break;
	case 1:
		fname = cmdp->argv[0];
		set_alt_fname(sp, fname);
		break;
	default:
		abort();
	}

	if (!F_ISSET(cmdp, E_FORCE) && !stat(fname, &sb)) {
		msgq(sp, M_ERR,
		    "%s exists, not written; use ! to override.", fname);
		return (1);
	}

	/* Create with max permissions of rw-r--r--. */
	if ((fd = open(fname, O_CREAT | O_TRUNC | O_WRONLY,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
		msgq(sp, M_ERR, "%s: %s", fname, strerror(errno));
		return (1);
	}

	if ((fp = fdopen(fd, "w")) == NULL) {
		sverrno = errno;
		(void)close(fd);
		errno = sverrno;
		goto e2;
	}

	if (abbr_save(sp, fp) || ferror(fp))
		goto e1;
	if (map_save(sp, fp) || ferror(fp))
		goto e1;
	if (opts_save(sp, fp) || ferror(fp))
		goto e1;
#ifndef NO_DIGRAPH
	digraph_save(sp, fd);
#endif
	if (fclose(fp))
		goto e2;

	msgq(sp, M_INFO, "New .exrc file: %s. ", fname);
	return (0);

e1:	sverrno = errno;
	(void)fclose(fp);
	errno = sverrno;
e2:	msgq(sp, M_ERR, "%s: incomplete: %s", fname, strerror(errno));
	return (1);
}
