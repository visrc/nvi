/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_z.c,v 8.1 1993/09/09 14:00:29 bostic Exp $ (Berkeley) $Date: 1993/09/09 14:00:29 $";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_z -- :[line] z [^-.+=] [count] [flags]
 *
 *	Adjust window.
 */
int
ex_z(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	recno_t cnt, equals, lno;
	size_t len;
	int eofcheck;
	char buf[256];

	/*
	 * !!!
	 * If no count specified, use either two times the size of the
	 * scrolling region, or the size of the window option.  POSIX
	 * 1003.2 claims that the latter is correct, but historic ex/vi
	 * documentation and practice appear to use the scrolling region.
	 * I'm using the window size as it means that the entire screen
	 * is used instead of losing a line to roundoff.  Note, we drop
	 * a line from the cnt if using the window size to leave room for
	 * the next prompt.
	 */
	if (F_ISSET(cmdp, E_COUNT))
		cnt = cmdp->count;
	else
#ifdef HISTORIC_PRACTICE
		cnt = O_VAL(sp, O_SCROLL) * 2;
#else
		cnt = O_VAL(sp, O_WINDOW) - 1;
#endif

	equals = 0;
	eofcheck = 0;
	lno = cmdp->addr1.lno;

	switch (F_ISSET(cmdp,
	    E_F_CARAT | E_F_DASH | E_F_DOT | E_F_EQUAL | E_F_PLUS)) {
	case E_F_CARAT:		/* Display cnt * 2 before the line. */
		eofcheck = 1;
		if (lno > cnt * 2)
			cmdp->addr1.lno = (lno - cnt * 2) + 1;
		else
			cmdp->addr1.lno = 1;
		cmdp->addr2.lno = (cmdp->addr1.lno + cnt) - 1;
		break;
	case E_F_DASH:		/* Line goes at the bottom of the screen. */
		cmdp->addr1.lno = lno > cnt ? (lno - cnt) + 1 : 1;
		cmdp->addr2.lno = lno;
		break;
	case E_F_DOT:		/* Line goes in the middle of the screen. */
		/*
		 * !!!
		 * Historically, the "middleness" of the line overrode the
		 * count, so that "3z.19" or "3z.20" would display the first
		 * 12 lines of the file, i.e. (N - 1) / 2 lines before and
		 * after the specified line.
		 */
		eofcheck = 1;
		cnt = (cnt - 1) / 2;
		cmdp->addr1.lno = lno > cnt ? lno - cnt : 1;
		cmdp->addr2.lno = lno + cnt;
		break;
	case E_F_EQUAL:		/* Center with hyphens. */
		/*
		 * !!!
		 * Strangeness.  The '=' flag is like the '.' flag (see the
		 * above comment, it applies here as well) but with a special
		 * little hack.  Print out lines of hyphens before and after
		 * the specified line.  Additionally, the cursor remains set
		 * on that line.
		 */
		eofcheck = 1;
		cnt = (cnt - 1) / 2;
		cmdp->addr1.lno = lno > cnt ? lno - cnt : 1;
		cmdp->addr2.lno = lno - 1;
		if (ex_pr(sp, ep, cmdp))
			return (1);
		(void)fprintf(sp->stdfp,
		    "%s", "----------------------------------------\n");
		cmdp->addr2.lno = cmdp->addr1.lno = equals = lno;
		if (ex_pr(sp, ep, cmdp))
			return (1);
		(void)fprintf(sp->stdfp,
		    "%s", "----------------------------------------\n");
		cmdp->addr1.lno = lno + 1;
		cmdp->addr2.lno = (lno + cnt) - 1;
		break;
	default:
		/* If no line specified, move to the next one. */
		if (F_ISSET(cmdp, E_ADDRDEF))
			++lno;
		/* FALLTHROUGH */
	case E_F_PLUS:		/* Line goes at the top of the screen. */
		eofcheck = 1;
		cmdp->addr1.lno = lno;
		cmdp->addr2.lno = (lno + cnt) - 1;
		break;
	}

	if (eofcheck) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (cmdp->addr2.lno > lno)
			cmdp->addr2.lno = lno;
	}

	if (ex_pr(sp, ep, cmdp))
		return (1);
	if (equals)
		sp->lno = equals;
	return (0);
}
