/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_write.c,v 5.24 1993/04/05 07:11:56 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:11:56 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_write --	:write[!] [>>] [file]
 *		:write [!] [cmd]
 *	Write to a file.
 */
int
ex_write(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	register char *p;
	struct stat sb;
	FILE *fp;
	MARK rm;
	int fd, flags, force;
	char *fname;

	/* If nothing, just write the file back. */
	if ((p = cmdp->string) == NULL) {
		if (F_ISSET(ep, F_NONAME)) {
			msgq(sp, M_ERROR, "No filename to which to write.");
			return (1);
		}
		if (F_ISSET(ep, F_NAMECHANGED)) {
			fname = ep->name;
			flags = O_TRUNC;
			force = 0;
			goto noargs;
		} else
			return (file_sync(sp, ep, 0));
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
			msgq(sp, M_ERROR, "Usage: %s.", cmdp->cmd->usage);
			return (1);
		}
		if (filtercmd(sp, ep,
		    &cmdp->addr1, &cmdp->addr2, &rm, ++p, NOOUTPUT))
			return (1);
		sp->lno = rm.lno;
		return (0);
	}

	/* If "write >>" it's an append to a file. */
	if (p[0] == '>' && p[1] == '>') {
		flags = O_APPEND;

		/* Skip ">>" and whitespace. */
		for (p += 2; *p && isspace(*p); ++p);
	} else
		flags = O_TRUNC;

	/* Build an argv. */
	if (buildargv(sp, ep, p, 1, &cmdp->argc, &cmdp->argv))
		return (1);

	switch(cmdp->argc) {
	case 0:
		fname = ep->name;
		break;
	case 1:
		fname = (char *)cmdp->argv[0];
		break;
	default:
		msgq(sp, M_ERROR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	/* If the file exists, must either be a ! or a >> flag. */
noargs:	if (!force && flags != O_APPEND && !stat(fname, &sb)) {
		msgq(sp, M_ERROR,
		    "%s already exists -- use ! to overwrite it.", fname);
		return (1);
	}

	if ((fd = open(fname, flags | O_CREAT | O_WRONLY, DEFFILEMODE)) < 0) {
		msgq(sp, M_ERROR, "%s: %s", fname, strerror(errno));
		return (1);
	}
	if ((fp = fdopen(fd, "w")) == NULL) {
		(void)close(fd);
		msgq(sp, M_ERROR, "%s: %s", fname, strerror(errno));
		return (1);
	}
	if (ex_writefp(sp, ep, fname, fp, &cmdp->addr1, &cmdp->addr2, 1))
		return (1);
	/* If wrote the entire file, turn off modify bit. */
	if (cmdp->flags & E_ADDR2_ALL)
		F_CLR(ep, F_MODIFIED);
	return (0);
}

/*
 * ex_writefp --
 *	Write a range of lines to a FILE *.
 */
int
ex_writefp(sp, ep, fname, fp, fm, tm, success_msg)
	SCR *sp;
	EXF *ep;
	char *fname;
	FILE *fp;
	MARK *fm, *tm;
	int success_msg;
{
	register u_long ccnt, fline, tline;
	size_t len;
	char *p;

	fline = fm->lno;
	tline = tm->lno;

	ccnt = 0;
	/*
	 * Historic vi permitted files of 0 length to be written.  However,
	 * since the way vi got around dealing with "empty" files was to
	 * always have a line in the file no matter what, it wrote them as
	 * files of a single, empty line.  `Alex, I'll take vi trivia for
	 * $500.'
	 */
	if (tline != 0)
		for (; fline <= tline; ++fline, ccnt += len) {
			if ((p = file_gline(sp, ep, fline, &len)) == NULL)
				break;
			if (fwrite(p, 1, len, fp) != len)
				break;
			if (putc('\n', fp) != '\n')
				break;
		}
	if (fclose(fp)) {
		msgq(sp, M_ERROR, "%s: %s", fname, strerror(errno));
		return (1);
	}
	if (success_msg)
		msgq(sp, M_DISPLAY, "%s: %lu lines, %lu characters.",
		    fname, tm->lno == 0 ? 0 : tm->lno - fm->lno + 1, ccnt);
	return (0);
}
