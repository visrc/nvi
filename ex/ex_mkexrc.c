/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mkexrc.c,v 8.14 1994/08/17 14:30:57 bostic Exp $ (Berkeley) $Date: 1994/08/17 14:30:57 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>
#include <pathnames.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_mkexrc -- :mkexrc[!] [file]
 *
 * Create (or overwrite) a .exrc file with the current info.
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
		fname = cmdp->argv[0]->bp;
		set_alt_name(sp, fname);
		break;
	default:
		abort();
	}

	if (!F_ISSET(cmdp, E_FORCE) && !stat(fname, &sb)) {
		msgq(sp, M_ERR,
		    "%s exists, not written; use ! to override", fname);
		return (1);
	}

	/* Create with max permissions of rw-r--r--. */
	if ((fd = open(fname, O_CREAT | O_TRUNC | O_WRONLY,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
		msgq(sp, M_SYSERR, fname);
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
