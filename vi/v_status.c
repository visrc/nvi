/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 8.4 1993/09/09 19:25:30 bostic Exp $ (Berkeley) $Date: 1993/09/09 19:25:30 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <unistd.h>

#include "vi.h"
#include "vcmd.h"

static int	isro __P((FREF *));

/*
 * v_status -- ^G
 *	Show the file status.
 */
int
v_status(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{

	/*
	 * ^G in historic vi reset the cursor column to the first
	 * non-blank character in the line.  This doesn't seem of
	 * any usefulness whatsoever, so I don't bother.
	 */
	return (status(sp, ep, fm->lno, 1));
}

int
status(sp, ep, lno, showlast)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	int showlast;
{
	recno_t last;
	char *ro, *pid;
#ifdef DEBUG
	char pbuf[50];

	(void)snprintf(pbuf, sizeof(pbuf), " (pid %u)", getpid());
	pid = pbuf;
#else
	pid = "";
#endif
	ro = isro(sp->frp) ? ", readonly" : "";
	if (showlast) {
		if (file_lline(sp, ep, &last))
			return (1);
		if (last >= 1)
			msgq(sp, M_INFO, "%s: %s%s: line %lu of %lu [%ld%%]%s",
			    sp->frp->fname,
			    F_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified",
			    ro, lno, last, (lno * 100) / last, pid);
		else
			msgq(sp, M_INFO,
			    "%s: %s%s: empty file%s", sp->frp->fname,
			    F_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified",
			    ro, pid);
	} else
		msgq(sp, M_INFO,
		    "%s: %s%s: line %lu%s", sp->frp->fname,
		    F_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified",
		    ro, lno, pid);
	return (0);
}

/*
 * isro --
 *	Try and figure out if a file is readonly.  This is fundamentally the
 *	wrong thing to do.  The kernel is the only arbiter of whether or not
 *	a file is writeable.  The best that you can do here is a guess -- an
 *	obvious loophole is a file that has write permissions for the user,
 *	but is in a file system that is mounted readonly.  Another example is
 *	that this code only looks at the stat(2) permissions -- if the file
 *	has ACL's or some other additional permission mechanism, we aren't
 *	going to get it right.  However, users complained, so we're doing it.
 */
static int
isro(fp)
	FREF *fp;
{
	struct stat sb;
	int cnt, ngroups, groups[NGROUPS];

	/* If marked readonly, we're done. */
	if (F_ISSET(fp, FR_RDONLY))
		return (1);

	/* If no name, it's not readonly. */
	if (F_ISSET(fp, FR_NONAME))
		return (0);

	/* If can't stat it, it's not readonly. */
	if (stat(fp->fname, &sb))
		return (0);

	/* Owner permissions. */
	if (sb.st_uid == getuid())
		return (!(sb.st_mode & S_IWUSR));

	/* Group permissions. */
	if (sb.st_gid == getgid())
		return (!(sb.st_mode & S_IWGRP));

	for (cnt = 0,
	    ngroups = getgroups(NGROUPS, groups); cnt < ngroups; ++cnt)
		if (sb.st_gid == groups[cnt])
			return (!(sb.st_mode & S_IWGRP));

	return (!(sb.st_mode & S_IWOTH));
}
