/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_write.c,v 5.9 1992/05/04 11:52:16 bostic Exp $ (Berkeley) $Date: 1992/05/04 11:52:16 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "exf.h"
#include "extern.h"

/*
 * ex_write --	:write[!] [>>] [file]
 *		:write [!] [cmd]
 *	Write to a file.
 */
int
ex_write(cmdp)
	EXCMDARG *cmdp;
{
	register char *p;
	struct stat sb;
	FILE *fp;
	int fd, flags, force;
	char *fname;

	/* If nothing, just write the file back. */
	if ((p = cmdp->string) == NULL) {
		if (curf->flags & F_NONAME) {
			msg("No filename to which to write.");
			return (1);
		}
		if (curf->flags & F_NAMECHANGED) {
			fname = curf->name;
			flags = O_TRUNC;
			goto noargs;
		} else
			return (file_sync(curf, 0));
	}

	/* If "write!" it's a force to a file. */
	if (*p == '!') {
		force = 1;
		++p;
	} else
		force = 0;

	/* Skip whitespace. */
	for (; *p && isspace(*p); ++p);

	/* If "write !" it's a pipe to a utility. */
	if (*p == '!') {
		for (; *p && isspace(*p); ++p);
		if (*p == '\0') {
			msg("Usage: %s.", cmdp->cmd->usage);
			return (1);
		}
		return (filter(cmdp->addr1, cmdp->addr2, ++p, NOOUTPUT));
	}

	/* If "write >>" it's an append to a file. */
	if (p[0] == '>' && p[1] == '>') {
		flags = O_APPEND;

		/* Skip ">>" and whitespace. */
		for (p += 2; *p && isspace(*p); ++p);
	} else
		flags = O_TRUNC;

	/* Build an argv. */
	if (buildargv(p, 1, cmdp))
		return (1);

	switch(cmdp->argc) {
	case 0:
		fname = curf->name;
		break;
	case 1:
		fname = cmdp->argv[0];
		break;
	default:
		msg("Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	/* If the file exists, must either be a ! or a >> flag. */
noargs:	if (!force && flags != O_APPEND && !stat(fname, &sb)) {
		msg("%s already exists -- use ! to overwrite it.", fname);
		return (1);
	}

	if ((fd = open(fname, flags | O_CREAT | O_WRONLY, DEFFILEMODE)) < 0) {
		msg("%s: %s", fname, strerror(errno));
		return (1);
	}
	if ((fp = fdopen(fd, "w")) == NULL) {
		(void)close(fd);
		msg("%s: %s", fname, strerror(errno));
		return (1);
	}
	return (ex_writefp(fname, fp, cmdp->addr1, cmdp->addr2, 1));
}

/*
 * ex_writefp --
 *	Write a range of lines to a FILE *.
 */
int
ex_writefp(fname, fp, from, to, success_msg)
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
	lcnt = tline - fline + 1;
	for (ccnt = 0; fline <= tline; ++fline, ccnt += len) {
		p = fetchline(fline, &len);
		if (fwrite(p, 1, len, fp) != len ||
		    putc('\n', fp) != '\n')
			goto err;
	}
	if (fclose(fp)) {
err:		msg("%s: %s", fname, strerror(errno));
		return (1);
	}
	if (success_msg)
		msg("%s: %lu lines, %lu characters.", fname, lcnt, ccnt);
	return (0);
}
