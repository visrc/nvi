/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mkexrc.c,v 10.5 1995/09/21 10:57:45 bostic Exp $ (Berkeley) $Date: 1995/09/21 10:57:45 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>
#include <pathnames.h>

#include "common.h"

/*
 * ex_mkexrc -- :mkexrc[!] [file]
 *
 * Create (or overwrite) a .exrc file with the current info.
 *
 * PUBLIC: int ex_mkexrc __P((SCR *, EXCMD *));
 */
int
ex_mkexrc(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	struct stat sb;
	FILE *fp;
	int fd, nf, sverrno;
	char *fname, *p;

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

	if (!FL_ISSET(cmdp->iflags, E_C_FORCE) && !stat(fname, &sb)) {
		p = msg_print(sp, fname, &nf);
		msgq(sp, M_ERR,
		    "137|%s exists, not written; use ! to override", p);
		if (nf)
			FREE_SPACE(sp, p, 0);
		return (1);
	}

	/* Create with max permissions of rw-r--r--. */
	if ((fd = open(fname, O_CREAT | O_TRUNC | O_WRONLY,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
		p = msg_print(sp, fname, &nf);
		msgq(sp, M_SYSERR, "%s", p);
		if (nf)
			FREE_SPACE(sp, p, 0);
		return (1);
	}

	if ((fp = fdopen(fd, "w")) == NULL) {
		sverrno = errno;
		(void)close(fd);
		goto e2;
	}

	if (seq_save(sp, fp, "abbreviate ", SEQ_ABBREV) || ferror(fp))
		goto e1;
	if (seq_save(sp, fp, "map ", SEQ_COMMAND) || ferror(fp))
		goto e1;
	if (seq_save(sp, fp, "map! ", SEQ_INPUT) || ferror(fp))
		goto e1;
	if (opts_save(sp, fp) || ferror(fp))
		goto e1;
	if (fclose(fp)) {
		sverrno = errno;
		goto e2;
	}

	p = msg_print(sp, fname, &nf);
	msgq(sp, M_INFO, "138|New exrc file: %s", p);
	if (nf)
		FREE_SPACE(sp, p, 0);
	return (0);

e1:	sverrno = errno;
	(void)fclose(fp);
e2:	p = msg_print(sp, fname, &nf);
	errno = sverrno;
	msgq(sp, M_SYSERR, "%s", p);
	if (nf)
		FREE_SPACE(sp, p, 0);
	return (1);
}
