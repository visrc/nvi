/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_bang.c,v 10.9 1995/09/25 11:11:06 bostic Exp $ (Berkeley) $Date: 1995/09/25 11:11:06 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"

/*
 * ex_bang -- :[line [,line]] ! command
 *
 * Pass the rest of the line after the ! character to the program named by
 * the O_SHELL option.
 *
 * Historical vi did NOT do shell expansion on the arguments before passing
 * them, only file name expansion.  This means that the O_SHELL program got
 * "$t" as an argument if that is what the user entered.  Also, there's a
 * special expansion done for the bang command.  Any exclamation points in
 * the user's argument are replaced by the last, expanded ! command.
 *
 * There's some fairly amazing slop in this routine to make the different
 * ways of getting here display the right things.  It took a long time to
 * get it right (wrong?), so be careful.
 *
 * PUBLIC: int ex_bang __P((SCR *, EXCMD *));
 */
int
ex_bang(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	enum filtertype ftype;
	ARGS *ap;
	EX_PRIVATE *exp;
	MARK rm;
	recno_t lno;
	size_t blen;
	int rval;
	char *bp, *msg;

	ap = cmdp->argv[0];
	if (ap->len == 0) {
		ex_message(sp, cmdp->cmd->usage, EXM_USAGE);
		return (1);
	}

	/* Set the "last bang command" remembered value. */
	exp = EXP(sp);
	if (exp->lastbcomm != NULL)
		free(exp->lastbcomm);
	if ((exp->lastbcomm = strdup(ap->bp)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}

	/*
	 * If the command was modified by the expansion, it was historically
	 * redisplayed.
	 */
	bp = NULL;
	if (F_ISSET(cmdp, E_MODIFY) && !F_ISSET(sp, S_EX_SILENT)) {
		/*
		 * Display the command if modified.  Historic ex/vi displayed
		 * the command if it was modified due to file name and/or bang
		 * expansion.  If piping lines in vi, it would be immediately
		 * overwritten by any error or line change reporting.  We don't
		 * want the user to have to enter a key because of this message,
		 * so we display it as a busy message.
		 *
		 * If that's not possible, i.e. we're entering canonical mode,
		 * pass it on to the ex_exec_proc routine to display after the
		 * screen has been reset.
		 */
		if (F_ISSET(sp, S_VI)) {
			GET_SPACE_RET(sp, bp, blen, ap->len + 3);
			bp[0] = '!';
			memmove(bp + 1, ap->bp, ap->len);
			bp[ap->len + 1] = '\n';
			bp[ap->len + 2] = '\0';
		} else if (F_ISSET(sp, S_EX)) {
			(void)ex_printf(sp, "!%s\n", ap->bp);
			(void)ex_fflush(sp);
		}
	}

	/*
	 * If no addresses were specified, run the command.  If the file has
	 * been modified and autowrite is set, write the file back.  If the
	 * file has been modified, autowrite is not set and the warn option is
	 * set, tell the user about the file.
	 */
	if (cmdp->addrcnt == 0) {
		msg = NULL;
		if (F_ISSET(sp->ep, F_MODIFIED))
			if (O_ISSET(sp, O_AUTOWRITE)) {
				if (file_aw(sp, FS_ALL)) {
					rval = 1;
					goto ret1;
				}
			} else if (O_ISSET(sp, O_WARN) &&
			    !F_ISSET(sp, S_EX_SILENT))
				msg = "File modified since last write.\n";

		rval = ex_exec_proc(sp, cmdp, ap->bp, msg, bp);
	}

	/*
	 * If addresses were specified, pipe lines from the file through the
	 * command.
	 *
	 * Historically, vi lines were replaced by both the stdout and stderr
	 * lines of the command, but ex by only the stdout lines.  This makes
	 * no sense to me, so nvi makes it consistent for both, and matches
	 * vi's historic behavior.
	 */
	else {
		NEEDFILE(sp, cmdp);

		/* Autoprint is set historically, even if the command fails. */
		F_SET(cmdp, E_AUTOPRINT);

		/*
		 * Vi gets a busy message.
		 *
		 * !!!
		 * Busy messages don't want the <newline> at the end of the
		 * message.
		 */
		if (bp != NULL) {
			bp[ap->len + 1] = '\0';
			(void)sp->gp->scr_busy(sp, bp, 1);
		}

		/*
		 * !!!
		 * Historical vi permitted "!!" in an empty file.  When it
		 * happens, we get called with two addresses of 1,1 and a
		 * bad attitude.  The simple solution is to turn it into a
		 * FILTER_READ operation, with the exception that stdin isn't
		 * opened for the utility.  This means that we don't put an
		 * empty line into the default cut buffer as did historic vi.
		 * Imagine, if you can, my disappointment.
		 */
		ftype = FILTER_BANG;
		if (cmdp->addr1.lno == 1 && cmdp->addr2.lno == 1) {
			if (file_lline(sp, &lno))
				return (1);
			if (lno == 0) {
				cmdp->addr1.lno = cmdp->addr2.lno = 0;
				ftype = FILTER_RBANG;
			}
		}
		rval = filtercmd(sp, cmdp,
		    &cmdp->addr1, &cmdp->addr2, &rm, ap->bp, ftype);

		/*
		 * If in vi mode, move to the first nonblank.
		 *
		 * !!!
		 * Historic vi wasn't consistent in this area -- if you used
		 * a forward motion it moved to the first nonblank, but if you
		 * did a backward motion it didn't.  And, if you followed a
		 * backward motion with a forward motion, it wouldn't move to
		 * the nonblank for either.  Going to the nonblank generally
		 * seems more useful and consistent, so we do it.
		 */
		sp->lno = rm.lno;
		sp->cno = rm.cno;
		if (rval == 0 && F_ISSET(sp, S_VI)) {
			sp->cno = 0;
			(void)nonblank(sp, sp->lno, &sp->cno);
		}
	}

	if (F_ISSET(sp, S_EX)) {
		/*
		 * Make sure all ex messages are flushed out so they aren't
		 * confused with any autoprint output.
		 */
		(void)ex_fflush(sp);

		/* Ex terminates with a bang, even if the command fails. */
		if (!F_ISSET(sp, S_EX_SILENT))
			(void)write(STDOUT_FILENO, "!\n", 2);
	}

ret1:	if (bp != NULL)
		FREE_SPACE(sp, bp, blen);

	/*
	 * XXX
	 * The ! commands never return an error, so that autoprint always
	 * happens in the ex parser.
	 */
	return (0);
}
