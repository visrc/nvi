/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_write.c,v 5.4 1992/04/15 09:14:53 bostic Exp $ (Berkeley) $Date: 1992/04/15 09:14:53 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_write(cmdp)
	CMDARG *cmdp;
{
	struct stat sb;
	int fd, flags, rval;
	char *fname;

	switch(cmdp->argc) {
	case 0:
		fname = origname;
		break;
	case 1:
		fname = cmdp->argv[0];
		break;
	}
		
	/* If the file exists, must either be a ! or a >> flag. */
	if (!(cmdp->flags & (E_FORCE | E_F_APPEND)) && !stat(fname, &sb)) {
		msg("%s already exists -- use ! to overwrite it.", fname);
		return (1);
	}

	flags = O_CREAT | O_TRUNC | O_WRONLY;
	if (cmdp->flags & E_F_APPEND)
		flags |= O_APPEND;

	if ((fd = open(fname, flags, DEFFILEMODE)) < 0) {
		msg("%s: %s", fname, strerror(errno));
		return (1);
	}

	rval = ex_writerange(fname, fd, cmdp->addr1, cmdp->addr2, 1);
	return (close(fd) || rval);
}

ex_writerange(fname, fd, from, to, success_msg)
	char *fname;
	int fd, success_msg;
	MARK from, to;
{
	register u_long ccnt, fline, lcnt, tline;
	register char *p;
	size_t len;

	fline = markline(from);
	tline = markline(to);
	lcnt = tline - fline;
	for (ccnt = 0; fline <= tline; ++fline, ccnt += len) {
		p = fetchline(fline, &len);
		p[len++] = '\n';
		if (write(fd, p, len) != len) {
			msg("%s: %s", fname, strerror(errno));
			return (1);
		}
	}
	if (success_msg)
		msg("%s: %lu lines, %lu characters.", fname, lcnt, ccnt);
	return (0);
}
