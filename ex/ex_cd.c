/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_cd.c,v 8.5 1994/03/18 11:01:25 bostic Exp $ (Berkeley) $Date: 1994/03/18 11:01:25 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
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

#include "vi.h"
#include "excmd.h"

/*
 * ex_cd -- :cd[!] [directory]
 *	Change directories.
 */
int
ex_cd(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	CDPATH *cdp;
	EX_PRIVATE *exp;
	char *dir, buf[MAXPATHLEN];

	switch (cmdp->argc) {
	case 0:
		if ((dir = getenv("HOME")) == NULL) {
			msgq(sp, M_ERR,
			    "Environment variable HOME not set.");
			return (1);
		}
		break;
	case 1:
		dir = cmdp->argv[0]->bp;
		break;
	default:
		abort();
	}

	if (chdir(dir) < 0) {
		exp = EXP(sp);
		for (cdp = exp->cdq.tqh_first;
		    cdp != NULL; cdp = cdp->q.tqe_next) {
			(void)snprintf(buf,
			    sizeof(buf), "%s/%s", cdp->path, dir);
			if (!chdir(buf))
				break;
		}
		if (cdp == NULL) {
			msgq(sp, M_SYSERR, dir);
			return (1);
		}
	}
	if (getcwd(buf, sizeof(buf)) != NULL)
		msgq(sp, M_INFO, "New directory: %s", buf);
	return (0);
}

#define	FREE_CDPATH(cdp) {						\
	TAILQ_REMOVE(&exp->cdq, (cdp), q);				\
	free((cdp)->path);						\
	FREE((cdp), sizeof(CDPATH));					\
}
/*
 * ex_cdalloc --
 *	Create a new list of cd paths.
 */
int
ex_cdalloc(sp, str)
	SCR *sp;
	char *str;
{
	EX_PRIVATE *exp;
	CDPATH *cdp;
	size_t len;
	char *p, *t;

	/* Free current queue. */
	exp = EXP(sp);
	while ((cdp = exp->cdq.tqh_first) != NULL)
		FREE_CDPATH(cdp);

	/*
	 * Create new queue.
	 *
	 * The environmental variable is delimited by colon characters.
	 * Permit blanks as well, since user's are like to use them as
	 * delimiters when manually entering a new value.
	 */
	for (p = t = str;; ++p) {
		if (*p == '\0' || *p == ':' || isblank(*p)) {
			if ((len = p - t) > 1) {
				MALLOC_RET(sp, cdp, CDPATH *, sizeof(CDPATH));
				MALLOC(sp, cdp->path, char *, len + 1);
				if (cdp->path == NULL) {
					free(cdp);
					return (1);
				}
				memmove(cdp->path, t, len);
				cdp->path[len] = '\0';
				TAILQ_INSERT_TAIL(&exp->cdq, cdp, q);
			}
			t = p + 1;
		}
		if (*p == '\0')
			 break;
	}
	return (0);
}
						/* Free previous queue. */
/*
 * ex_cdfree --
 *	Free the cd path list.
 */
int
ex_cdfree(sp)
	SCR *sp;
{
	EX_PRIVATE *exp;
	CDPATH *cdp;

	/* Free up cd path information. */
	exp = EXP(sp);
	while ((cdp = exp->cdq.tqh_first) != NULL)
		FREE_CDPATH(cdp);
	return (0);
}
