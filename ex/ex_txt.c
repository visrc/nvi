/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_txt.c,v 10.2 1995/06/09 13:43:19 bostic Exp $ (Berkeley) $Date: 1995/06/09 13:43:19 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "vi.h"

/*
 * !!!
 * The backslash characters was special when it preceded a newline as part of
 * a substitution replacement pattern.  For example, the input ":a\<cr>" would
 * failed immediately with an error, as the <cr> wasn't part of a substitution
 * replacement pattern.  This implies a frightening integration of the editor
 * and the parser and/or the RE engine.  There's no way I'm going to reproduce
 * those semantics.
 *
 * So, if backslashes are special, this code inserts the backslash and the next
 * character into the string, without regard for the character or the command
 * being entered.  Since "\<cr>" was illegal historically (except for the one
 * special case), and the command will fail eventually, no historical scripts
 * should break (presuming they didn't depend on the failure mode itself or the
 * characters remaining when failure occurred.
 */

static int	txt_dent __P((SCR *, TEXT *));
static void	txt_prompt __P((SCR *, TEXT *));

/*
 * ex_txt_setup --
 *	Set up for ex text input.
 *
 * PUBLIC: int ex_txt_setup __P((SCR *, ARG_CHAR_T));
 */
int
ex_txt_setup(sp, prompt)
	SCR *sp;
	ARG_CHAR_T prompt;
{
	GS *gp;
	EX_PRIVATE *exp;
	TEXT *tp;
	TEXTH *tiqh;

	gp = sp->gp;
	exp = EXP(sp);

	/*
	 * Get a TEXT structure with some initial buffer space, reusing the
	 * last one if it's big enough.  (All TEXT bookkeeping fields default
	 * to 0 -- text_init() handles this.)
	 */
	tiqh = &exp->im_tiq;
	if (tiqh->cqh_first != (void *)tiqh) {
		tp = tiqh->cqh_first;
		if (tp->q.cqe_next != (void *)tiqh || tp->lb_len < 32) {
			text_lfree(tiqh);
			goto newtp;
		}
		tp->len = 0;
	} else {
newtp:		if ((tp = text_init(sp, NULL, 0, 32)) == NULL)
			return (1);
		CIRCLEQ_INSERT_HEAD(tiqh, tp, q);
	}
	exp->im_tp = tp;

	/* Set the starting line number. */
	tp->lno = sp->lno + 1;

	/*
	 * If it's a terminal, set up autoindent, put out the prompt, and
	 * set it up so we know we were suspended.  Otherwise, turn off
	 * the autoindent flag, as that requires less special casing below.
	 *
	 * XXX
	 * Historic practice is that ^Z suspended command mode (but, because
	 * it ran in cooked mode, it was unaffected by the autowrite option.)
	 * On restart, any "current" input was discarded, whether in insert
	 * mode or not, and ex was in command mode.  This code matches historic
	 * practice, but not 'cause it's easier.
	 */
	exp->im_prompt = prompt;
	if (F_ISSET(sp->gp, G_STDIN_TTY)) {
		if (FL_ISSET(exp->im_flags, TXT_AUTOINDENT)) {
			FL_SET(exp->im_flags, TXT_EOFCHAR);
			if (v_txt_auto(sp, sp->lno, NULL, 0, tp))
				return (1);
		}
		txt_prompt(sp, tp);
	} else
		FL_CLR(exp->im_flags, TXT_AUTOINDENT);

	/* Other setup. */
	exp->im_carat = C_NOTSET;

	/* Set up default teardown state, move to text input loop. */
	exp->run_func = ex_txt_ev;
	gp->cm_state = ES_RUNNING;
	gp->cm_next = ES_CTEXT_TEARDOWN;
	return (0);
}

/*
 * ex_txt_ev --
 *	Handle text input events.
 *
 * PUBLIC: int ex_txt_ev __P((SCR *, EVENT *, int *));
 */
int
ex_txt_ev(sp, evp, completep)
	SCR *sp;
	EVENT *evp;
	int *completep;
{
	EX_PRIVATE *exp;
	TEXT *ntp, *tp;
	size_t cnt;

	exp = EXP(sp);
	tp = exp->im_tp;

	/*
	 * Check to see if the character fits into the input buffer.  (Use
	 * tp->len, ignore overwrite characters.)
	 */
	BINC_GOTO(sp, tp->lb, tp->lb_len, tp->len + 1);

	/*
	 * Handle SIGINT, SIGCONT events by discarding any partially
	 * entered text and returning successful.
	 */
	if (evp->e_event != E_CHARACTER)
		goto notlast;

	switch (evp->e_value) {
	case K_CR:
		/*
		 * !!!
		 * Historically, <carriage-return>'s in the command weren't
		 * special, so the ex parser would return an unknown command
		 * error message.  However, if they terminated the command if
		 * they were in a map.  I'm pretty sure this still isn't right,
		 * but it handles what I've seen so far.
		 */
		if (!F_ISSET(&evp->e_ch, CH_MAPPED))
			goto ins_ch;
		/* FALLTHROUGH */
	case K_NL:
		/*
		 * '\' can escape <carriage-return>/<newline>.  We toss the
		 * backslash.
		 */
		if (FL_ISSET(exp->im_flags, TXT_BACKSLASH) &&
		    tp->len != 0 && tp->lb[tp->len - 1] == '\\') {
			--tp->len;
			goto ins_ch;
		}

		/*
		 * CR returns from the ex command line.  Terminate with a nul,
		 * needed by filter.
		 */
		if (FL_ISSET(exp->im_flags, TXT_CR)) {
			tp->lb[tp->len] = '\0';
			goto done;
		}

		/* '.' may terminate text input mode; free the current TEXT. */
		if (FL_ISSET(exp->im_flags, TXT_DOTTERM) &&
		    tp->len == tp->ai + 1 && tp->lb[tp->len - 1] == '.') {
notlast:		CIRCLEQ_REMOVE(&exp->im_tiq, tp, q);
			text_free(tp);
			goto done;
		}

#ifdef __TK__
		/* Display any accumulated error messages. */
		if (sp->gp->scr_msgflush(sp, NULL, NULL))
			goto err;
#endif

		/* Set up bookkeeping for the new line. */
		if ((ntp = text_init(sp, NULL, 0, 32)) == NULL)
			goto err;
		ntp->lno = tp->lno + 1;

		/*
		 * Reset the autoindent line value.  0^D keeps the autoindent
		 * line from changing, ^D changes the level, even if there were
		 * no characters in the old line.  Note, if using the current
		 * tp structure, use the cursor as the length, the autoindent
		 * characters may have been erased.
		 */
		if (FL_ISSET(exp->im_flags, TXT_AUTOINDENT)) {
			if (exp->im_carat == C_NOCHANGE) {
				if (v_txt_auto(sp,
				    OOBLNO, &exp->im_ait, exp->im_ait.ai, ntp))
					goto err;
				free(exp->im_ait.lb);
			} else
				if (v_txt_auto(sp, OOBLNO, tp, tp->len, ntp))
					goto err;
			exp->im_carat = C_NOTSET;
		}
		txt_prompt(sp, ntp);

		/*
		 * Swap old and new TEXT's, and insert the new TEXT into the
		 * queue.
		 */
		exp->im_tp = tp = ntp;
		CIRCLEQ_INSERT_TAIL(&exp->im_tiq, tp, q);
		break;
	case K_CARAT:			/* Delete autoindent chars. */
		if (tp->len <= tp->ai &&
		    FL_ISSET(exp->im_flags, TXT_AUTOINDENT))
			exp->im_carat = C_CARATSET;
		goto ins_ch;
	case K_ZERO:			/* Delete autoindent chars. */
		if (tp->len <= tp->ai &&
		    FL_ISSET(exp->im_flags, TXT_AUTOINDENT))
			exp->im_carat = C_ZEROSET;
		goto ins_ch;
	case K_CNTRLD:			/* Delete autoindent char. */
		/*
		 * !!!
		 * Historically, the ^D command took (but then ignored) a
		 * count.  For simplicity, we don't return it unless it's
		 * the first character entered.  The check for len equal
		 * to 0 is okay, TXT_AUTOINDENT won't be set.
		 */
		if (FL_ISSET(exp->im_flags, TXT_CNTRLD)) {
			for (cnt = 0; cnt < tp->len; ++cnt)
				if (!isblank(tp->lb[cnt]))
					break;
			if (cnt == tp->len) {
				tp->len = 1;
				tp->lb[0] = evp->e_c;
				tp->lb[1] = '\0';

				/*
				 * Put out a line separator, in case
				 * the command fails.
				 */
				(void)putchar('\n');
				goto done;
			}
		}

		/*
		 * POSIX 1003.1b-1993, paragraph 7.1.1.9, states that the EOF
		 * characters are discarded if there are other characters to
		 * process in the line, i.e. if the EOF is not the first
		 * character in the line.  For this reason, historic ex
		 * discarded the EOF characters, even if they occurred in the
		 * middle of the input line.  We match that historic practice.
		 *
		 * !!!
		 * The test for discarding in the middle of the line is done
		 * in the switch, as the CARAT forms are N + 1, not N.
		 *
		 * !!!
		 * There's considerable magic to make the terminal code return
		 * the EOF character at all.  See cl/cl_main.c.
		 */
		if (!FL_ISSET(exp->im_flags, TXT_AUTOINDENT) || tp->len == 0)
			return (0);
		switch (exp->im_carat) {
		case C_CARATSET:		/* ^^D */
			if (tp->len > tp->ai + 1)
				return (0);

			/* Save the ai string for later. */
			exp->im_ait.lb = NULL;
			exp->im_ait.lb_len = 0;
			BINC_GOTO(sp,
			    exp->im_ait.lb, exp->im_ait.lb_len, tp->ai);
			memmove(exp->im_ait.lb, tp->lb, tp->ai);
			exp->im_ait.ai = exp->im_ait.len = tp->ai;

			exp->im_carat = C_NOCHANGE;
			goto leftmargin;
		case C_ZEROSET:			/* 0^D */
			if (tp->len > tp->ai + 1)
				return (0);

			exp->im_carat = C_NOTSET;
leftmargin:		sp->gp->scr_exadjust(sp, EX_TERM_CE);
			tp->ai = tp->len = 0;
			break;
		case C_NOTSET:			/* ^D */
			if (tp->len > tp->ai)
				return (0);

			if (txt_dent(sp, tp))
				goto err;
			break;
		default:
			abort();
		}

		/* Clear and redisplay the line. */
		sp->gp->scr_exadjust(sp, EX_TERM_CE);
		txt_prompt(sp, tp);
		break;
	default:
		/*
		 * See the TXT_BEAUTIFY comment in vi/v_txt_ev.c.
		 *
		 * Silently eliminate any iscntrl() character that wasn't
		 * already handled specially, except for <tab> and <ff>.
		 */
ins_ch:		if (FL_ISSET(exp->im_flags, TXT_BEAUTIFY) &&
		    iscntrl(evp->e_c) && evp->e_value != K_FORMFEED &&
		    evp->e_value != K_TAB)
			break;

		tp->lb[tp->len++] = evp->e_c;
		break;
	}

	*completep = 0;
	if (0)
done:		*completep = 1;
	return (0);

err:	
binc_err:
	return (1);
}

/*
 * txt_prompt --
 *	Display the ex prompt, line number, ai characters.  Characters had
 *	better be printable by the terminal driver, but that's its problem,
 *	not ours.
 */
static void
txt_prompt(sp, tp)
	SCR *sp;
	TEXT *tp;
{
	EX_PRIVATE *exp;

	exp = EXP(sp);

	/* Display the prompt. */
	if (exp->im_flags, FL_ISSET(exp->im_flags, TXT_PROMPT))
		(void)printf("%c", exp->im_prompt);

	/* Display the line number. */
	if (FL_ISSET(exp->im_flags, TXT_NUMBER) && O_ISSET(sp, O_NUMBER))
		(void)printf("%6lu  ", (u_long)tp->lno);

	/* Print out autoindent string. */
	if (FL_ISSET(exp->im_flags, TXT_AUTOINDENT))
		(void)printf("%.*s", (int)tp->ai, tp->lb);
	(void)fflush(stdout);
}

/*
 * txt_dent --
 *	Handle ^D outdents.
 *
 * Ex version of vi/v_ntext.c:txt_dent().  See that code for the (usual)
 * ranting and raving.  This is a fair bit simpler as ^T isn't special.
 */
static int
txt_dent(sp, tp)
	SCR *sp;
	TEXT *tp;
{
	u_long sw, ts;
	size_t cno, off, scno, spaces, tabs;

	ts = O_VAL(sp, O_TABSTOP);
	sw = O_VAL(sp, O_SHIFTWIDTH);

	/* Get the current screen column. */
	for (off = scno = 0; off < tp->len; ++off)
		if (tp->lb[off] == '\t')
			scno += COL_OFF(scno, ts);
		else
			++scno;

	/* Get the previous shiftwidth column. */
	cno = scno;
	scno -= --scno % sw;

	/*
	 * Since we don't know what comes before the character(s) being
	 * deleted, we have to resolve the autoindent characters .  The
	 * example is a <tab>, which doesn't take up a full shiftwidth
	 * number of columns because it's preceded by <space>s.  This is
	 * easy to get if the user sets shiftwidth to a value less than
	 * tabstop, and then uses ^T to indent, and ^D to outdent.
	 *
	 * Count up spaces/tabs needed to get to the target.
	 */
	for (cno = 0, tabs = 0; cno + COL_OFF(cno, ts) <= scno; ++tabs)
		cno += COL_OFF(cno, ts);
	spaces = scno - cno;

	/* Make sure there's enough room. */
	BINC_RET(sp, tp->lb, tp->lb_len, tabs + spaces + 1);

	/* Adjust the final ai character count. */
	tp->ai = tabs + spaces;

	/* Enter the replacement characters. */
	for (tp->len = 0; tabs > 0; --tabs)
		tp->lb[tp->len++] = '\t';
	for (; spaces > 0; --spaces)
		tp->lb[tp->len++] = ' ';
	return (0);
}
