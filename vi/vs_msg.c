/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_msg.c,v 10.37 1995/11/11 10:03:31 bostic Exp $ (Berkeley) $Date: 1995/11/11 10:03:31 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "vi.h"

typedef enum {
	SCROLL_W,			/* User wait. */
	SCROLL_W_EX,			/* User wait, or enter : to continue. */
	SCROLL_W_QUIT			/* User wait, or enter q to quit. */
					/*
					 * SCROLL_W_QUIT has another semantic
					 * -- only wait if the screen is full
					 */
} sw_t;

static void	vs_divider __P((SCR *));
static void	vs_output __P((SCR *, mtype_t, const char *, int));
static void	vs_msgsave __P((SCR *, mtype_t, char *, size_t));
static void	vs_scroll __P((SCR *, int *, sw_t));
static void	vs_wait __P((SCR *, int *, sw_t));

/*
 * vs_busy --
 *	Display, update or clear a busy message.
 *
 * This routine is the default editor interface for vi busy messages.  It
 * implements a standard strategy of stealing lines from the bottom of the
 * vi text screen.  Screens using an alternate method of displaying busy
 * messages, e.g. X11 clock icons, should set their scr_busy function to the
 * correct function before calling the main editor routine.
 *
 * PUBLIC: void vs_busy __P((SCR *, const char *, busy_t));
 */
void
vs_busy(sp, msg, btype)
	SCR *sp;
	const char *msg;
	busy_t btype;
{
	GS *gp;
	VI_PRIVATE *vip;
	static const char flagc[] = "|/-|-\\";
	struct timeval tv;
	size_t len, notused;
	const char *p;

	/* Ex doesn't display busy messages. */
	if (F_ISSET(sp, S_EX | S_SCR_EXWROTE))
		return;

	gp = sp->gp;
	vip = VIP(sp);

	/*
	 * Most of this routine is to deal with the screen sharing real estate
	 * between the normal edit messages and the busy messages.  Logically,
	 * all that's needed is something that puts up a message, periodically
	 * updates it, and then goes away.
	 */
	switch (btype) {
	case BUSY_ON:
		++vip->busy_ref;
		if (vip->totalcount != 0 || vip->busy_ref != 1)
			break;

		/* Initialize state for updates. */
		vip->busy_ch = 0;
		(void)gettimeofday(&vip->busy_tv, NULL);

		/* Save the current cursor. */
		(void)gp->scr_cursor(sp, &vip->busy_oldy, &vip->busy_oldx);

		/* Display the busy message. */
		p = msg_cat(sp, msg, &len);
		(void)gp->scr_move(sp, LASTLINE(sp), 0);
		(void)gp->scr_addstr(sp, p, len);
		(void)gp->scr_cursor(sp, &notused, &vip->busy_fx);
		(void)gp->scr_clrtoeol(sp);
		(void)gp->scr_move(sp, LASTLINE(sp), vip->busy_fx);
		break;
	case BUSY_OFF:
		if (vip->busy_ref == 0)
			break;
		--vip->busy_ref;

		/*
		 * If the line isn't in use for another purpose, clear it.
		 * Always return to the original position.
		 */
		if (vip->totalcount == 0 && vip->busy_ref == 0) {
			(void)gp->scr_move(sp, LASTLINE(sp), 0);
			(void)gp->scr_clrtoeol(sp);
		}
		(void)gp->scr_move(sp, vip->busy_oldy, vip->busy_oldx);
		break;
	case BUSY_UPDATE:
		if (vip->totalcount != 0 || vip->busy_ref == 0)
			break;

		/* Update no more than every 1/4 of a second. */
		(void)gettimeofday(&tv, NULL);
		if (((tv.tv_sec - vip->busy_tv.tv_sec) * 1000000 +
		    (tv.tv_usec - vip->busy_tv.tv_usec)) < 4000)
			return;

		/* Display the update. */
		if (vip->busy_ch == sizeof(flagc))
			vip->busy_ch = 0;
		(void)gp->scr_move(sp, LASTLINE(sp), vip->busy_fx);
		(void)gp->scr_addstr(sp, flagc + vip->busy_ch++, 1);
		(void)gp->scr_move(sp, LASTLINE(sp), vip->busy_fx);
		break;
	}
	(void)gp->scr_refresh(sp, 0);
}

/*
 * vs_update --
 *	Update a command.
 *
 * PUBLIC: void vs_update __P((SCR *, char *, char *));
 */
void
vs_update(sp, m1, m2)
	SCR *sp;
	char *m1, *m2;
{
	GS *gp;
	size_t len, mlen, oldy, oldx;

	/*
	 * This routine displays a message on the bottom line of the screen,
	 * without updating any of the command structures that would keep it
	 * there for any period of time, i.e. it is overwritten immediately.
	 *
	 * It's used by the ex read and ! commands when the user's command is
	 * expanded, and by the ex substitution confirmation prompt.
	 */
	if (F_ISSET(sp, S_SCR_EXWROTE)) {
		(void)ex_printf(sp,
		    "%s\n", m1 == NULL? "" : m1, m2 == NULL ? "" : m2);
		(void)ex_fflush(sp);
	}

	gp = sp->gp;
	(void)gp->scr_cursor(sp, &oldy, &oldx);
	(void)gp->scr_move(sp, LASTLINE(sp), 0);
	(void)gp->scr_clrtoeol(sp);

	/*
	 * XXX
	 * Don't let long file names screw up the screen.
	 */
	if (m1 != NULL) {
		mlen = len = strlen(m1);
		if (len > sp->cols - 2)
			mlen = len = sp->cols - 2;
		(void)gp->scr_addstr(sp, m1, mlen);
	}
	if (m2 != NULL) {
		mlen = strlen(m2);
		if (len + mlen > sp->cols - 2)
			mlen = (sp->cols - 2) - len;
		(void)gp->scr_addstr(sp, m2, mlen);
	}

	(void)gp->scr_move(sp, oldy, oldx);
	(void)sp->gp->scr_refresh(sp, 0);
}

/*
 * vs_msg --
 *	Display ex output or error messages for the screen.
 *
 * This routine is the default editor interface for all ex output, and all ex
 * and vi error/informational messages.  It implements the standard strategy
 * of stealing lines from the bottom of the vi text screen.  Screens using an
 * alternate method of displaying messages, e.g. dialog boxes, should set their
 * scr_msg function to the correct function before calling the editor.
 *
 * PUBLIC: void vs_msg __P((SCR *, mtype_t, char *, size_t));
 */
void
vs_msg(sp, mtype, line, len)
	SCR *sp;
	mtype_t mtype;
	char *line;
	size_t len;
{
	GS *gp;
	VI_PRIVATE *vip;
	size_t cols, oldx, oldy, padding;
	const char *e, *s, *t;

	gp = sp->gp;
	vip = VIP(sp);

	/*
	 * Ring the bell if it's scheduled.
	 *
	 * XXX
	 * Shouldn't we save this, too?
	 */
	if (F_ISSET(gp, G_BELLSCHED)) {
		F_CLR(gp, G_BELLSCHED);
		(void)gp->scr_bell(sp);
	}

	/*
	 * Ex or ex controlled screen output.
	 *
	 * If output happens during startup, e.g., a .exrc file, we may be
	 * in ex mode but haven't initialized the screen.  Initialize here,
	 * and in this case, stay in ex mode.
	 *
	 * If the S_SCR_EXWROTE bit is set, then we're switching back and
	 * forth between ex and vi, but the screen is trashed and we have
	 * to respect that.  Switch to ex mode long enough to put out the
	 * message.
	 *
	 * If the S_EX_DONTWAIT bit is set, turn it off -- we're writing,
	 * so previous opinions should be ignored.
	 */
	if (F_ISSET(sp, S_EX | S_SCR_EXWROTE)) {
		if (!F_ISSET(sp, S_SCR_EX)) {
			if (sp->gp->scr_screen(sp, S_EX))
				return;
			if (!F_ISSET(sp, S_SCR_EXWROTE)) {
				ex_e_resize(sp);
				F_SET(sp, S_EX | S_SCR_EX);
			}
		}

		if (mtype == M_ERR)
			(void)gp->scr_attr(sp, SA_INVERSE, 1);
		(void)printf("%.*s", (int)len, line);
		if (mtype == M_ERR)
			(void)gp->scr_attr(sp, SA_INVERSE, 0);
		(void)fflush(stdout);

		F_CLR(sp, S_EX_DONTWAIT);

		if (!F_ISSET(sp, S_SCR_EX))
			(void)sp->gp->scr_screen(sp, S_VI);
		return;
	}

	/* If the vi screen isn't ready, save the message. */
	if (!F_ISSET(sp, S_SCR_VI)) {
		(void)vs_msgsave(sp, mtype, line, len);
		return;
	}

	/* Save the cursor position. */
	(void)gp->scr_cursor(sp, &oldy, &oldx);

	/*
	 * If the message type is changing, terminate any previous message
	 * and move to a new line.
	 */
	if (mtype != vip->mtype) {
		if (vip->lcontinue != 0) {
			if (vip->mtype == M_NONE)
				vs_output(sp, vip->mtype, "\n", 1);
			else
				vs_output(sp, vip->mtype, ".\n", 2);
			vip->lcontinue = 0;
		}
		vip->mtype = mtype;
	}

	/* If it's an ex output message, just write it out. */
	if (mtype == M_NONE) {
		vs_output(sp, mtype, line, len);
		goto ret;
	}

	/*
	 * Need up to three padding characters normally; a semi-colon and
	 * two separating spaces.  If only a single line on the screen, add
	 * some more for the trailing continuation message.
	 *
	 * XXX
	 * Assume that a semi-colon takes up a single column on the screen.
	 */
	if (IS_ONELINE(sp))
		(void)msg_cmsg(sp, CMSG_CONT_S, &padding);
	else
		padding = 0;
	padding += 3;

	/*
	 * If a message won't fit on a single line, try to split on a <blank>.
	 * If a subsequent message fits on the same line, write a separator
	 * and output it.  Otherwise, put out a newline.
	 *
	 * XXX
	 * There are almost certainly pathological cases that will break this
	 * code.
	 */
	if (vip->lcontinue != 0)
		if (len + vip->lcontinue + padding >= sp->cols)
			vs_output(sp, mtype, ".\n", 2);
		else  {
			vs_output(sp, mtype, ";", 1);
			vs_output(sp, M_NONE, "  ", 2);
		}
	for (cols = sp->cols - padding, s = line; len > 0; s = t) {
		for (; isblank(*s) && --len != 0; ++s);
		if (len == 0)
			break;
		if (len > cols) {
			for (e = s + cols; e > s && !isblank(*e); --e);
			if (e == s)
				 e = t = s + cols;
			else
				for (t = e; isblank(e[-1]); --e);
		} else {
			e = s + len;
			/*
			 * XXX:
			 * If t isn't initialized for "s = t", len will be
			 * equal to 0.  Shut the freakin' compiler up.
			 */
			t = s;
		}
		len -= e - s;
		if ((e - s) > 1 && s[(e - s) - 1] == '.')
			--e;
		vs_output(sp, mtype, s, e - s);
		if (len != 0)
			vs_output(sp, M_NONE, "\n", 1);
		if (INTERRUPTED(sp))
			break;
	}

ret:	/* Restore the cursor position. */
	(void)gp->scr_move(sp, oldy, oldx);

	/* Refresh the screen. */
	(void)gp->scr_refresh(sp, 0);
}

/*
 * vs_output --
 *	Output the text to the screen.
 */
static void
vs_output(sp, mtype, line, llen)
	SCR *sp;
	mtype_t mtype;
	const char *line;
	int llen;
{
	GS *gp;
	VI_PRIVATE *vip;
	size_t notused;
	int len, rlen, tlen;
	const char *p, *t;

	gp = sp->gp;
	vip = VIP(sp);
	for (p = line, rlen = llen; llen > 0;) {
		/* Get the next physical line. */
		if ((p = memchr(line, '\n', llen)) == NULL)
			len = llen;
		else
			len = p - line;

		/*
		 * The max is sp->cols characters, and we may have already
		 * written part of the line.
		 */
		if (len + vip->lcontinue > sp->cols)
			len = sp->cols - vip->lcontinue;

		/*
		 * If the first line output, do nothing.  If the second line
		 * output, draw the divider line.  If drew a full screen, we
		 * remove the divider line.  If it's a continuation line, move
		 * to the continuation point, else, move the screen up.
		 */
		if (vip->lcontinue == 0) {
			if (!IS_ONELINE(sp)) {
				if (vip->totalcount == 1) {
					(void)gp->scr_move(sp,
					    LASTLINE(sp) - 1, 0);
					(void)gp->scr_clrtoeol(sp);
					(void)vs_divider(sp);
					F_SET(vip, VIP_DIVIDER);
					++vip->totalcount;
					++vip->linecount;
				}
				if (vip->totalcount == sp->t_maxrows &&
				    F_ISSET(vip, VIP_DIVIDER)) {
					--vip->totalcount;
					--vip->linecount;
					F_CLR(vip, VIP_DIVIDER);
				}
			}
			if (vip->totalcount != 0)
				vs_scroll(sp, NULL, SCROLL_W_QUIT);

			(void)gp->scr_move(sp, LASTLINE(sp), 0);
			++vip->totalcount;
			++vip->linecount;

			if (INTERRUPTED(sp))
				break;
		} else
			(void)gp->scr_move(sp, LASTLINE(sp), vip->lcontinue);

		/* Display the line, doing character translation. */
		if (mtype == M_ERR)
			(void)gp->scr_attr(sp, SA_INVERSE, 1);
		for (t = line, tlen = len; tlen--; ++t)
			(void)gp->scr_addstr(sp,
			    KEY_NAME(sp, *t), KEY_LEN(sp, *t));
		if (mtype == M_ERR)
			(void)gp->scr_attr(sp, SA_INVERSE, 0);

		/* Clear the rest of the line. */
		(void)gp->scr_clrtoeol(sp);

		/* If we loop, it's a new line. */
		vip->lcontinue = 0;

		/* Reset for the next line. */
		line += len;
		llen -= len;
		if (p != NULL) {
			++line;
			--llen;
		}
	}

	/* Set up next continuation line. */
	if (p == NULL)
		gp->scr_cursor(sp, &notused, &vip->lcontinue);
}

/*
 * vs_ex_resolve --
 *	Deal with ex message output.
 *
 * This routine is called when exiting a colon command to resolve any ex
 * output that may have occurred.
 *
 * PUBLIC: int vs_ex_resolve __P((SCR *, int *));
 */
int
vs_ex_resolve(sp, continuep)
	SCR *sp;
	int *continuep;
{
	CHAR_T ch;
	EVENT ev;
	GS *gp;
	VI_PRIVATE *vip;
	int cancontinue;

	gp = sp->gp;
	vip = VIP(sp);
	*continuep = 0;

	/*
	 * If we didn't switch into ex and only 0 or 1 lines of output, we may
	 * be able to continue w/o making the user wait.  First, if no lines of
	 * output, we can simply return, leaving the line modifications report
	 * to the next call to vs_resolve from the main vi loop.  Else, output
	 * that report and see if that pushes us over the edge.  If it does, we
	 * wait, else we can return, again leaving the line modification report
	 * until later.
	 *
	 * Note, all other code paths require waiting, so those cases leave the
	 * report of modified lines until later.  As a result, groups of ex
	 * commands will have cumulative line modification reports.  That seems
	 * right (well, at least not wrong) to me.
	 */
	if (!F_ISSET(sp, S_SCR_EXWROTE) && vip->totalcount < 2) {
		if (vip->totalcount == 0)
			return (0);
		msgq_rpt(sp);
		if (vip->totalcount < 2)
			return (0);
	}

	/*
	 * If we switched out of the vi screen, switch back while figuring what
	 * to do with the screen and potentially get another command to execute.
	 */
	if (F_ISSET(sp, S_SCR_EXWROTE) && sp->gp->scr_screen(sp, S_VI))
		return (1);

	/*
	 * Wait, unless explicitly told not to wait, the user interrupted
	 * the command or is leaving (or trying to leave) the screen.
	 *
	 * If the user is continuing, and we're already into an ex screen,
	 * output a <newline> so that we don't erase anything.  It has to
	 * be done here, because we never get control back if the command
	 * is all internal, e.g. :set.
	 */
	if (!F_ISSET(sp, S_EX_DONTWAIT) && !INTERRUPTED(sp) &&
	    !F_ISSET(sp, S_EXIT | S_EXIT_FORCE | S_FSWITCH | S_SSWITCH)) {
		if (F_ISSET(sp, S_SCR_EXWROTE)) {
			vs_wait(sp, continuep, SCROLL_W_EX);
			if (*continuep)
				ex_puts(sp, "\n");
		} else
			vs_scroll(sp, continuep, SCROLL_W_EX);
		if (*continuep)
			return (0);
	}

	/* If ex changed the underlying screen, redraw from scratch. */
	if (F_ISSET(sp, S_SCR_EXWROTE) || F_ISSET(vip, VIP_N_REDRAW))
		F_SET(sp, S_SCR_REDRAW);

	/*
	 * Whew.  We're finally back home, after what feels like years.
	 * Kiss the ground.
	 */
	F_CLR(sp, S_SCR_EXWROTE | S_EX_DONTWAIT);

	/*
	 * We may need to repaint some of the screen, e.g.:
	 *
	 *	:set
	 *	:!ls
	 *
	 * gives us a combination of some lines that are "wrong", and a
	 * need for a full refresh.
	 */
	if (vip->totalcount > 1) {
		/* Set up the redraw of the overwritten lines. */
		ev.e_event = E_REPAINT;
		ev.e_flno = vip->totalcount >=
		    sp->rows ? 1 : sp->rows - vip->totalcount;
		ev.e_tlno = sp->rows;

		/* Reset the count of overwriting lines. */
		vip->linecount = vip->lcontinue = vip->totalcount = 0;

		/* Redraw. */
		(void)vs_repaint(sp, &ev);
	} else
		/* Reset the count of overwriting lines. */
		vip->linecount = vip->lcontinue = vip->totalcount = 0;

	return (0);
}

/*
 * vs_resolve --
 *	Deal with message output.
 *
 * This routine is called from the main vi loop to periodically ensure that
 * the user has seen any messages that have been displayed.
 *
 * PUBLIC: int vs_resolve __P((SCR *));
 */
int
vs_resolve(sp)
	SCR *sp;
{
	EVENT ev;
	GS *gp;
	MSG *mp;
	VI_PRIVATE *vip;
	size_t oldy, oldx;
	int redraw;

	gp = sp->gp;
	vip = VIP(sp);

	/* Save the cursor position. */
	(void)gp->scr_cursor(sp, &oldy, &oldx);

	/* Ring the bell if it's scheduled. */
	if (F_ISSET(gp, G_BELLSCHED)) {
		F_CLR(gp, G_BELLSCHED);
		(void)gp->scr_bell(sp);
	}

	/* Display new file status line. */
	if (F_ISSET(sp, S_STATUS)) {
		F_CLR(sp, S_STATUS);
		msgq_status(sp, sp->lno, 0);
	}

	/* Report on line modifications. */
	msgq_rpt(sp);

	/*
	 * Flush any saved messages.  If the screen isn't ready, refresh
	 * it.  (A side-effect of screen refresh is that we can display
	 * messages.)  Once this is done, don't trust the cursor.  That
	 * extra refresh screwed the pooch.
	 */
	if (gp->msgq.lh_first != NULL) {
		if (!F_ISSET(sp, S_SCR_VI) && vs_refresh(sp))
			return (1);
		while ((mp = gp->msgq.lh_first) != NULL) {
			gp->scr_msg(sp, mp->mtype, mp->buf, mp->len);
			LIST_REMOVE(mp, q);
			free(mp->buf);
			free(mp);
		}
		F_SET(vip, VIP_CUR_INVALID);
	}

	switch (vip->totalcount) {
	case 0:
		redraw = 0;
		break;
	case 1:
		redraw = 0;

		/* Skip the modeline if it's in use. */
		F_SET(vip, VIP_S_MODELINE);
		break;
	default:
		/*
		 * If >1 message line in use, prompt the user to continue and
		 * repaint overwritten lines.
		 */
		vs_scroll(sp, NULL, SCROLL_W);

		ev.e_event = E_REPAINT;
		ev.e_flno = vip->totalcount >=
		    sp->rows ? 1 : sp->rows - vip->totalcount;
		ev.e_tlno = sp->rows;
		redraw = 1;
		break;
	}

	/* Reset the count of overwriting lines. */
	vip->linecount = vip->lcontinue = vip->totalcount = 0;

	/* Redraw. */
	if (redraw)
		(void)vs_repaint(sp, &ev);

	/* Restore the cursor position. */
	(void)gp->scr_move(sp, oldy, oldx);

	return (0);
}

/*
 * vs_scroll --
 *	Scroll the screen for output.
 */
static void
vs_scroll(sp, continuep, wtype)
	SCR *sp;
	int *continuep;
	sw_t wtype;
{
	GS *gp;
	VI_PRIVATE *vip;

	gp = sp->gp;
	vip = VIP(sp);
	if (!IS_ONELINE(sp)) {
		/*
		 * Scroll the screen.  Instead of scrolling the entire screen,
		 * delete the line above the first line output so preserve the
		 * maximum amount of the screen.
		 */
		(void)gp->scr_move(sp, vip->totalcount <
		    sp->rows ? LASTLINE(sp) - vip->totalcount : 0, 0);
		(void)gp->scr_deleteln(sp);

		/* If there are screens below us, push them back into place. */
		if (sp->q.cqe_next != (void *)&sp->gp->dq) {
			(void)gp->scr_move(sp, LASTLINE(sp), 0);
			(void)gp->scr_insertln(sp);
		}
	}
	if (wtype == SCROLL_W_QUIT && vip->linecount < sp->t_maxrows)
		return;
	vs_wait(sp, continuep, wtype);
}

/*
 * vs_wait --
 *	Prompt the user to continue.
 */
static void
vs_wait(sp, continuep, wtype)
	SCR *sp;
	int *continuep;
	sw_t wtype;
{
	EVENT ev;
	VI_PRIVATE *vip;
	const char *p;
	GS *gp;
	size_t len;

	gp = sp->gp;
	vip = VIP(sp);

	(void)gp->scr_move(sp, LASTLINE(sp), 0);
	if (IS_ONELINE(sp))
		p = msg_cmsg(sp, CMSG_CONT_S, &len);
	else
		switch (wtype) {
		case SCROLL_W_QUIT:
			p = msg_cmsg(sp, CMSG_CONT_Q, &len);
			break;
		case SCROLL_W_EX:
			p = msg_cmsg(sp, CMSG_CONT_EX, &len);
			break;
		case SCROLL_W:
			p = msg_cmsg(sp, CMSG_CONT, &len);
			break;
		}
	(void)gp->scr_addstr(sp, p, len);

	++vip->totalcount;
	vip->linecount = 0;

	(void)gp->scr_clrtoeol(sp);
	(void)gp->scr_refresh(sp, 0);

	/* Get a single character from the terminal. */
	if (continuep != NULL)
		*continuep = 0;
	for (;;) {
		if (v_event_get(sp, &ev, 0, 0))
			return;
		if (ev.e_event == E_CHARACTER)
			break;
		if (ev.e_event == E_INTERRUPT) {
			ev.e_c = CH_QUIT;
			F_SET(gp, G_INTERRUPTED);
			break;
		}
		(void)gp->scr_bell(sp);
	}
	switch (wtype) {
	case SCROLL_W_QUIT:
		if (ev.e_c == CH_QUIT)
			F_SET(gp, G_INTERRUPTED);
		break;
	case SCROLL_W_EX:
		if (ev.e_c == ':' && continuep != NULL)
			*continuep = 1;
		break;
	}
}

/*
 * vs_divider --
 *	Draw a dividing line between the screen and the output.
 */
static void
vs_divider(sp)
	SCR *sp;
{
	GS *gp;
	size_t len;

#define	DIVIDESTR	"+=+=+=+=+=+=+=+"
	len =
	    sizeof(DIVIDESTR) - 1 > sp->cols ? sp->cols : sizeof(DIVIDESTR) - 1;
	gp = sp->gp;
	(void)gp->scr_attr(sp, SA_INVERSE, 1);
	(void)gp->scr_addstr(sp, DIVIDESTR, len);
	(void)gp->scr_attr(sp, SA_INVERSE, 0);
}

/*
 * vs_msgsave --
 *	Save a message for later display.
 */
static void
vs_msgsave(sp, mt, p, len)
	SCR *sp;
	mtype_t mt;
	char *p;
	size_t len;
{
	GS *gp;
	MSG *mp_c, *mp_n;

	/*
	 * We have to handle messages before we have any place to put them.
	 * If there's no screen support yet, allocate a msg structure, copy
	 * in the message, and queue it on the global structure.  If we can't
	 * allocate memory here, we're genuinely screwed, dump the message
	 * to stderr in the (probably) vain hope that someone will see it.
	 */
	CALLOC_GOTO(sp, mp_n, MSG *, 1, sizeof(MSG));
	MALLOC_GOTO(sp, mp_n->buf, char *, len);

	memmove(mp_n->buf, p, len);
	mp_n->len = len;
	mp_n->mtype = mt;

	gp = sp->gp;
	if ((mp_c = gp->msgq.lh_first) == NULL) {
		LIST_INSERT_HEAD(&gp->msgq, mp_n, q);
	} else {
		for (; mp_c->q.le_next != NULL; mp_c = mp_c->q.le_next);
		LIST_INSERT_AFTER(mp_c, mp_n, q);
	}
	return;

alloc_err:
	if (mp_n != NULL)
		free(mp_n);
	(void)fprintf(stderr, "%.*s\n", (int)len, p);
}
