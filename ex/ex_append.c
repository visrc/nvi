/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_append.c,v 10.1 1995/04/13 17:21:56 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:21:56 $";
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

#include "common.h"

enum which {APPEND, CHANGE, INSERT};

static int ex_aci __P((SCR *, EXCMD *, enum which));

/*
 * ex_append -- :[line] a[ppend][!]
 *	Append one or more lines of new text after the specified line,
 *	or the current line if no address is specified.
 */
int
ex_append(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	return (ex_aci(sp, cmdp, APPEND));
}

/*
 * ex_change -- :[line[,line]] c[hange][!] [count]
 *	Change one or more lines to the input text.
 */
int
ex_change(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	return (ex_aci(sp, cmdp, CHANGE));
}

/*
 * ex_insert -- :[line] i[nsert][!]
 *	Insert one or more lines of new text before the specified line,
 *	or the current line if no address is specified.
 */
int
ex_insert(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	return (ex_aci(sp, cmdp, INSERT));
}

static int
ex_aci(sp, cmdp, cmd)
	SCR *sp;
	EXCMD *cmdp;
	enum which cmd;
{
	CHAR_T *p, *t;
	EX_PRIVATE *exp;
	GS *gp;
	recno_t lno;
	size_t len;

	gp = sp->gp;
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
	 * from the user.
	 */
	lno = cmdp->addr1.lno;
	sp->lno = lno;
	if ((cmd == CHANGE || cmd == INSERT) && lno != 0)
		--lno;

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
	 * Anything that was left after the command separator becomes part
	 * of the inserted text.  Apparently, it was common usage to enter:
	 *
	 *	:g/pattern/append|stuff1
	 *
	 * and append the line of text "stuff1" to the lines containing the
	 * pattern.  It was also historically legal to enter:
	 *
	 *	:append|stuff1
	 *	stuff2
	 *	.
	 *
	 * and the text on the ex command line would be appended as well as
	 * the text inserted after it.  There was an historic bug however,
	 * that the user had to enter *two* terminating lines (the '.' lines)
	 * to terminate text input mode, in this case.  This whole thing
	 * could be taken too far, however.  Entering:
	 *
	 *	:append|stuff1\
	 *	stuff2
	 *	stuff3
	 *	.
	 *
	 * i.e. mixing and matching the forms confused the historic vi, and,
	 * not only did it take two terminating lines to terminate text input
	 * mode, but the trailing backslashes were retained on the input.  We
	 * match historic practice except that we discard the backslashes.
	 *
	 * Input lines specified on the ex command line lines are separated by
	 * <newline>s.  If there is a trailing delimiter an empty line was
	 * inserted.  There may also be a leading delimiter, which is ignored
	 * unless it's also a trailing delimiter.  It is possible to encounter
	 * a termination line, i.e. a single '.', in a global command, but not
	 * necessary if the text insert command was the last of the global
	 * commands.
	 */
	if (cmdp->save_cmdlen != 0) {
		for (p = cmdp->save_cmd,
		    len = cmdp->save_cmdlen; len > 0; p = t) {
			for (t = p; len > 0 && t[0] != '\n'; ++t, --len);
			if (t != p || len == 0) {
				if (F_ISSET(sp, S_EX_GLOBAL) &&
				    t - p == 1 && p[0] == '.') {
					++t;
					--len;
					break;
				}
				if (file_aline(sp, 1, lno++, p, t - p))
					return (1);
			}
			if (len != 0) {
				++t;
				if (--len == 0 &&
				    file_aline(sp, 1, lno++, "", 0))
					return (1);
			}
		}
		/*
		 * If there's any remaining text, we're in a global, and
		 * there's more command to parse.
		 *
		 * !!!
		 * We depend on the fact that non-global commands will eat the
		 * rest of the command line as text input, and before getting
		 * any text input from the user.  Otherwise, we'd have to save
		 * off the command text before or during the call to the text
		 * input function below.
		 */
		if (len != 0)
			cmdp->save_cmd = t;
		cmdp->save_cmdlen = len;
	}

	if (F_ISSET(sp, S_EX_GLOBAL)) {
		if ((sp->lno = lno) == 0 && file_eline(sp, 1))
			sp->lno = 1;
		return (0);
	}

	/*
	 * If not in a global command, read from the terminal.
	 *
	 * If this code is called by vi, we want to reset the terminal and use
	 * ex's line get routine.  It actually works fine if we use vi's get
	 * routine, but it doesn't look as nice.  Maybe if we had a separate
	 * window or something, but getting a line at a time looks awkward.
	 * However, depending on the screen that we're using, that may not
	 * be possible.
	 */
	if (F_ISSET(sp, S_VI)) {
		if (gp->scr_canon(sp, 1)) {
			msgq(sp, M_ERR,
		    "281|Cannot enter ex text input mode from current mode");
			return (1);
		}
		(void)write(STDOUT_FILENO, "\n", 1);
	}

	/*
	 * Set input flags; the ! flag turns off autoindent for append,
	 * change and insert.
	 */
	exp = EXP(sp);
	FL_INIT(exp->im_flags, TXT_DOTTERM | TXT_NUMBER);
	if (!FL_ISSET(cmdp->iflags, E_C_FORCE) && O_ISSET(sp, O_AUTOINDENT))
		FL_SET(exp->im_flags, TXT_AUTOINDENT);
	if (O_ISSET(sp, O_BEAUTIFY))
		FL_SET(exp->im_flags, TXT_BEAUTIFY);

	/*
	 * This code can't use the common screen TEXTH structure (sp->tiq),
	 * as it may already be in use, e.g. ":append|s/abc/ABC/" would fail
	 * as we are only halfway through the text when the append code fires.
	 * Use a local structure instead.  Because we have to have a local
	 * structure, the rest of the ex code uses it as well.  (The ex code
	 * would have to use a local structure except that we're guaranteed
	 * to finish any remaining characters in the common TEXTH structure
	 * when they were inserted into the file, above.)
	 */
	memset(&exp->im_tiq, 0, sizeof(TEXTH));
	CIRCLEQ_INIT(&exp->im_tiq);
	exp->im_lno = lno;

	if (ex_txt_setup(sp, 0))
		return (1);
	gp->cm_next = ES_ITEXT_TEARDOWN;
	return (0);
}

/*
 * ex_aci_td --
 *	Teardown input text mode, and resolve input lines.
 */
int
ex_aci_td(sp)
	SCR *sp;
{
	TEXT *tp;
	EX_PRIVATE *exp;
	recno_t lno;
	int rval;

	rval = 0;

	exp = EXP(sp);
	lno = exp->im_lno;
	for (tp = exp->im_tiq.cqh_first;
	    tp != (TEXT *)&exp->im_tiq; tp = tp->q.cqe_next)
		if (file_aline(sp, 1, lno++, tp->lb, tp->len)) {
			rval = 1;
			goto ret;
		}

	/*
	 * Set sp->lno to the final line number value (correcting for a
	 * possible 0 value) as that's historically correct for the final
	 * line value, whether or not the user entered any text.
	 */
	if ((sp->lno = lno) == 0 && file_eline(sp, 1))
		sp->lno = 1;

ret:	if (F_ISSET(sp, S_VI)) {
		/* The screen must be completely refreshed. */
		F_SET(sp, S_EX_WROTE | S_SCR_REFRESH);

		if (sp->gp->scr_canon(sp, 1)) {
			msgq(sp, M_ERR, "282|Unable to restore vi screen");
			return (1);
		}
	}
	return (rval);
}
