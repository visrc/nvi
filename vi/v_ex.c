/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 5.13 1992/10/24 15:25:51 bostic Exp $ (Berkeley) $Date: 1992/10/24 15:25:51 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "screen.h"
#include "term.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_ex --
 *	Execute strings of ex commands.
 */
int
v_ex(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	EXF *scurf;
	int flags, key;
	u_char *p;

	scurf = curf;
	v_startex();
	for (flags = GB_BS;; flags |= GB_NLECHO) {
		/*
		 * Get an ex command; echo the newline on any prompts after
		 * the first.  We may have to overwrite the command later;
		 * get the length for later.
		 */
		if (gb(ISSET(O_PROMPT) ? ':' : 0, &p, &ex_prerase, flags) ||
		    p == NULL)
			break;

		/*
		 * Execute the command.  If the command fails, and nothing was
		 * printed, we return to vi, confident that the error messages
		 * will be displayed.  If something has been printed, we want
		 * to group the errors together with the normal output, so we
		 * supply a terminating newline if it's needed, and then display
		 * the error messages.
		 *
		 * If successful and nothing was printed, return to vi.
		 *
		 * In either case, if something was printed, wait for the user
		 * to confirm that they saw it.
		 */
		if (ex_cstring(p, ex_prerase, 0)) {
			if (ex_prstate == PR_NONE)
				break;
			if (ex_prstate == PR_STARTED)
				EX_PRNEWLINE;
			if (msgcnt)
				msg_eflush();
			else
				(void)printf("Error...\n");
		} else if (ex_prstate < PR_PRINTED)
			break;
		(void)tputs(SO, 0, __putchar);
		(void)printf("Enter key to continue: ");
		(void)tputs(SE, 0, __putchar);
		(void)fflush(stdout);

		/* The user may continue in ex mode by entering a ':'. */
		if ((key = getkey(0)) != ':')
			break;
	}
	v_leaveex();

	/* The file may have changed. */
	if (scurf != curf) {
		scr_ref(curf);
		refresh();
	}

	/* The only cursor modifications will have been real. */
	rp->lno = curf->lno;
	rp->cno = curf->cno;

	return (0);
}
