/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_bang.c,v 8.12 1993/11/03 17:18:46 bostic Exp $ (Berkeley) $Date: 1993/11/03 17:18:46 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "sex/sex_screen.h"

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
 */
int
ex_bang(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	enum filtertype ftype;
	recno_t lno;
	CHAR_T ch;
	MARK rm;
	size_t blen, len;
	int rval;
	char *bp, *msg;

	if (cmdp->argv[0][0] == '\0') {
		msgq(sp, M_ERR, "Usage: %s", cmdp->cmd->usage);
		return (1);
	}

	/* Swap commands. */
	if (sp->lastbcomm != NULL)
		FREE(sp->lastbcomm, strlen(sp->lastbcomm) + 1);
	sp->lastbcomm = strdup(cmdp->argv[0]);

	/* Display the command if modified. */
	if (F_ISSET(cmdp, E_MODIFY)) {
		if (F_ISSET(sp, S_MODE_EX)) {
			(void)ex_printf(EXCOOKIE, "!%s\n", cmdp->argv[0]);
			(void)ex_fflush(EXCOOKIE);
		}
		/*
		 * !!!
		 * Historic vi redisplayed the command if it was modified due
		 * to file name and/or bang expansion.  This was immediately
		 * erased by any error or line change reporting.  We don't want
		 * to force the user to page through the responses, so we only
		 * put it up until it's erased by something else.
		 */
		if (F_ISSET(sp, S_MODE_VI)) {
			len = strlen(cmdp->argv[0]);
			GET_SPACE(sp, bp, blen, len + 2);
			bp[0] = '!';
			memmove(bp + 1, cmdp->argv[0], len + 1);
			(void)sp->s_busy(sp, bp);
			FREE_SPACE(sp, bp, blen);
		}
	}

	/*
	 * If addresses were specified, pipe lines from the file through
	 * the command.
	 */
	if (cmdp->addrcnt != 0) {

		/*
		 * !!!
		 * Historical vi permitted "!!" in an empty file, for no
		 * immediately apparent reason.  When it happens, we end
		 * up here with line addresses of 0,1 and a bad attitude.
		 */
		ftype = FILTER;
		if (cmdp->addr1.lno == 0 && cmdp->addr2.lno == 1) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno == 0) {
				cmdp->addr1.lno = cmdp->addr2.lno = 0;
				ftype = FILTER_READ;
			}
		}
		if (filtercmd(sp, ep,
		    &cmdp->addr1, &cmdp->addr2, &rm, cmdp->argv[0], ftype))
			return (1);
		sp->lno = rm.lno;
		F_SET(sp, S_AUTOPRINT);
		return (0);
	}

	/*
	 * If no addresses were specified, run the command.  If the file
	 * has been modified and autowrite is set, write the file back.
	 * If the file has been modified, autowrite is not set and the
	 * warn option is set, tell the user about the file.
	 */
	if (F_ISSET(ep, F_MODIFIED)) {
		if (O_ISSET(sp, O_AUTOWRITE)) {
			if (file_write(sp, ep, NULL, NULL, NULL, FS_ALL))
				return (1);
		} else if (O_ISSET(sp, O_WARN))
			if (F_ISSET(sp, S_MODE_VI) &&
			    F_ISSET(cmdp, E_MODIFY))
				msg = "\nFile modified since last write.\n";
			else
				msg = "File modified since last write.\n";
	} else
		msg = "\n";

	/* Run the command. */
	rval = ex_exec_proc(sp, O_STR(sp, O_SHELL), cmdp->argv[0], msg);

	/* Ex terminates with a bang. */
	if (F_ISSET(sp, S_MODE_EX))
		(void)write(STDOUT_FILENO, "!\n", 2);

	/* Vi will repaint the screen; get the user to okay it. */
	if (F_ISSET(sp, S_MODE_VI)) {
		(void)write(STDOUT_FILENO, CONTMSG, sizeof(CONTMSG) - 1);
		for (;;) {
			if (term_key(sp, &ch, 0) != INP_OK)
				break;
			if (sp->special[ch] == K_CR ||
			    sp->special[ch] == K_NL || isblank(ch))
				break;
			(void)sp->s_bell(sp);
		}
		F_SET(sp, S_REFRESH);
	}

	return (rval);
}
