/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_cd.c,v 8.12 1994/08/04 16:45:26 bostic Exp $ (Berkeley) $Date: 1994/08/04 16:45:26 $";
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
	size_t len;
	char *dir;		/* XXX End of the stack, don't trust getcwd. */
	char buf[MAXPATHLEN * 2];

	switch (cmdp->argc) {
	case 0:
		if ((dir = getenv("HOME")) == NULL) {
			msgq(sp, M_ERR, "Environment variable HOME not set");
			return (1);
		}
		len = strlen(dir);
		break;
	case 1:
		dir = cmdp->argv[0]->bp;
		len = cmdp->argv[0]->len;
		break;
	default:
		abort();
	}

	/*
	 * If the user has a CDPATH variable, we use it, otherwise
	 * we use the current directory.
	 *
	 * !!!
	 * This violates historic practice.  Historic vi didn't consider
	 * CDPATH, and therefore always used the current directory. This
	 * is probably correct; if users have set CDPATH to not include
	 * the current directory, they probably had a reason.
	 */
	if (dir[0] == '/') {
		if (argv_exp2(sp, ep, cmdp, dir, len, 0))
			return (1);
		if (chdir(cmdp->argv[cmdp->argc - 1]->bp) < 0)
			goto err;
	} else {
		exp = EXP(sp);
		for (cdp = exp->cdq.tqh_first;
		    cdp != NULL; cdp = cdp->q.tqe_next) {
			len = snprintf(buf,
			    sizeof(buf), "%s/%s", cdp->path, dir);
			if (argv_exp2(sp, ep, cmdp, buf, len, 0))
				return (1);
			if (!chdir(cmdp->argv[cmdp->argc - 1]->bp))
				break;
		}
		if (cdp == NULL) {
err:			msgq(sp, M_SYSERR, dir);
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
	int founddot;
	char *p, *t;

	/* Free current queue. */
	exp = EXP(sp);
	while ((cdp = exp->cdq.tqh_first) != NULL)
		FREE_CDPATH(cdp);

	/*
	 * Create new queue.  The CDPATH environmental variable (and the
	 * user's manual entry) are delimited by colon characters.
	 */
	for (p = t = str, founddot = 0;; ++p) {
		if (*p == '\0' || *p == ':') {
			/*
			 * Empty strings specify ".".  The only way to get an
			 * empty string is a leading colon, colons in a row,
			 * or a trailing colon.  Or, to put it the other way,
			 * if the the length is zero, then it's either ":XXX",
			 * "XXX::XXXX" , "XXX:", or "", and the only failure
			 * mode is the last one.  Note, the string ":" gives
			 * us two entries of '.', so we only include one of
			 * them.
			 */
			if ((len = p - t) == 0) {
				if (p == str && *p == '\0')
					break;
				if (founddot) {
					if (*p == '\0')
						break;
					continue;
				}
				len = 1;
				t = ".";
				founddot = 1;
			}
			MALLOC_RET(sp, cdp, CDPATH *, sizeof(CDPATH));
			MALLOC(sp, cdp->path, char *, len + 1);
			if (cdp->path == NULL) {
				free(cdp);
				return (1);
			}
			memmove(cdp->path, t, len);
			cdp->path[len] = '\0';
			TAILQ_INSERT_TAIL(&exp->cdq, cdp, q);
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
