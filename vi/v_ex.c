/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 10.7 1995/07/04 12:45:56 bostic Exp $ (Berkeley) $Date: 1995/07/04 12:45:56 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
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
	/* Save the current line/column number. */
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
 *	Do a tag search on a the cursor keyword.
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
	if (v_tcmd_setup(sp, vp,
	    '!', TXT_BS | TXT_CR | TXT_ESCAPE | TXT_PROMPT))
		return (1);
	VIP(sp)->cm_next = VS_FILTER_TEARDOWN;
	return (0);
}

/*
 * v_filter_td --
 *	Tear down the filter text input setup and run the filter command.
 *
 * PUBLIC: int v_filter_td __P((SCR *, VICMD *));
 */
int
v_filter_td(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	ARGS *ap[2], a;
	EXCMD cmd;
	TEXT *tp;

	/*
	 * Check to see if the user changed their mind.
	 *
	 * !!!
	 * Entering <escape> on an empty line was historically
	 * an error, this implementation doesn't bother.
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
 * !!!
 * There's some tricky stuff going on here to handle when a user has mapped
 * a key to multiple ex commands, or, more simply, enters more than a single
 * ex command.  Historic practice was that vi ran without any special actions,
 * as if the user were entering the characters, until ex trashed the screen,
 * e.g. a '!' command.  At that point, we no longer know what the screen looks
 * like, and can't afford to overwrite anything.  The solution is to go into
 * real ex mode until we get to the end of the command strings.
 *
 * PUBLIC: int v_ex __P((SCR *, VICMD *));
 */
int
v_ex(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	if (v_tcmd_setup(sp, vp, ':', TXT_BS | TXT_PROMPT))
		return (1);
	VIP(sp)->cm_next = VS_EX_TEARDOWN1;
	return (0);
}

/*
 * v_ex_td1 --
 *	Tear down the colon text input setup and run the ex command.
 *
 * PUBLIC: int v_ex_td1 __P((SCR *, VICMD *));
 */
int
v_ex_td1(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	GS *gp;
	TEXT *tp;

	/* If the user didn't enter anything, we're done. */
	tp = sp->tiq.cqh_first;
	if (tp->term == TERM_BS) {
		vp->m_final.lno = sp->lno;
		vp->m_final.cno = sp->cno;
		return (0);
	}

	/*
	 * Create and push a command on the command stack.  The ex parser
	 * will remove and discard the structure when the command finishes.
	 *
	 * Don't specify any of the E_* flags, we get the correct behavior
	 * because we're in vi mode.
	 */
	gp = sp->gp;
	gp->cm_state = ES_PARSE;
	gp->excmd.cp = tp->lb;
	gp->excmd.clen = tp->len;
	F_INIT(&gp->excmd, 0);
	return (v_ex_td2(sp, vp));
}

/*
 * v_ex_td2 --
 *	Start or continue with the ex command.
 *
 * PUBLIC: int v_ex_td2 __P((SCR *, VICMD *));
 */
int
v_ex_td2(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	GS *gp;
	VI_PRIVATE *vip;

	gp = sp->gp;
	vip = VIP(sp);

	/*
	 * Vi is in one of two states:
	 *
	 *	VS_EX_TEARDOWN1	-- haven't called the ex parser.
	 *	VS_EX_TEARDOWN2 -- restarting the ex parser.
	 *
	 * If the latter, update the ex state, the running command has
	 * finished.
	 */
	switch (vip->cm_state) {
	case VS_EX_TEARDOWN1:
		break;
	case VS_EX_TEARDOWN2:
		gp->cm_state = gp->cm_next;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	/*
	 * Call the ex parser.  The ex parser will be in one of two states
	 * when it returns:
	 *
	 *	ES_PARSE:	Done.
	 *	ES_RUNNING:	Not done.
	 *
	 * The first one is easy, clean up and return.  The second means we
	 * clean up and return, but also set things up so that we'll continue
	 * the command (there's additional code in the main vi loop to make
	 * this happen.)
	 */
	if (ex_cmd(sp))
		return (1);

	switch (gp->cm_state) {
	case ES_PARSE:
		/*
		 * We've just completed an ex command, catch up on messages,
		 * and possibly leave canonical mode.  This is all done by
		 * the screen so that it's possible to do it in a variety of
		 * different ways.
		 */
		F_SET(sp, S_COMPLETE_EX);
		if (F_ISSET(sp, S_EX_CANON))
			F_SET(vip, VIP_SKIPREFRESH);
		return (v_ex_done(sp, vp));
	case ES_RUNNING:
		vip->run_func = EXP(sp)->run_func;
		vip->cm_state = VS_RUNNING;
		vip->cm_next = VS_EX_TEARDOWN2;
		F_SET(vip, VIP_SKIPREFRESH);
		break;
	default:
		abort();
	}
	return (0);
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
	recno_t lno;
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
	if (file_gline(sp, sp->lno, &len) == NULL) {
		if (file_lline(sp, &lno))
			return (1);
		if (lno != 0)
			FILE_LERR(sp, sp->lno);
		sp->lno = 1;
		sp->cno = 0;
	} else if (sp->cno >= len)
		sp->cno = len ? len - 1 : 0;

	vp->m_final.lno = sp->lno;
	vp->m_final.cno = sp->cno;
	return (0);
}
