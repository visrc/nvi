/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_append.c,v 8.13 1994/04/10 14:32:27 bostic Exp $ (Berkeley) $Date: 1994/04/10 14:32:27 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

enum which {APPEND, CHANGE, INSERT};

static int aci __P((SCR *, EXF *, EXCMDARG *, enum which));

/*
 * ex_append -- :[line] a[ppend][!]
 *	Append one or more lines of new text after the specified line,
 *	or the current line if no address is specified.
 */
int
ex_append(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (aci(sp, ep, cmdp, APPEND));
}

/*
 * ex_change -- :[line[,line]] c[hange][!] [count]
 *	Change one or more lines to the input text.
 */
int
ex_change(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (aci(sp, ep, cmdp, CHANGE));
}

/*
 * ex_insert -- :[line] i[nsert][!]
 *	Insert one or more lines of new text before the specified line,
 *	or the current line if no address is specified.
 */
int
ex_insert(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (aci(sp, ep, cmdp, INSERT));
}

static int
aci(sp, ep, cmdp, cmd)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	enum which cmd;
{
	MARK m;
	TEXTH *sv_tiqp, tiq;
	TEXT *tp;
	recno_t cnt;
	u_int flags;
	int rval, aiset;

	/*
	 * The ! flag turns off autoindent for append, change and
	 * insert.
	 */
	if (F_ISSET(cmdp, E_FORCE)) {
		aiset = O_ISSET(sp, O_AUTOINDENT);
		O_CLR(sp, O_AUTOINDENT);
	} else
		aiset = 0;

	/*
	 * If doing a change, replace lines as long as possible.
	 * Then, append more lines or delete remaining lines.
	 * Inserts are the same as appends to the previous line.
	 */
	rval = 0;
	m = cmdp->addr1;
	if (cmd == INSERT) {
		--m.lno;
		cmd = APPEND;
	}

	/* Set input flags. */
	LF_INIT(TXT_CR | TXT_NLECHO);
	if (O_ISSET(sp, O_BEAUTIFY))
		LF_SET(TXT_BEAUTIFY);

	/* Input is interruptible. */
	F_SET(sp, S_INTERRUPTIBLE);

	/*
	 * If we're called by vi, the standard TEXTH structure (sp->tiqp) may
	 * already be in use, e.g. ":append|s/abc/ABC/" doesn't work because
	 * we're only halfway through the line when the append code fires.
	 * Use a local structure instead.
	 */
	memset(tiq, 0, sizeof(TEXTH));
	CIRCLEQ_INIT(&tiq);
	sv_tiqp = sp->tiqp;
	sp->tiqp = &tiq;

	if (cmd == CHANGE)
		for (;; ++m.lno) {
			if (m.lno > cmdp->addr2.lno) {
				cmd = APPEND;
				--m.lno;
				break;
			}
			switch (sp->s_get(sp, ep, &tiq, 0, flags)) {
			case INP_OK:
				break;
			case INP_EOF:
			case INP_ERR:
				goto err;
			}
			tp = tiq.cqh_first;
			if (tp->len == 1 && tp->lb[0] == '.') {
				for (cnt =
				    (cmdp->addr2.lno - m.lno) + 1; cnt--;)
					if (file_dline(sp, ep, m.lno))
						goto err;
				goto done;
			}
			if (file_sline(sp, ep, m.lno, tp->lb, tp->len))
				goto err;
			if (F_ISSET(sp, S_INTERRUPTED))
				goto done;
			if (sp->s_refresh(sp, ep))
				goto err;
		}

	if (cmd == APPEND)
		for (;; ++m.lno) {
			switch (sp->s_get(sp, ep, &tiq, 0, flags)) {
			case INP_OK:
				break;
			case INP_EOF:
			case INP_ERR:
				goto err;
			}
			tp = tiq.cqh_first;
			if (tp->len == 1 && tp->lb[0] == '.')
				break;
			if (file_aline(sp, ep, 1, m.lno, tp->lb, tp->len))
				goto err;
			if (F_ISSET(sp, S_INTERRUPTED))
				goto done;
			if (sp->s_refresh(sp, ep))
				goto err;
		}
	if (0)
err:		rval = 1;

done:	if (aiset)
		O_SET(sp, O_AUTOINDENT);

	/* Set the line number to the last line successfully modified. */
	sp->lno = rval ? m.lno : m.lno + 1;

	sp->tiqp = sv_tiqp;
	text_lfree(&tiq);
	return (rval);
}
