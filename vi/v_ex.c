/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 9.4 1995/01/30 09:13:05 bostic Exp $ (Berkeley) $Date: 1995/01/30 09:13:05 $";
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

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"
#include "../svi/svi_screen.h"

/*
 * v_again -- &
 *	Repeat the previous substitution.
 */
int
v_again(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;

	ex_cbuild(&cmd, C_SUBAGAIN,
	    2, vp->m_start.lno, vp->m_start.lno, 1, ap, &a, "");
	return (svi_ex_cmd(sp, &cmd, &vp->m_final));
}

/*
 * v_at -- @
 *	Execute a buffer.
 */
int
v_at(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;

        ex_cbuild(&cmd, C_AT, 0, OOBLNO, OOBLNO, 0, ap, &a, NULL);
	if (F_ISSET(vp, VC_BUFFER)) {
		F_SET(&cmd, E_BUFFER);
		cmd.buffer = vp->buffer;
	}
        return (svi_ex_cmd(sp, &cmd, &vp->m_final));
}

/*
 * v_ex -- :
 *	Execute a colon command line.
 */
int
v_ex(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	return (svi_ex_run(sp, &vp->m_final));
}

/*
 * v_exmode -- Q
 *	Switch the editor into EX mode.
 */
int
v_exmode(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	/* Save the current line/column number. */
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

	/* Switch to ex mode. */
	sp->saved_vi_mode = F_ISSET(sp, S_VI);
	F_CLR(sp, S_SCREENS);
	F_SET(sp, S_EX);
	return (0);
}

/*
 * v_filter -- [count]!motion command(s)
 *	Run range through shell commands, replacing text.
 */
int
v_filter(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;
	TEXT *tp;

	/*
	 * !!!
	 * Historical vi permitted "!!" in an empty file, and it's handled
	 * as a special case in the ex_bang routine.  Don't modify this setup
	 * without understanding that one.  In particular, note that we're
	 * manipulating the ex argument structures behind ex's back.
	 *
	 * !!!
	 * Historical vi did not permit the '!' command to be associated with
	 * a non-line oriented motion command, in general, although it did
	 * with search commands.  So, !f; and !w would fail, but !/;<CR>
	 * would succeed, even if they all moved to the same location in the
	 * current line.  I don't see any reason to disallow '!' using any of
	 * the possible motion commands.
	 *
	 * !!!
	 * Historical vi ran the last bang command if N or n was used as the
	 * search motion.
	 */
	ex_cbuild(&cmd, C_BANG,
	    2, vp->m_start.lno, vp->m_stop.lno, 0, ap, &a, NULL);
	EXP(sp)->argsoff = 0;			/* XXX */
	if (F_ISSET(vp, VC_ISDOT) ||
	    ISCMD(vp->rkp, 'N') || ISCMD(vp->rkp, 'n')) {
		if (argv_exp1(sp, &cmd, "!", 1, 1))
			return (1);
	} else {
		/* Get the command from the user. */
		if (svi_get(sp, sp->tiqp,
		    '!', TXT_BS | TXT_CR | TXT_ESCAPE | TXT_PROMPT) != INP_OK)
			return (1);

		/*
		 * Check to see if the user changed their mind.
		 *
		 * !!!
		 * Entering <escape> on an empty line was historically
		 * an error, this implementation doesn't bother.
		 */
		tp = sp->tiqp->cqh_first;
		if (tp->term != TERM_OK)
			return (0);

		if (argv_exp1(sp, &cmd, tp->lb + 1, tp->len - 1, 1))
			return (1);
	}
	cmd.argc = EXP(sp)->argsoff;		/* XXX */
	cmd.argv = EXP(sp)->args;		/* XXX */
	return (svi_ex_cmd(sp, &cmd, &vp->m_final));
}

/*
 * v_join -- [count]J
 *	Join lines together.
 */
int
v_join(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;
	int lno;

	/*
	 * YASC.
	 * The general rule is that '#J' joins # lines, counting the current
	 * line.  However, 'J' and '1J' are the same as '2J', i.e. join the
	 * current and next lines.  This doesn't map well into the ex command
	 * (which takes two line numbers), so we handle it here.  Note that
	 * we never test for EOF -- historically going past the end of file
	 * worked just fine.
	 */
	lno = vp->m_start.lno + 1;
	if (F_ISSET(vp, VC_C1SET) && vp->count > 2)
		lno = vp->m_start.lno + (vp->count - 1);

	ex_cbuild(&cmd, C_JOIN, 2, vp->m_start.lno, lno, 0, ap, &a, NULL);
	return (svi_ex_cmd(sp, &cmd, &vp->m_final));
}

/*
 * v_shiftl -- [count]<motion
 *	Shift lines left.
 */
int
v_shiftl(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;

	ex_cbuild(&cmd, C_SHIFTL,
	    2, vp->m_start.lno, vp->m_stop.lno, 0, ap, &a, "<");
	return (svi_ex_cmd(sp, &cmd, &vp->m_final));
}

/*
 * v_shiftr -- [count]>motion
 *	Shift lines right.
 */
int
v_shiftr(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;

	ex_cbuild(&cmd, C_SHIFTR,
	    2, vp->m_start.lno, vp->m_stop.lno, 0, ap, &a, ">");
	return (svi_ex_cmd(sp, &cmd, &vp->m_final));
}

/*
 * v_switch -- ^^
 *	Switch to the previous file.
 */
int
v_switch(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;
	char *name;

	/*
	 * Try the alternate file name, then the previous file
	 * name.  Use the real name, not the user's current name.
	 */
	if ((name = sp->alt_name) == NULL) {
		msgq(sp, M_ERR, "176|No previous file to edit");
		return (1);
	}

	/* If autowrite is set, write out the file. */
	if (file_m1(sp, 0, FS_ALL))
		return (1);

	ex_cbuild(&cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0, ap, &a, name);
	return (svi_ex_cmd(sp, &cmd, &vp->m_final));
}

/*
 * v_tagpush -- ^[
 *	Do a tag search on a the cursor keyword.
 */
int
v_tagpush(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;

	ex_cbuild(&cmd, C_TAG, 0, OOBLNO, 0, 0, ap, &a, vp->keyword);
	return (svi_ex_cmd(sp, &cmd, &vp->m_final));
}

/*
 * v_tagpop -- ^T
 *	Pop the tags stack.
 */
int
v_tagpop(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;

	ex_cbuild(&cmd, C_TAGPOP, 0, OOBLNO, 0, 0, ap, &a, NULL);
	return (svi_ex_cmd(sp, &cmd, &vp->m_final));
}
