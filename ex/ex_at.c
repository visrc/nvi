/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 8.16 1994/01/09 14:20:53 bostic Exp $ (Berkeley) $Date: 1994/01/09 14:20:53 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_at -- :@[@ | buffer]
 *	    :*[* | buffer]
 *
 *	Execute the contents of the buffer.
 */
int
ex_at(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	CB *cbp;
	EX_PRIVATE *exp;
	TEXT *tp;
	int name, lmode;

	exp = EXP(sp);

	/* Historically, @@ and ** execute the last buffer. */
	name = cmdp->buffer;
	if (name == cmdp->cmd->name[0]) {
		if (!exp->at_lbuf_set) {
			msgq(sp, M_ERR, "No previous buffer to execute.");
			return (1);
		}
		name = exp->at_lbuf;
	}

	CBNAME(sp, cbp, name);
	if (cbp == NULL) {
		msgq(sp, M_ERR, "Buffer %s is empty.", charname(sp, name));
		return (1);
	}

	/* Save for reuse. */
	exp->at_lbuf = name;
	exp->at_lbuf_set = 1;
		
	/*
	 * If the buffer was cut in line mode or had portions of more
	 * than one line, <newlines> are appended to each line as it
	 * is pushed onto the stack.
	 */
	tp = cbp->textq.cqh_last;
	lmode = F_ISSET(cbp, CB_LMODE) || tp->q.cqe_prev != (void *)&cbp->textq;
	for (; tp != (void *)&cbp->textq; tp = tp->q.cqe_prev)
		if ((lmode || tp->q.cqe_prev != (void *)&cbp->textq) &&
		    term_push(sp, "\n", 1, 0, 0) ||
		    term_push(sp, tp->lb, tp->len, 0, CH_QUOTED))
			return (1);
	return (0);
}
