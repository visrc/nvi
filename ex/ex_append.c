/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_append.c,v 5.24 1993/02/28 12:19:08 bostic Exp $ (Berkeley) $Date: 1993/02/28 12:19:08 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "screen.h"
#include "term.h"

enum which {APPEND, CHANGE};

static int ac __P((EXF *, EXCMDARG *, enum which));

/*
 * ex_append -- :address append[!]
 *	Append one or more lines of new text after the specified line,
 *	or the current line if no address is specified.
 */
int
ex_append(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (ac(ep, cmdp, APPEND));
}

/*
 * ex_change -- :range change[!] [count]
 *	Change one or more lines to the input text.
 */
int
ex_change(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (ac(ep, cmdp, CHANGE));
}

static int
ac(ep, cmdp, cmd)
	EXF *ep;
	EXCMDARG *cmdp;
	enum which cmd;
{
	MARK m;
	recno_t cnt;
	size_t len;
	int rval, set;
	u_char *p;

	/* The ! flag turns off autoindent for change and append. */
	if (cmdp->flags & E_FORCE) {
		set = ISSET(O_AUTOINDENT);
		UNSET(O_AUTOINDENT);
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
			if (ex_gb(ep, 0, &p, &len,
			    GB_BEAUTIFY | GB_MAPINPUT | GB_NLECHO)) {
				rval = 1;
				goto done;
			}
			if (len == 1 && p[0] == '.') {
				cnt = cmdp->addr2.lno - m.lno;
				while (cnt--)
					if (file_dline(ep, m.lno)) {
						rval = 1;
						goto done;
					}
				goto done;
			}
			if (file_sline(ep, m.lno, p, len)) {
				rval = 1;
				goto done;
			}
			++m.lno;
		}

	if (cmd == APPEND)
		for (;;) {
			if (ex_gb(ep, 0, &p, &len,
			    GB_BEAUTIFY | GB_MAPINPUT | GB_NLECHO)) {
				rval = 1;
				goto done;
			}

			if (len == 1 && p[0] == '.')
				break;
			if (file_aline(ep, m.lno, p, len)) {
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
			FF_SET(ep, F_AUTOPRINT);

		/* Set the cursor. */
		SCRLNO(ep) = m.lno;
		SCRCNO(ep) = 0;
	}

	if (set)
		SET(O_AUTOINDENT);

	return (rval);
}
