/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_write.c,v 5.5 1992/04/16 09:50:10 bostic Exp $ (Berkeley) $Date: 1992/04/16 09:50:10 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_write(cmdp)
	CMDARG *cmdp;
{
	struct stat sb;
	FILE *fp;
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
	if ((fp = fdopen(fd, "w")) == NULL) {
		(void)close(fd);
		msg("%s: %s", fname, strerror(errno));
		return (1);
	}
	rval = ex_writerange(fname, fp, cmdp->addr1, cmdp->addr2, 1);
	return (fclose(fp) || rval);
}

ex_writerange(fname, fp, from, to, success_msg)
	char *fname;
	FILE *fp;
	MARK from, to;
	int success_msg;
{
	register u_long ccnt, fline, tline;
	u_long lcnt;
	size_t len;
	char *p;

	fline = markline(from);
	tline = markline(to);
	lcnt = tline - fline;
	for (ccnt = 0; fline <= tline; ++fline, ccnt += len) {
		p = fetchline(fline, &len);
		if (fwrite(p, 1, len, fp) != len ||
		    putc('\n', fp) != '\n') {
			msg("%s: %s", fname, strerror(errno));
			return (1);
		}
	}
	if (success_msg)
		msg("%s: %lu lines, %lu characters.", fname, lcnt, ccnt);
	return (0);
}
