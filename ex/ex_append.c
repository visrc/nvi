/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_append.c,v 9.14 1995/02/09 16:19:53 bostic Exp $ (Berkeley) $Date: 1995/02/09 16:19:53 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "../sex/sex_screen.h"

enum which {APPEND, CHANGE, INSERT};

static int aci __P((SCR *, EXCMDARG *, enum which));

/*
 * ex_append -- :[line] a[ppend][!]
 *	Append one or more lines of new text after the specified line,
 *	or the current line if no address is specified.
 */
int
ex_append(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	return (aci(sp, cmdp, APPEND));
}

/*
 * ex_change -- :[line[,line]] c[hange][!] [count]
 *	Change one or more lines to the input text.
 */
int
ex_change(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	return (aci(sp, cmdp, CHANGE));
}

/*
 * ex_insert -- :[line] i[nsert][!]
 *	Insert one or more lines of new text before the specified line,
 *	or the current line if no address is specified.
 */
int
ex_insert(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	return (aci(sp, cmdp, INSERT));
}

static int
aci(sp, cmdp, cmd)
	SCR *sp;
	EXCMDARG *cmdp;
	enum which cmd;
{
	MARK m;
	TEXTH *sv_tiqp, tiq;
	TEXT *tp;
	struct termios ts;
	size_t len;
	u_int flags;
	int rval;
	char *p, *t;

	NEEDFILE(sp, cmdp->cmd);

	/*
	 * If doing a change, replace lines for as long as possible.  Then,
	 * append more lines or delete remaining lines.  Changes to an empty
	 * file are appends, inserts are the same as appends to the previous
	 * line.
	 *
	 * !!!
	 * Set the address to which we'll append.  We set sp->lno to this
	 * address as well so that autoindent works correctly when get text
	 * from the user.  Eventually set sp->lno to the final m.lno value
	 * (correcting for a possible 0 value) as that's historically correct
	 * for the final line value, whether or not the user entered any text.
	 */
	m = cmdp->addr1;
	sp->lno = m.lno;
	if ((cmd == CHANGE || cmd == INSERT) && m.lno != 0)
		--m.lno;

	/*
	 * !!!
	 * If the file isn't empty, cut changes into the unnamed buffer.
	 */
	if (cmd == CHANGE && cmdp->addr1.lno != 0 &&
	    (cut(sp, NULL, &cmdp->addr1, &cmdp->addr2, CUT_LINEMODE) ||
	    delete(sp, &cmdp->addr1, &cmdp->addr2, 1)))
		return (1);

	/*
	 * !!!
	 * Input lines specified on the ex command line (see the comment in
	 * ex.c:ex_cmd()) lines are separated by <newline>s.  If there is a
	 * trailing delimiter an empty line is inserted.  There may also be
	 * a leading delimiter, which is ignored unless it's also a trailing
	 * delimiter.  It is possible to encounter a termination line, i.e.
	 * a single '.', in a global command, but not necessary if the text
	 * insert command was the last of the global commands.
	 */
	if (cmdp->aci_len != 0) {
		for (p = cmdp->aci_text, len = cmdp->aci_len; len > 0; p = t) {
			for (t = p; len > 0 && t[0] != '\n'; ++t, --len);
			if (t != p || len == 0) {
				if (F_ISSET(sp, S_GLOBAL) &&
				    t - p == 1 && p[0] == '.') {
					++t;
					--len;
					break;
				}
				if (file_aline(sp, 1, m.lno++, p, t - p))
					return (1);
			}
			if (len != 0) {
				++t;
				if (--len == 0 &&
				    file_aline(sp, 1, m.lno++, "", 0))
					return (1);
			}
		}
		/*
		 * If there's any remaining text, we're in a global, and
		 * there's more command to parse.
		 */
		if (len != 0) {
			cmdp->aci_text = t;
			cmdp->aci_len = len;
		} else
			cmdp->aci_text = NULL;
	}

	if (F_ISSET(sp, S_GLOBAL)) {
		if ((sp->lno = m.lno) == 0 && file_eline(sp, 1))
			sp->lno = 1;
		return (0);
	}

	/*
	 * If not in a global command, read from the terminal.
	 *
	 * Set input flags; the ! flag turns off autoindent for append,
	 * change and insert.
	 */
	LF_INIT(TXT_DOTTERM | TXT_NUMBER);
	if (!F_ISSET(cmdp, E_FORCE) && O_ISSET(sp, O_AUTOINDENT))
		LF_SET(TXT_AUTOINDENT);
	if (O_ISSET(sp, O_BEAUTIFY))
		LF_SET(TXT_BEAUTIFY);

	/* The screen is now dirty. */
	F_SET(sp, S_SCR_EXWROTE);

	/*
	 * If this code is called by vi, the screen TEXTH structure (sp->tiqp)
	 * may already be in use, e.g. ":append|s/abc/ABC/" would fail as we
	 * are only halfway through the line when the append code fires.  Use
	 * the local structure instead.  The ex code would also have to use a
	 * local structure except that we're guaranteed to finish with the
	 * remaining characters in the screen TEXTH structure when they were
	 * inserted into the file, above.
	 *
	 * If this code is called by vi, we want to reset the terminal and use
	 * ex's line get routine.  It actually works fine if we use vi's get
	 * routine, but it doesn't look as nice.  Maybe if we had a separate
	 * window or something, but getting a line at a time looks awkward.
	 */
	if (F_ISSET(sp, S_VI)) {
		memset(&tiq, 0, sizeof(TEXTH));
		CIRCLEQ_INIT(&tiq);
		sv_tiqp = sp->tiqp;
		sp->tiqp = &tiq;

		if (sex_tsetup(sp, &ts))
			return (1);
		(void)write(STDOUT_FILENO, "\n", 1);
	}

	if (sex_get(sp, sp->tiqp, 0, flags) != INP_OK)
		goto err;

	for (tp = sp->tiqp->cqh_first;
	    tp != (TEXT *)sp->tiqp; tp = tp->q.cqe_next)
		if (file_aline(sp, 1, m.lno++, tp->lb, tp->len))
			goto err;
	if ((sp->lno = m.lno) == 0 && file_eline(sp, 1))
		sp->lno = 1;

	rval = 0;
	if (0) {
err:		rval = 1;
	}

	if (F_ISSET(sp, S_VI)) {
		sp->tiqp = sv_tiqp;
		text_lfree(&tiq);

		if (sex_tteardown(sp, &ts))
			rval = 1;
		F_SET(sp, S_SCR_REFRESH);
	}
	return (rval);
}
