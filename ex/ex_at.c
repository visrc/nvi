/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static const char sccsid[] = "$Id: ex_at.c,v 8.26 1994/08/17 09:47:03 bostic Exp $ (Berkeley) $Date: 1994/08/17 09:47:03 $";
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
ex_at(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	CB *cbp;
	EX_PRIVATE *exp;
	TEXT *tp;
	int name;

	exp = EXP(sp);

	/*
	 * !!!
	 * Historically, [@*]<carriage-return> and [@*][@*] executed the most
	 * recently executed buffer in ex mode.  In vi mode, only @@ repeated
	 * the last buffer.  We change historic practice and make @* work from
	 * vi mode as well, it's simpler and more consistent.
	 */
	name = F_ISSET(cmdp, E_BUFFER) ? cmdp->buffer : '@';
	if (name == '@' || name == '*') {
		if (!exp->at_lbuf_set) {
			msgq(sp, M_ERR, "No previous buffer to execute");
			return (1);
		}
		name = exp->at_lbuf;
	}

	CBNAME(sp, cbp, name);
	if (cbp == NULL) {
		msgq(sp, M_ERR, "Buffer %s is empty", KEY_NAME(sp, name));
		return (1);
	}

	/* Save for reuse. */
	exp->at_lbuf = name;
	exp->at_lbuf_set = 1;

	/*
	 * !!!
	 * Historic practice is that if the buffer was cut in line mode,
	 * <newlines> were appended to each line as it was pushed onto
	 * the stack.  If the buffer was cut in character mode, <newlines>
	 * were appended to all lines but the last one.
	 */
	for (tp = cbp->textq.cqh_last;
	    tp != (void *)&cbp->textq; tp = tp->q.cqe_prev)
		if ((F_ISSET(cbp, CB_LMODE) ||
		    tp->q.cqe_next != (void *)&cbp->textq) &&
		    term_push(sp, "\n", 1, 0) ||
		    term_push(sp, tp->lb, tp->len, 0))
			return (1);
	return (0);
}
