/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_append.c,v 5.28 1993/04/06 11:37:11 bostic Exp $ (Berkeley) $Date: 1993/04/06 11:37:11 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

enum which {APPEND, CHANGE};

static int ac __P((SCR *, EXF *, EXCMDARG *, enum which));

/*
 * ex_append -- :address append[!]
 *	Append one or more lines of new text after the specified line,
 *	or the current line if no address is specified.
 */
int
ex_append(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (ac(sp, ep, cmdp, APPEND));
}

/*
 * ex_change -- :range change[!] [count]
 *	Change one or more lines to the input text.
 */
int
ex_change(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (ac(sp, ep, cmdp, CHANGE));
}

static int
ac(sp, ep, cmdp, cmd)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	enum which cmd;
{
	MARK m;
	recno_t cnt;
	size_t len;
	int rval, set;
	char *p;

	/* The ! flag turns off autoindent for change and append. */
	if (cmdp->flags & E_FORCE) {
		set = O_ISSET(sp, O_AUTOINDENT);
		O_CLR(sp, O_AUTOINDENT);
	} else
		set = 0;

	rval = 0;

	/*
	 * If doing a change, replace lines as long as possible.
	 * Then, append more lines, or delete remaining lines.
	 */
	m = cmdp->addr1;
	if (m.lno == 0)
		cmd = APPEND;
	if (cmd == CHANGE)
		for (;;) {
			if (m.lno > cmdp->addr2.lno) {
				cmd = APPEND;
				--m.lno;
				break;
			}
			if (ex_gb(sp, 0, &p, &len,
			    GB_BEAUTIFY | GB_MAPINPUT | GB_NLECHO)) {
				rval = 1;
				goto done;
			}
			if (len == 1 && p[0] == '.') {
				cnt = cmdp->addr2.lno - m.lno;
				while (cnt--)
					if (file_dline(sp, ep, m.lno)) {
						rval = 1;
						goto done;
					}
				goto done;
			}
			if (file_sline(sp, ep, m.lno, p, len)) {
				rval = 1;
				goto done;
			}
			++m.lno;
		}

	if (cmd == APPEND)
		for (;;) {
			if (ex_gb(sp, 0, &p, &len,
			    GB_BEAUTIFY | GB_MAPINPUT | GB_NLECHO)) {
				rval = 1;
				goto done;
			}

			if (len == 1 && p[0] == '.')
				break;
			if (file_aline(sp, ep, m.lno, p, len)) {
				rval = 1;
				goto done;
			}
			++m.lno;
		}

done:	if (rval == 0) {
		/*
		 * XXX
		 * Not sure historical ex set autoprint for change/append.
		 * Hack for user giving us line zero and then never putting
		 * anything in the file.
		 */
		if (m.lno != 0)
			F_SET(sp, S_AUTOPRINT);

		/* Set the cursor. */
		sp->lno = m.lno;
		sp->cno = 0;
	}

	if (set)
		O_SET(sp, O_AUTOINDENT);

	return (rval);
}
