/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 10.1 1995/04/13 17:22:01 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:22:01 $";
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

#include "common.h"

/*
 * ex_at -- :@[@ | buffer]
 *	    :*[* | buffer]
 *
 *	Execute the contents of the buffer.
 */
int
ex_at(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	CB *cbp;
	CHAR_T name;
	GAT *gatp;
	RANGE *rp;
	TEXT *tp;
	size_t len;
	char *p;

	/*
	 * !!!
	 * Historically, [@*]<carriage-return> and [@*][@*] executed the most
	 * recently executed buffer in ex mode.
	 */
	name = FL_ISSET(cmdp->iflags, E_C_BUFFER) ? cmdp->buffer : '@';
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
	 * Get a GAT structure.  We use the same structure for both global
	 * commands and @ buffer commands because they have to hang on the
	 * same stack, and they have lots of similarities.
	 */
	CALLOC_RET(sp, gatp, GAT *, 1, sizeof(GAT));

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
	 *
	 * All of this is implemented in the ex_cmd() parse loop, BECAUSE EVENT
	 * DRIVEN PROGRAMMING JUST SUCKS!! (Uh, sorry.  But this code used to
	 * be so very, very much cleaner before I made it work with an X event
	 * loop.  Just as an example, the vi parser was three, mildly complex
	 * 200 line functions.  I had to trade them in for a roughly 1000 line
	 * switch statement, with 20 goto's.  I won't even talk about what's in
	 * the ex parser, now.  I'm NOT pleased.)
	 */
	CIRCLEQ_INIT(&gatp->rangeq);
	CALLOC_RET(sp, rp, RANGE *, 1, sizeof(RANGE));
	rp->start = cmdp->addr1.lno;
	rp->stop = cmdp->addr2.lno;
	CIRCLEQ_INSERT_HEAD(&gatp->rangeq, rp, q);

	/*
	 * Save any remaining portion of the current command for restoration
	 * when this command is finished.
	 */
	if (cmdp->save_cmdlen != 0) {
		MALLOC_RET(sp, gatp->save_cmd, char *, cmdp->save_cmdlen);
		memmove(gatp->save_cmd, cmdp->save_cmd, cmdp->save_cmdlen);
		gatp->save_cmdlen = cmdp->save_cmdlen;
		cmdp->save_cmdlen = 0;
	}

	/*
	 * Buffers executed in ex mode or from the colon command line in vi
	 * were ex commands.  We can't push it on the terminal queue, since
	 * it has to be executed immediately, and we may be in the middle of
	 * an ex command already.  Push the command on the ex command stack.
	 * Build two copies of the command.  We need two copies because the
	 * ex parser may step on the command string when it's parsing it.
	 */
	for (len = 0, tp = cbp->textq.cqh_last;
	    tp != (void *)&cbp->textq; tp = tp->q.cqe_prev)
		len += tp->len + 1;
	MALLOC_RET(sp, gatp->cmd, char *, len * 2);
	for (p = gatp->cmd, tp = cbp->textq.cqh_last;
	    tp != (void *)&cbp->textq; tp = tp->q.cqe_prev) {
		memmove(p, tp->lb, tp->len);
		p += tp->len;
		*p++ = '\n';
	}
	gatp->cmd_len = len;

	LIST_INSERT_HEAD(&EXP(sp)->gatq, gatp, q);
	return (0);
}
