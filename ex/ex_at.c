/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 9.3 1995/02/17 11:43:17 bostic Exp $ (Berkeley) $Date: 1995/02/17 11:43:17 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_at -- :@[@ | buffer]
 *	    :*[* | buffer]
 *
 *	Execute the contents of the buffer.
 */
int
ex_at(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	CB *cbp;
	CHAR_T name;
	TEXT *tp;
	recno_t elno, lno;
	int rval;
	char *bp, *p;
	size_t len;

	/*
	 * !!!
	 * Historically, [@*]<carriage-return> and [@*][@*] executed the most
	 * recently executed buffer in ex mode.
	 */
	name = F_ISSET(cmdp, E_BUFFER) ? cmdp->buffer : '@';
	if (name == '@' || name == '*') {
		if (!F_ISSET(sp, S_AT_SET)) {
			ex_message(sp, NULL, EXM_NOPREVBUF);
			return (1);
		}
		name = sp->at_lbuf;
	}

	CBNAME(sp, cbp, name);
	if (cbp == NULL) {
		ex_message(sp, KEY_NAME(sp, name), EXM_EMPTYBUF);
		return (1);
	}

	/* Save for reuse. */
	sp->at_lbuf = name;

	/*
	 * Buffers executed in ex mode or from the colon command line in vi
	 * were ex commands.  We can't push it on the terminal queue, since
	 * we may be executing a command from the vi colon command line.
	 * Build two copies of the command.  We need two copies because the
	 * ex parser may step on the command string when it's parsing it.
	 */
	for (len = 0, tp = cbp->textq.cqh_last;
	    tp != (void *)&cbp->textq; tp = tp->q.cqe_prev)
		len += tp->len + 1;
	MALLOC_RET(sp, bp, char *, len * 2);
	for (p = bp, tp = cbp->textq.cqh_last;
	    tp != (void *)&cbp->textq; tp = tp->q.cqe_prev) {
		memmove(p, tp->lb, tp->len);
		p += tp->len;
		*p++ = '\n';
	}

	/*
	 * !!!
	 * Historically the @ command took a range of lines, and the @ buffer
	 * was executed once per line.  The historic vi could be trashed by
	 * this, since it didn't notice if the underlying file changed, or,
	 * for that matter, if there were no more lines on which to operate.
	 * As an example, take a 10 line file, load "%delete" into a buffer,
	 * and enter :8,10@<buffer>.  We take the same approach as for global
	 * commands, i.e. if we exit or switch to a new file/screen, the rest
	 * of the command is discarded.
	 */
	rval = 0;
	for (lno = cmdp->addr1.lno,
	    elno = cmdp->addr2.lno; lno <= elno; ++lno) {
		/*
		 * Make a new copy and execute the command, setting the cursor
		 * to the line so that relative addressing works.  This means
		 * that the cursor moves to the last line sent to the command,
		 * by default, even if the command fails.
		 */
		sp->lno = lno;
		memmove(bp + len, bp, len);
		if (ex_cmd(sp, bp + len, len, 0))
			goto err;

		/* Someone's unhappy, time to stop. */
		if (INTERRUPTED(sp))
			break;

		/*
		 * If we've exited or switched to a new file/screen, the rest
		 * of the command is discarded.
		 */
		if (F_ISSET(sp,
		    S_EXIT | S_EXIT_FORCE | S_EX_CMDABORT | S_SSWITCH))
			break;
	}
	if (0)
err:		rval = 1;
	free(bp);
	return (rval);
}
