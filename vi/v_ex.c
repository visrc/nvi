/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 10.16 1995/10/17 08:10:45 bostic Exp $ (Berkeley) $Date: 1995/10/17 08:10:45 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "vi.h"

static int v_ex_cmd __P((SCR *, VICMD *, EXCMD *));
static int v_ex_done __P((SCR *, VICMD *));

/*
 * v_again -- &
 *	Repeat the previous substitution.
 *
 * PUBLIC: int v_again __P((SCR *, VICMD *));
 */
int
v_again(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	ex_cbuild(&cmd, C_SUBAGAIN,
	    2, vp->m_start.lno, vp->m_start.lno, 1, ap, &a, "");
	return (v_ex_cmd(sp, vp, &cmd));
}

/*
 * v_exmode -- Q
 *	Switch the editor into EX mode.
 *
 * PUBLIC: int v_exmode __P((SCR *, VICMD *));
 */
int
v_exmode(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	/* Try and switch screens -- the screen may not permit it. */
	if (sp->gp->scr_screen(sp, S_EX)) {
		msgq(sp, M_ERR,
		    "207|The Q command requires the ex terminal interface");
		return (1);
	}

	/* Save the current cursor position. */
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

	/* Switch to ex mode. */
	F_CLR(sp, S_VI);
	F_SET(sp, S_EX);
	return (0);
}

/*
 * v_join -- [count]J
 *	Join lines together.
 *
 * PUBLIC: int v_join __P((SCR *, VICMD *));
 */
int
v_join(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	ARGS *ap[2], a;
	EXCMD cmd;
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
	return (v_ex_cmd(sp, vp, &cmd));
}

/*
 * v_shiftl -- [count]<motion
 *	Shift lines left.
 *
 * PUBLIC: int v_shiftl __P((SCR *, VICMD *));
 */
int
v_shiftl(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	ex_cbuild(&cmd, C_SHIFTL,
	    2, vp->m_start.lno, vp->m_stop.lno, 0, ap, &a, "<");
	return (v_ex_cmd(sp, vp, &cmd));
}

/*
 * v_shiftr -- [count]>motion
 *	Shift lines right.
 *
 * PUBLIC: int v_shiftr __P((SCR *, VICMD *));
 */
int
v_shiftr(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	ex_cbuild(&cmd, C_SHIFTR,
	    2, vp->m_start.lno, vp->m_stop.lno, 0, ap, &a, ">");
	return (v_ex_cmd(sp, vp, &cmd));
}

/*
 * v_switch -- ^^
 *	Switch to the previous file.
 *
 * PUBLIC: int v_switch __P((SCR *, VICMD *));
 */
int
v_switch(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	ARGS *ap[2], a;
	EXCMD cmd;
	char *name;

	/*
	 * Try the alternate file name, then the previous file
	 * name.  Use the real name, not the user's current name.
	 */
	if ((name = sp->alt_name) == NULL) {
		msgq(sp, M_ERR, "180|No previous file to edit");
		return (1);
	}

	/* If autowrite is set, write out the file. */
	if (file_m1(sp, 0, FS_ALL))
		return (1);

	ex_cbuild(&cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0, ap, &a, name);
	return (v_ex_cmd(sp, vp, &cmd));
}

/*
 * v_tagpush -- ^[
 *	Do a tag search on the cursor keyword.
 *
 * PUBLIC: int v_tagpush __P((SCR *, VICMD *));
 */
int
v_tagpush(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	ex_cbuild(&cmd, C_TAG, 0, OOBLNO, 0, 0, ap, &a, VIP(sp)->keyw);
	return (v_ex_cmd(sp, vp, &cmd));
}

/*
 * v_tagpop -- ^T
 *	Pop the tags stack.
 *
 * PUBLIC: int v_tagpop __P((SCR *, VICMD *));
 */
int
v_tagpop(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	ex_cbuild(&cmd, C_TAGPOP, 0, OOBLNO, 0, 0, ap, &a, NULL);
	return (v_ex_cmd(sp, vp, &cmd));
}

/*
 * v_filter -- [count]!motion command(s)
 *	Run range through shell commands, replacing text.
 *
 * PUBLIC: int v_filter __P((SCR *, VICMD *));
 */
int
v_filter(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	ARGS *ap[2], a;
	EXCMD cmd;
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
	if (F_ISSET(vp, VC_ISDOT) ||
	    ISCMD(vp->rkp, 'N') || ISCMD(vp->rkp, 'n')) {
		ex_cbuild(&cmd, C_BANG,
		    2, vp->m_start.lno, vp->m_stop.lno, 0, ap, &a, NULL);
		EXP(sp)->argsoff = 0;			/* XXX */

		if (argv_exp1(sp, &cmd, "!", 1, 1))
			return (1);
		cmd.argc = EXP(sp)->argsoff;		/* XXX */
		cmd.argv = EXP(sp)->args;		/* XXX */
		return (v_ex_cmd(sp, vp, &cmd));
	}

	/* Get the command from the user. */
	if (v_tcmd(sp, vp,
	    '!', TXT_BS | TXT_CR | TXT_ESCAPE | TXT_FILEC | TXT_PROMPT))
		return (1);

	/*
	 * Check to see if the user changed their mind.
	 *
	 * !!!
	 * Entering <escape> on an empty line was historically an error,
	 * this implementation doesn't bother.
	 */
	tp = sp->tiq.cqh_first;
	if (tp->term != TERM_OK) {
		vp->m_final.lno = sp->lno;
		vp->m_final.cno = sp->cno;
		return (0);
	}

	ex_cbuild(&cmd, C_BANG,
	    2, vp->m_start.lno, vp->m_stop.lno, 0, ap, &a, NULL);
	EXP(sp)->argsoff = 0;			/* XXX */

	if (argv_exp1(sp, &cmd, tp->lb + 1, tp->len - 1, 1))
		return (1);
	cmd.argc = EXP(sp)->argsoff;		/* XXX */
	cmd.argv = EXP(sp)->args;		/* XXX */
	return (v_ex_cmd(sp, vp, &cmd));
}

/*
 * v_event_exec --
 *	Execute some command(s) based on an event.
 *
 * PUBLIC: int v_event_exec __P((SCR *, VICMD *));
 */
int
v_event_exec(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	switch (vp->ev.e_event) {
	case E_QUIT:
		ex_cbuild(&cmd, C_QUIT, 0, OOBLNO, OOBLNO, 0, ap, &a, NULL);
		break;
	case E_WRITE:
		ex_cbuild(&cmd, C_WRITE, 0, OOBLNO, OOBLNO, 0, ap, &a, NULL);
		break;
	case E_WRITEQUIT:
		ex_cbuild(&cmd, C_WQ, 0, OOBLNO, OOBLNO, 0, ap, &a, NULL);
		break;
	default:
		abort();
	}
	return (v_ex_cmd(sp, vp, &cmd));
}

/*
 * v_ex_cmd --
 *	Execute an ex command.
 */
static int
v_ex_cmd(sp, vp, exp)
	SCR *sp;
	VICMD *vp;
	EXCMD *exp;
{
	int rval;

	rval = exp->cmd->fn(sp, exp);
	return (v_ex_done(sp, vp) || rval);
}

/*
 * v_ex -- :
 *	Execute a colon command line.
 *
 * PUBLIC: int v_ex __P((SCR *, VICMD *));
 */
int
v_ex(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	GS *gp;
	TEXT *tp;
	int colon, can_continue;

	/*
	 * !!!
	 * If we put out more than a single line of messages, or ex trashes
	 * the screen, the user may continue entering ex commands.  We find
	 * this out when we do the screen/message resolution.  We can't enter
	 * completely into ex mode however, because the user can elect to
	 * return into vi mode by entering any key, i.e. we have to be in raw
	 * mode.
	 */
	for (;;) {
		/*
		 * !!!
		 * There may already be an ex command waiting.  If so, we
		 * continue with it.
		 */
		if (!EXCMD_RUNNING(sp->gp)) {
			/* Get a command. */
			if (v_tcmd(sp,
			    vp, ':', TXT_BS | TXT_FILEC | TXT_PROMPT))
				return (1);

			/* If the user didn't enter anything, we're done. */
			tp = sp->tiq.cqh_first;
			if (tp->term == TERM_BS) {
				vp->m_final.lno = sp->lno;
				vp->m_final.cno = sp->cno;
				return (0);
			}

			/*
			 * Push a command on the command stack.  The ex parser
			 * removes and discards the structure when the command
			 * finishes.
			 *
			 * Don't specify any of the E_* flags, we get correct
			 * behavior because we're in vi mode.
			 */
			gp = sp->gp;
			gp->excmd.cp = tp->lb;
			gp->excmd.clen = tp->len;
			F_INIT(&gp->excmd, 0);
		}

		/* Call the ex parser. */
		(void)ex_cmd(sp);

		/* Return if we were interrupted or left the screen .*/
		can_continue = INTERRUPTED(sp) || 
		    !F_ISSET(sp, S_EXIT | S_EXIT_FORCE | S_SSWITCH);

		/* Resolve messages. */
		if (vs_ex_resolve(sp, can_continue ? &colon : NULL))
			return (1);

		if (!colon || !can_continue)
			break;
	}
	return (v_ex_done(sp, vp));
}

/*
 * v_ex_done --
 *	Cleanup from an ex command.
 */
static int
v_ex_done(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	size_t len;

	/*
	 * The only cursor modifications are real, however, the underlying
	 * line may have changed; don't trust anything.  This code has been
	 * a remarkably fertile place for bugs.  Do a reality check on a
	 * cursor value, and make sure it's okay.  If necessary, change it.
	 * Ex keeps track of the line number, but it cares less about the
	 * column and it may have disappeared.
	 *
	 * Don't trust ANYTHING.
	 *
	 * XXX
	 * Ex will soon have to start handling the column correctly; see
	 * the POSIX 1003.2 standard.
	 */
	if (db_eget(sp, sp->lno, NULL, &len, NULL)) {
		sp->lno = 1;
		sp->cno = 0;
	} else if (sp->cno >= len)
		sp->cno = len ? len - 1 : 0;

	vp->m_final.lno = sp->lno;
	vp->m_final.cno = sp->cno;
	return (0);
}
