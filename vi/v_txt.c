/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_txt.c,v 10.7 1995/09/21 12:08:44 bostic Exp $ (Berkeley) $Date: 1995/09/21 12:08:44 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "vi.h"

static int	 txt_abbrev __P((SCR *, TEXT *, CHAR_T *, int, int *, int *));
static void	 txt_ai_resolve __P((SCR *, TEXT *));
static TEXT	*txt_backup __P((SCR *, TEXTH *, TEXT *, u_int32_t *));
static void	 txt_err __P((SCR *, TEXTH *));
static int	 txt_hex __P((SCR *, TEXT *));
static int	 txt_margin __P((SCR *, TEXT *, TEXT *, int *, u_int32_t));
static void	 txt_nomorech __P((SCR *));
static int	 txt_dent __P((SCR *, TEXT *, int));
static void	 txt_Rcleanup __P((SCR *,
		    TEXTH *, TEXT *, const char *, const size_t));
static int	 txt_resolve __P((SCR *, TEXTH *, u_int32_t));
static int	 txt_showmatch __P((SCR *));
static void	 txt_unmap __P((SCR *, TEXT *, u_int32_t *));

/* Cursor character (space is hard to track on the screen). */
#if defined(DEBUG) && 0
#undef	CH_CURSOR
#define	CH_CURSOR	'+'
#endif

/*
 * v_tcmd --
 *	Fill a buffer from the terminal for vi.
 *
 * PUBLIC: int v_tcmd __P((SCR *, VICMD *, ARG_CHAR_T, u_int));
 */
int
v_tcmd(sp, vp, prompt, flags)
	SCR *sp;
	VICMD *vp;
	ARG_CHAR_T prompt;
	u_int flags;
{
	SMAP *esmp;
	VI_PRIVATE *vip;
	recno_t sv_lno, sv_tm_lno;
	size_t cnt, sv_cno, sv_t_maxrows, sv_t_minrows, sv_t_rows, sv_tm_off;

	/* Save current cursor. */
	sv_lno = sp->lno;
	sv_cno = sp->cno;

	if (!IS_ONELINE(sp)) {
		/*
		 * Fake like the user is doing input on the last line of the
		 * screen.  This makes all of the scrolling work correctly,
		 * and allows us the use of the vi text editing routines, not
		 * to mention practically infinite length ex commands.
		 *
		 * Save the current location.
		 */
		sv_tm_lno = TMAP->lno;
		sv_tm_off = TMAP->off;
		sv_t_rows = sp->t_rows;
		sv_t_minrows = sp->t_minrows;
		sv_t_maxrows = sp->t_maxrows;

		/*
		 * If it's a small screen, TMAP may be small for the screen.
		 * Fix it, filling in fake lines as we go.
		 */
		if (IS_SMALL(sp))
			for (esmp =
			    HMAP + (sp->t_maxrows - 1); TMAP < esmp; ++TMAP) {
				TMAP[1].lno = TMAP[0].lno + 1;
				TMAP[1].off = 1;
			}

		/* Build the fake entry. */
		TMAP[1].lno = TMAP[0].lno + 1;
		TMAP[1].off = 1;
		SMAP_FLUSH(&TMAP[1]);
		++TMAP;

		/* Reset the screen information. */
		sp->t_rows = sp->t_minrows = ++sp->t_maxrows;
	}

	/* Move to the last line. */
	sp->lno = TMAP[0].lno;
	sp->cno = 0;

	/* Don't update the modeline for now. */
	F_SET(sp, S_INPUT_INFO);

	LF_SET(TXT_APPENDEOL | TXT_CR | TXT_ESCAPE | TXT_INFOLINE);
	if (O_ISSET(sp, O_ALTWERASE))
		LF_SET(TXT_ALTWERASE);
	if (O_ISSET(sp, O_TTYWERASE))
		LF_SET(TXT_TTYWERASE);

	if (v_txt(sp, vp, NULL, NULL, 0, prompt, 0, flags))
		return (1);

	/* Reenable the modeline updates. */
	F_CLR(sp, S_INPUT_INFO);

	if (!IS_ONELINE(sp)) {
		/* Restore the screen information. */
		sp->t_rows = sv_t_rows;
		sp->t_minrows = sv_t_minrows;
		sp->t_maxrows = sv_t_maxrows;

		/*
		 * If it's a small screen, TMAP may be wrong.  Clear any
		 * lines that might have been overwritten.
		 */
		if (IS_SMALL(sp)) {
			for (cnt = sp->t_rows; cnt <= sp->t_maxrows; ++cnt) {
				(void)sp->gp->scr_move(sp, cnt, 0);
				(void)sp->gp->scr_clrtoeol(sp);
			}
			TMAP = HMAP + (sp->t_rows - 1);
		} else
			--TMAP;

		/*
		 * The map may be wrong if the user entered more than one
		 * (logical) line.  Fix it.  If the user entered a whole
		 * screen, this will be slow, but we probably don't care.
		 */
		while (sv_tm_lno != TMAP->lno || sv_tm_off != TMAP->off)
			if (vs_sm_1down(sp))
				return (1);
	} else
		F_SET(sp, S_SCR_REDRAW);

	/*
	 * Invalidate the cursor and the line size cache, the line never
	 * really existed.  This fixes bugs where the user searches for
	 * the last line on the screen + 1 and the refresh routine thinks
	 * that's where we just were.
	 */
	vip = VIP(sp);
	VI_SCR_CFLUSH(vip);
	F_SET(vip, VIP_CUR_INVALID);

	/* Restore the original cursor. */
	sp->lno = sv_lno;
	sp->cno = sv_cno;
	return (0);
}

/*
 * If doing input mapping on the colon command line, may need to unmap
 * based on the command.
 */
#define	UNMAP_TST							\
	FL_ISSET(ec_flags, EC_MAPINPUT) && LF_ISSET(TXT_INFOLINE)

/*
 * v_txt --
 *	Vi text input.
 *
 * !!!
 * Historic vi did a special screen optimization for tab characters.  For
 * the keystrokes "iabcd<esc>0C<tab>", the tab would overwrite the rest of
 * the string when it was displayed.  Because this implementation redisplays
 * the entire line on each keystroke, the "bcd" gets pushed to the right as
 * we ignore that the user has "promised" to change the rest of the characters.
 * Users have noticed, but this isn't worth fixing, and, the way that the
 * historic vi did it results in an even worse bug.  Given the keystrokes
 * "iabcd<esc>0R<tab><esc>", the "bcd" disappears, and magically reappears
 * on the second <esc> key.
 *
 * PUBLIC: int v_txt __P((SCR *, VICMD *,
 * PUBLIC:    MARK *, const char *, size_t, ARG_CHAR_T, recno_t, u_int32_t));
 */
int
v_txt(sp, vp, tm, lp, len, prompt, ai_line, flags)
	SCR *sp;
	VICMD *vp;
	MARK *tm;		/* To MARK. */
	const char *lp;		/* Input line. */
	size_t len;		/* Input line length. */
	ARG_CHAR_T prompt;	/* Prompt to display. */
	recno_t ai_line;	/* Line number to use for autoindent count. */
	u_int32_t flags;	/* TXT_* flags. */
{
	EVENT ev, *evp;		/* Current event. */
	GS *gp;
	TEXT *ntp, *tp;		/* Input text structures. */
	TEXT ait;		/* Autoindent text structure. */
	TEXT wmt;		/* Wrapmargin text structure. */
	TEXTH *tiqh;
	VI_PRIVATE *vip;
	abb_t abb;		/* State of abbreviation checks. */
	carat_t carat;		/* State of the "[^0]^D" sequences. */
	quote_t quote;		/* State of quotation. */
	size_t owrite, insert;	/* Temporary copies of TEXT fields. */
	size_t margin;		/* Wrapmargin value. */
	size_t rcol;		/* 0-N: insert offset in the replay buffer. */
	size_t tcol;		/* Temporary column. */
	u_int32_t ec_flags;	/* Input mapping flags. */
	int abcnt, ab_turnoff;	/* Abbreviation character count, switch. */
	int hexcnt;		/* Hex character count. */
	int showmatch;		/* Showmatch set on this character. */
	int rcount;		/* Replay count. */
	int wm_set, wm_skip;	/* Wrapmargin happened, blank skip flags. */
	int max, tmp;
	char *p;

	gp = sp->gp;
	vip = VIP(sp);

	/*
	 * Set the input flag, so tabs get displayed correctly
	 * and everyone knows that the text buffer is in use.
	 */
	F_SET(sp, S_INPUT);

	/*
	 * Get one TEXT structure with some initial buffer space, reusing
	 * the last one if it's big enough.  (All TEXT bookkeeping fields
	 * default to 0 -- text_init() handles this.)  If changing a line,
	 * copy it into the TEXT buffer.
	 */
	tiqh = &sp->tiq;
	if (tiqh->cqh_first != (void *)tiqh) {
		tp = tiqh->cqh_first;
		if (tp->q.cqe_next != (void *)tiqh || tp->lb_len < len + 32) {
			text_lfree(tiqh);
			goto newtp;
		}
		tp->ai = tp->insert = tp->offset = tp->owrite = 0;
		if (lp != NULL) {
			tp->len = len;
			memmove(tp->lb, lp, len);
		} else
			tp->len = 0;
	} else {
newtp:		if ((tp = text_init(sp, lp, len, len + 32)) == NULL)
			return (1);
		CIRCLEQ_INSERT_HEAD(tiqh, tp, q);
	}

	/* Set default termination condition. */
	tp->term = TERM_OK;

	/* Set the starting line number. */
	tp->lno = sp->lno;

	/*
	 * Set the insert and overwrite counts.  If overwriting characters,
	 * do insertion afterward.  If not overwriting characters, assume
	 * doing insertion.  If change is to a mark, emphasize it with an
	 * CH_ENDMARK character.
	 */
	if (len) {
		if (LF_ISSET(TXT_OVERWRITE)) {
			tp->owrite = (tm->cno - sp->cno) + 1;
			tp->insert = (len - tm->cno) - 1;
		} else
			tp->insert = len - sp->cno;

		if (LF_ISSET(TXT_EMARK))
			tp->lb[tm->cno] = CH_ENDMARK;
	}

	/*
	 * Many of the special cases in text input are to handle autoindent
	 * support.  Somebody decided that it would be a good idea if "^^D"
	 * and "0^D" deleted all of the autoindented characters.  In an editor
	 * that takes single character input from the user, this beggars the
	 * imagination.  Note also, "^^D" resets the next lines' autoindent,
	 * but "0^D" doesn't.
	 *
	 * We assume that autoindent only happens on empty lines, so insert
	 * and overwrite will be zero.  If doing autoindent, figure out how
	 * much indentation we need and fill it in.  Update input column and
	 * screen cursor as necessary.
	 */
	if (LF_ISSET(TXT_AUTOINDENT) && ai_line != OOBLNO) {
		if (v_txt_auto(sp, ai_line, NULL, 0, tp))
			return (1);
		sp->cno = tp->ai;
	} else {
		/*
		 * The cc and S commands have a special feature -- leading
		 * <blank> characters are handled as autoindent characters.
		 * Beauty!
		 */
		if (LF_ISSET(TXT_AICHARS)) {
			tp->offset = 0;
			tp->ai = sp->cno;
		} else
			tp->offset = sp->cno;
	}

	/* If getting a command buffer from the user, there may be a prompt. */
	if (LF_ISSET(TXT_PROMPT)) {
		tp->lb[sp->cno++] = prompt;
		++tp->len;
		++tp->offset;
	}

	/*
	 * If appending after the end-of-line, add a space into the buffer
	 * and move the cursor right.  This space is inserted, i.e. pushed
	 * along, and then deleted when the line is resolved.  Assumes that
	 * the cursor is already positioned at the end of the line.  This
	 * avoids the nastiness of having the cursor reside on a magical
	 * column, i.e. a column that doesn't really exist.  The only down
	 * side is that we may wrap lines or scroll the screen before it's
	 * strictly necessary.  Not a big deal.
	 */
	if (LF_ISSET(TXT_APPENDEOL)) {
		tp->lb[sp->cno] = CH_CURSOR;
		++tp->len;
		++tp->insert;
	}

	/*
	 * Historic practice is that the wrapmargin value was a distance
	 * from the RIGHT-HAND margin, not the left.  It's more useful to
	 * us as a distance from the left-hand margin, i.e. the same as
	 * the wraplen value.  The wrapmargin option is historic practice.
	 * Nvi added the wraplen option so that it would be possible to
	 * edit files with consistent margins without knowing the number of
	 * columns in the window.
	 *
	 * XXX
	 * Setting margin causes a significant performance hit.  Normally
	 * we don't update the screen if there are keys waiting, but we
	 * have to if margin is set, otherwise the screen routines don't
	 * know where the cursor is.
	 *
	 * !!!
	 * Abbreviated keys were affected by the wrapmargin option in the
	 * historic 4BSD vi.  Mapped keys were usually, but sometimes not.
	 * See the comment in vi/v_text():set_txt_std for more information.
	 *
	 * !!!
	 * One more special case.  If an inserted <blank> character causes
	 * wrapmargin to split the line, the next user entered character is
	 * discarded if it's a <space> character.
	 */
	wm_set = wm_skip = 0;
	if (LF_ISSET(TXT_WRAPMARGIN))
		if ((margin = O_VAL(sp, O_WRAPMARGIN)) != 0)
			margin = sp->cols - margin;
		else
			margin = O_VAL(sp, O_WRAPLEN);
	else
		margin = 0;

	/* Initialize abbreviation checks. */
	abcnt = ab_turnoff = 0;
	abb = F_ISSET(gp, G_ABBREV) &&
	    LF_ISSET(TXT_MAPINPUT) ? AB_INWORD : AB_NOTSET;

	/*
	 * Set up the dot command.  Dot commands are done by saving the actual
	 * characters and then reevaluating them so that things like wrapmargin
	 * can change between the insert and the replay.
	 *
	 * !!!
	 * Historically, vi did not remap or reabbreviate replayed input.  (It
	 * did beep at you if you changed an abbreviation and then replayed the
	 * input.  We're not that compatible.)  We don't have to do anything to
	 * avoid remapping, as we're not getting characters from the terminal
	 * routines.  Turn the abbreviation check off.
	 *
	 * XXX
	 * It would be nice if we could swallow backspaces and such, but it's
	 * not all that easy to do.
	 */
	rcol = 0;
	if (LF_ISSET(TXT_REPLAY)) {
		abb = AB_NOTSET;
		LF_CLR(TXT_RECORD);
	}

	/* Other text input mode setup. */
	hexcnt = 0;
	quote = Q_NOTSET;
	carat = C_NOTSET;

	/* Initialize replay count, showmatch flag. */
	rcount = showmatch = 0;

	/* Initialize input flags. */
	ec_flags = LF_ISSET(TXT_MAPINPUT) ? EC_MAPINPUT : 0;

	/* Refresh the screen. */
	if (vs_refresh(sp))
		return (1);

	/* If it's dot, just do it now. */
	if (F_ISSET(vp, VC_ISDOT))
		goto replay;

	/* Get an event. */
	evp = &ev;
next:	if (v_get_event(sp, evp, ec_flags))
		return (1);

	/* Deal with all non-character events. */
	switch (evp->e_event) {
	case E_CHARACTER:
		break;
	case E_ERR:
	case E_EOF:
		F_SET(sp, S_EXIT_FORCE);
		return (1);
	case E_INTERRUPT:
		/*
		 * !!!
		 * Historically, <interrupt> exited the user from text input
		 * mode or cancelled a colon command, and returned to command
		 * mode.  It also beeped the terminal, but that seems a bit
		 * excessive.  We also get resize events here, which mean that
		 * our text structures may no longer be correct.
		 */
		goto k_escape;
	case E_REPAINT:
		if (vs_repaint(sp, &ev))
			return (1);
		goto next;
	case E_RESIZE:
		(void)v_event_push(sp, &ev, NULL, 1, 0);
		goto k_escape;
	default:
		abort();
	}

	/*
	 * !!!
	 * If the first character of the input is a nul, replay the previous
	 * input.  Note, it was okay to replay non-existent input.  This was
	 * not documented as far as I know, and is a great test of vi clones.
	 */
	if (LF_ISSET(TXT_REPLAY)) {
		LF_CLR(TXT_REPLAY);
		if (evp->e_c == '\0') {
			if (vip->rep == NULL)
				goto done;

			rcol = 0;
			abb = AB_NOTSET;
			LF_CLR(TXT_RECORD);
			LF_SET(TXT_REPLAY);
			goto replay;
		}
	}

	/* Abbreviation check.  See comment in txt_abbrev(). */
#define	MAX_ABBREVIATION_EXPANSION	256
	if (F_ISSET(&evp->e_ch, CH_ABBREVIATED)) {
		if (++abcnt > MAX_ABBREVIATION_EXPANSION) {
			if (v_event_flush(sp, CH_ABBREVIATED))
				msgq(sp, M_ERR,
"191|Abbreviation exceeded expansion limit: characters discarded");
			abcnt = 0;
			goto ret;
		}
	} else
		abcnt = 0;

	/* Check to see if the character fits into the replay buffers. */
	if (LF_ISSET(TXT_RECORD)) {
		BINC_GOTO(sp, vip->rep,
		    vip->rep_len, (rcol + 1) * sizeof(EVENT));
		vip->rep[rcol++] = *evp;
	}

replay:	if (LF_ISSET(TXT_REPLAY))
		evp = vip->rep + rcol++;

	/* Wrapmargin check for leading space. */
	if (wm_skip) {
		wm_skip = 0;
		if (evp->e_c == ' ')
			goto ret;
	}

	/*
	 * Check to see if the character fits into the input buffer.  (Use
	 * tp->len, ignore overwrite characters.)
	 */
	BINC_GOTO(sp, tp->lb, tp->lb_len, tp->len + 1);

	/*
	 * If quoted by someone else, simply insert the character.
	 *
	 * !!!
	 * If this character was quoted by a K_VLNEXT or a backslash, replace
	 * the placeholder (a carat or a backslash) with the new character.
	 * Skip tests for abbreviations; ":ab xa XA" followed by "ixa^V<space>"
	 * doesn't perform an abbreviation.  Special case, ^V^J is the same as
	 * ^J, historically.
	 */
	if (F_ISSET(&evp->e_ch, CH_QUOTED))
		goto insq_ch;
	if (quote != Q_NOTSET) {
		if (quote == Q_VTHIS && evp->e_value != K_NL ||
		    quote == Q_BTHIS &&
		    (evp->e_value == K_VERASE || evp->e_value == K_VKILL)) {
			--sp->cno;
			++tp->owrite;
			FL_CLR(ec_flags, EC_QUOTED);
			quote = Q_NOTSET;
			goto insl_ch;
		}
		if (quote == Q_VTHIS && evp->e_value == K_NL)
			--sp->cno;
		FL_CLR(ec_flags, EC_QUOTED);
		quote = Q_NOTSET;
	}

	/*
	 * !!!
	 * Translate "<CH_HEX>[isxdigit()]*" to a character with a hex value:
	 * this test delimits the value by any non-hex character.  Offset by
	 * one, we use 0 to mean that we've found <CH_HEX>.
	 */
	if (hexcnt > 1 && !isxdigit(evp->e_c)) {
		hexcnt = 0;
		if (txt_hex(sp, tp))
			goto err;
	}

	switch (evp->e_value) {
	case K_CR:				/* Carriage return. */
	case K_NL:				/* New line. */
		/* Return in script windows and the command line. */
k_cr:		if (LF_ISSET(TXT_CR)) {
			/*
			 * If this was a map, we may have not displayed
			 * the line.  Display it, just in case.
			 *
			 * If a script window and not the colon line,
			 * push a <cr> so it gets executed.
			 */
			if (LF_ISSET(TXT_INFOLINE)) {
				if (vs_change(sp, tp->lno, LINE_RESET))
					goto err;
			} else if (F_ISSET(sp, S_SCRIPT))
				(void)v_event_push(sp, NULL, "\r", 1, CH_NOMAP);

			/* If empty, set termination condition. */
			if (sp->cno <= tp->offset)
				tp->term = TERM_CR;
			goto k_escape;
		}

#define	LINE_RESOLVE {							\
		/*							\
		 * Handle abbreviations.  If there was one, discard the	\
		 * replay characters.					\
		 */							\
		if (abb == AB_INWORD &&					\
		    !LF_ISSET(TXT_REPLAY) && F_ISSET(gp, G_ABBREV)) {	\
			if (txt_abbrev(sp, tp, &evp->e_c,		\
			    LF_ISSET(TXT_INFOLINE), &tmp,		\
			    &ab_turnoff))				\
				goto err;				\
			if (tmp) {					\
				if (LF_ISSET(TXT_RECORD))		\
					rcol -= tmp + 1;		\
				goto ret;				\
			}						\
		}							\
		if (abb != AB_NOTSET)					\
			abb = AB_NOTWORD;				\
		if (UNMAP_TST)						\
			txt_unmap(sp, tp, &ec_flags);			\
		/* Delete any appended cursor. */			\
		if (LF_ISSET(TXT_APPENDEOL)) {				\
			--tp->len;					\
			--tp->insert;					\
		}							\
}
		LINE_RESOLVE;

		/*
		 * Save the current line information for restoration in
		 * txt_backup().  Set the new line length.
		 */
		tp->sv_len = tp->len;
		tp->sv_cno = sp->cno;
		tp->len = sp->cno;

		/* Update the old line. */
		if (vs_change(sp, tp->lno, LINE_RESET))
			goto err;

		/*
		 * Historic practice was to delete <blank> characters following
		 * the inserted newline.  This affected the 'R', 'c', and 's'
		 * commands; 'c' and 's' retained the insert characters only,
		 * 'R' moved overwrite and insert characters into the next TEXT
		 * structure.  All other commands simply deleted the overwrite
		 * characters.  We keep track of the number of characters erased
		 * for the 'R' command so that we can get the final resolution
		 * of the line correct.
		 */
		tp->R_erase = 0;
		owrite = tp->owrite;
		insert = tp->insert;
		if (LF_ISSET(TXT_REPLACE) && owrite != 0) {
			for (p = tp->lb + sp->cno; owrite > 0 && isblank(*p);
			    ++p, --owrite, ++tp->R_erase);
			if (owrite == 0)
				for (; insert > 0 && isblank(*p);
				    ++p, ++tp->R_erase, --insert);
		} else {
			for (p = tp->lb + sp->cno + owrite;
			    insert > 0 && isblank(*p); ++p, --insert);
			owrite = 0;
		}

		/* Set up bookkeeping for the new line. */
		if ((ntp = text_init(sp, p,
		    insert + owrite, insert + owrite + 32)) == NULL)
			goto err;
		ntp->insert = insert;
		ntp->owrite = owrite;
		ntp->lno = tp->lno + 1;

		/*
		 * Reset the autoindent line value.  0^D keeps the autoindent
		 * line from changing, ^D changes the level, even if there were
		 * no characters in the old line.  Note, if using the current
		 * tp structure, use the cursor as the length, the autoindent
		 * characters may have been erased.
		 */
		if (LF_ISSET(TXT_AUTOINDENT)) {
			if (carat == C_NOCHANGE) {
				if (v_txt_auto(sp, OOBLNO, &ait, ait.ai, ntp))
					goto err;
				FREE_SPACE(sp, ait.lb, ait.lb_len);
			} else
				if (v_txt_auto(sp, OOBLNO, tp, sp->cno, ntp))
					goto err;
			carat = C_NOTSET;
		}

		/* Reset the cursor. */
		sp->lno = ntp->lno;
		sp->cno = ntp->ai;

		/*
		 * If we're here because wrapmargin was set and we've broken a
		 * line, there may be additional information (i.e. the start of
		 * a line) in the wmt structure.
		 */
		if (wm_set) {
			if (wmt.offset != 0 ||
			    wmt.owrite != 0 || wmt.insert != 0) {
#define	WMTSPACE	wmt.offset + wmt.owrite + wmt.insert
				BINC_GOTO(sp, ntp->lb,
				    ntp->lb_len, ntp->len + WMTSPACE + 32);
				memmove(ntp->lb + sp->cno, wmt.lb, WMTSPACE);
				ntp->len += WMTSPACE;
				sp->cno += wmt.offset;
				ntp->owrite = wmt.owrite;
				ntp->insert = wmt.insert;
			}
			wm_set = 0;
		}

		/* New lines are TXT_APPENDEOL. */
		if (ntp->owrite == 0 && ntp->insert == 0) {
			BINC_GOTO(sp, ntp->lb, ntp->lb_len, ntp->len + 1);
			LF_SET(TXT_APPENDEOL);
			ntp->lb[sp->cno] = CH_CURSOR;
			++ntp->insert;
			++ntp->len;
		}

		/*
		 * Swap old and new TEXT's, and insert the new TEXT into the
		 * queue.
		 *
		 * !!!
		 * DON'T insert until the old line has been updated, or the
		 * inserted line count in line.c:file_gline() will be wrong.
		 */
		tp = ntp;
		CIRCLEQ_INSERT_TAIL(&sp->tiq, tp, q);

		/* Update the new line. */
		if (vs_change(sp, tp->lno, LINE_INSERT))
			goto err;

		goto ret;
	case K_ESCAPE:				/* Escape. */
		if (!LF_ISSET(TXT_ESCAPE))
			goto ins_ch;

		/* If we have a count, start replaying the input. */
		if (F_ISSET(vp, VC_C1SET) && ++rcount != vp->count) {
			rcol = 0;
			abb = AB_NOTSET;
			LF_CLR(TXT_RECORD);
			LF_SET(TXT_REPLAY);

			/*
			 * A few commands (e.g. 'o') need a <newline> for
			 * each repetition.
			 */
			if (LF_ISSET(TXT_ADDNEWLINE))
				goto k_cr;
			goto replay;
		}

		/* If empty, set termination condition. */
		if (sp->cno <= tp->offset)
			tp->term = TERM_ESC;
		
k_escape:	LINE_RESOLVE;

		/*
		 * Clean up for the 'R' command, restoring overwrite
		 * characters, and making them into insert characters.
		 */
		if (LF_ISSET(TXT_REPLACE))
			txt_Rcleanup(sp, &sp->tiq, tp, lp, len);

		/*
		 * If there are any overwrite characters, copy down
		 * any insert characters, and decrement the length.
		 */
		if (tp->owrite) {
			if (tp->insert)
				memmove(tp->lb + sp->cno,
				    tp->lb + sp->cno + tp->owrite, tp->insert);
			tp->len -= tp->owrite;
		}

		/*
		 * Optionally resolve the lines into the file.  If not
		 * resolving the lines into the file, end the line with
		 * a nul.  If the line is empty, then set the length to
		 * 0, the termination condition has already been set.
		 *
		 * XXX
		 * This is wrong, should pass back a length.
		 */
		if (LF_ISSET(TXT_RESOLVE)) {
			if (txt_resolve(sp, &sp->tiq, flags))
				goto err;
		} else {
			BINC_GOTO(sp, tp->lb, tp->lb_len, tp->len + 1);
			tp->lb[tp->len] = '\0';
		}

		/*
		 * Set the return cursor position to rest on the last
		 * inserted character.
		 */
		sp->lno = tp->lno;
		if (sp->cno != 0)
			--sp->cno;

		/* Update the last line. */
		if (vs_change(sp, sp->lno, LINE_RESET))
			return (1);
		goto done;
	case K_CARAT:			/* Delete autoindent chars. */
		if (sp->cno <= tp->ai && LF_ISSET(TXT_AUTOINDENT))
			carat = C_CARATSET;
		goto ins_ch;
	case K_ZERO:			/* Delete autoindent chars. */
		if (sp->cno <= tp->ai && LF_ISSET(TXT_AUTOINDENT))
			carat = C_ZEROSET;
		goto ins_ch;
	case K_CNTRLD:			/* Delete autoindent char. */
		/*
		 * If in the first column or no characters to erase,
		 * ignore the ^D (this matches historic practice).  If
		 * not doing autoindent or already inserted non-ai
		 * characters, it's a literal.  The latter test is done
		 * in the switch, as the CARAT forms are N + 1, not N.
		 */
		if (!LF_ISSET(TXT_AUTOINDENT))
			goto ins_ch;
		if (sp->cno == 0)
			goto ret;

		switch (carat) {
		case C_CARATSET:	/* ^^D */
			if (sp->cno > tp->ai + tp->offset + 1)
				goto ins_ch;

			/* Save the ai string for later. */
			ait.lb = NULL;
			ait.lb_len = 0;
			BINC_GOTO(sp, ait.lb, ait.lb_len, tp->ai);
			memmove(ait.lb, tp->lb, tp->ai);
			ait.ai = ait.len = tp->ai;

			carat = C_NOCHANGE;
			goto leftmargin;
		case C_ZEROSET:		/* 0^D */
			if (sp->cno > tp->ai + tp->offset + 1)
				goto ins_ch;

			carat = C_NOTSET;
leftmargin:		tp->lb[sp->cno - 1] = ' ';
			tp->owrite += sp->cno - tp->offset;
			tp->ai = 0;
			sp->cno = tp->offset;
			break;
		case C_NOTSET:		/* ^D */
			if (sp->cno > tp->ai + tp->offset)
				goto ins_ch;

			(void)txt_dent(sp, tp, 0);
			break;
		default:
			abort();
		}
		break;
	case K_VERASE:			/* Erase the last character. */
		/* If can erase over the prompt, return. */
		if (sp->cno <= tp->offset && LF_ISSET(TXT_BS)) {
			tp->term = TERM_BS;
			goto done;
		}

		/*
		 * If at the beginning of the line, try and drop back to a
		 * previously inserted line.
		 */
		if (sp->cno == 0) {
			if ((ntp =
			    txt_backup(sp, &sp->tiq, tp, &flags)) == NULL)
				goto err;
			tp = ntp;
			break;
		}

		/* If nothing to erase, bell the user. */
		if (sp->cno <= tp->offset) {
			txt_nomorech(sp);
			break;
		}

		/* Drop back one character. */
		--sp->cno;

		/*
		 * Increment overwrite, decrement ai if deleted.
		 *
		 * !!!
		 * Historic vi did not permit users to use erase characters
		 * to delete autoindent characters.  We do.  Eat hot death,
		 * POSIX.
		 */
		++tp->owrite;
		if (sp->cno < tp->ai)
			--tp->ai;
		break;
	case K_VWERASE:			/* Skip back one word. */
		/*
		 * If at the beginning of the line, try and drop back to a
		 * previously inserted line.
		 */
		if (sp->cno == 0) {
			if ((ntp =
			    txt_backup(sp, &sp->tiq, tp, &flags)) == NULL)
				goto err;
			tp = ntp;
		}

		/*
		 * If at offset, nothing to erase so bell the user.
		 */
		if (sp->cno <= tp->offset) {
			txt_nomorech(sp);
			break;
		}

		/*
		 * The first werase goes back to any autoindent column and the
		 * second werase goes back to the offset.
		 *
		 * !!!
		 * Historic vi did not permit users to use erase characters to
		 * delete autoindent characters.
		 */
		if (tp->ai && sp->cno > tp->ai)
			max = tp->ai;
		else {
			tp->ai = 0;
			max = tp->offset;
		}

		/* Skip over trailing space characters. */
		while (sp->cno > max && isblank(tp->lb[sp->cno - 1])) {
			--sp->cno;
			++tp->owrite;
		}
		if (sp->cno == max)
			break;
		/*
		 * There are three types of word erase found on UNIX systems.
		 * They can be identified by how the string /a/b/c is treated
		 * -- as 1, 3, or 6 words.  Historic vi had two classes of
		 * characters, and strings were delimited by them and
		 * <blank>'s, so, 6 words.  The historic tty interface used
		 * <blank>'s to delimit strings, so, 1 word.  The algorithm
		 * offered in the 4.4BSD tty interface (as stty altwerase)
		 * treats it as 3 words -- there are two classes of
		 * characters, and strings are delimited by them and
		 * <blank>'s.  The difference is that the type of the first
		 * erased character erased is ignored, which is exactly right
		 * when erasing pathname components.  The edit options
		 * TXT_ALTWERASE and TXT_TTYWERASE specify the 4.4BSD tty
		 * interface and the historic tty driver behavior,
		 * respectively, and the default is the same as the historic
		 * vi behavior.
		 */
		if (LF_ISSET(TXT_TTYWERASE))
			while (sp->cno > max) {
				--sp->cno;
				++tp->owrite;
				if (isblank(tp->lb[sp->cno - 1]))
					break;
			}
		else {
			if (LF_ISSET(TXT_ALTWERASE)) {
				--sp->cno;
				++tp->owrite;
				if (isblank(tp->lb[sp->cno - 1]))
					break;
			}
			if (sp->cno > max)
				tmp = inword(tp->lb[sp->cno - 1]);
			while (sp->cno > max) {
				--sp->cno;
				++tp->owrite;
				if (tmp != inword(tp->lb[sp->cno - 1])
				    || isblank(tp->lb[sp->cno - 1]))
					break;
			}
		}
		break;
	case K_VKILL:			/* Restart this line. */
		/*
		 * !!!
		 * If at the beginning of the line, try and drop back to a
		 * previously inserted line.  Historic vi did not permit
		 * users to go back to previous lines.
		 */
		if (sp->cno == 0) {
			if ((ntp =
			    txt_backup(sp, &sp->tiq, tp, &flags)) == NULL)
				goto err;
			tp = ntp;
		}

		/* If at offset, nothing to erase so bell the user. */
		if (sp->cno <= tp->offset) {
			txt_nomorech(sp);
			break;
		}

		/*
		 * First kill goes back to any autoindent and second kill goes
		 * back to the offset.
		 *
		 * !!!
		 * Historic vi did not permit users to use erase characters to
		 * delete autoindent characters.
		 */
		if (tp->ai && sp->cno > tp->ai)
			max = tp->ai;
		else {
			tp->ai = 0;
			max = tp->offset;
		}
		tp->owrite += sp->cno - max;
		sp->cno = max;
		break;
	case K_CNTRLT:			/* Add autoindent characters. */
		if (!LF_ISSET(TXT_CNTRLT))
			goto ins_ch;
		if (txt_dent(sp, tp, 1))
			goto err;
		goto ebuf_chk;
	case K_RIGHTBRACE:
	case K_RIGHTPAREN:
		if (LF_ISSET(TXT_SHOWMATCH))
			showmatch = 1;
		goto ins_ch;
	case K_BACKSLASH:		/* Quote next erase/kill. */
		/*
		 * !!!
		 * Historic vi tried to make abbreviations after a backslash
		 * escape work.  If you did ":ab x y", and inserted "x\^H",
		 * (assuming the erase character was ^H) you got "x^H", and
		 * no abbreviation was done.  If you inserted "x\z", however,
		 * it tried to back up and do the abbreviation, i.e. replace
		 * 'x' with 'y'.  The problem was it got it wrong, and you
		 * ended up with "zy\".
		 *
		 * This is really hard to do (you have to remember the
		 * word/non-word state, for example), and doesn't make any
		 * sense to me.  Both backslash and the characters it
		 * (usually) escapes will individually trigger the
		 * abbreviation, so I don't see why the combination of them
		 * wouldn't.  I don't expect to get caught on this one,
		 * particularly since it never worked right, but I've been
		 * wrong before.
		 *
		 * Do the tests for abbreviations, so ":ab xa XA",
		 * "ixa\<K_VERASE>" performs the abbreviation.
		 */
		quote = Q_BNEXT;
		goto insq_ch;
	case K_VLNEXT:			/* Quote next character. */
		evp->e_c = '^';
		quote = Q_VNEXT;
		FL_SET(ec_flags, EC_QUOTED);

		/*
		 * !!!
		 * Skip the tests for abbreviations, so ":ab xa XA",
		 * "ixa^V<space>" doesn't perform the abbreviation.
		 */
		goto insl_ch;
	case K_HEXCHAR:
		hexcnt = 1;
		goto insq_ch;
	default:			/* Insert the character. */
ins_ch:		/*
		 * Historically, vi eliminated nul's out of hand.  If the
		 * beautify option was set, it also deleted any unknown
		 * ASCII value less than space (040) and the del character
		 * (0177), except for tabs.  Unknown is a key word here.
		 * Most vi documentation claims that it deleted everything
		 * but <tab>, <nl> and <ff>, as that's what the original
		 * 4BSD documentation said.  This is obviously wrong,
		 * however, as <esc> would be included in that list.  What
		 * we do is eliminate any unquoted, iscntrl() character that
		 * wasn't a replay and wasn't handled specially, except
		 * <tab> or <ff>.
		 */
		if (LF_ISSET(TXT_BEAUTIFY) && iscntrl(evp->e_c) &&
		    evp->e_value != K_FORMFEED && evp->e_value != K_TAB) {
			msgq(sp, M_BERR,
			    "192|Illegal character; quote to enter");
			break;
		}

insq_ch:	/*
		 * If entering a non-word character after a word, check for
		 * abbreviations.  If there was one, discard replay characters.
		 * If entering a blank character, check for unmap commands,
		 * as well.
		 */
		if (!inword(evp->e_c)) {
			if (abb == AB_INWORD &&
			    !LF_ISSET(TXT_REPLAY) && F_ISSET(gp, G_ABBREV)) {
				if (txt_abbrev(sp, tp, &evp->e_c,
				    LF_ISSET(TXT_INFOLINE), &tmp, &ab_turnoff))
					goto err;
				if (tmp) {
					if (LF_ISSET(TXT_RECORD))
						rcol -= tmp + 1;
					goto ret;
				}
			}
			if (isblank(evp->e_c) && UNMAP_TST)
				txt_unmap(sp, tp, &ec_flags);
		}
		if (abb != AB_NOTSET)
			abb = inword(evp->e_c) ? AB_INWORD : AB_NOTWORD;

insl_ch:	if (tp->owrite)			/* Overwrite a character. */
			--tp->owrite;
		else if (tp->insert) {		/* Insert a character. */
			++tp->len;
			if (tp->insert == 1)
				tp->lb[sp->cno + 1] = tp->lb[sp->cno];
			else
				memmove(tp->lb + sp->cno + 1,
				    tp->lb + sp->cno, tp->insert);
		}
		tp->lb[sp->cno++] = evp->e_c;	/* Finally! */

		/*
		 * !!!
		 * Translate "<CH_HEX>[isxdigit()]*" to a character with
		 * a hex value: this test delimits the value by the max
		 * number of hex bytes.  Offset by one, we use 0 to mean
		 * that we've found <CH_HEX>.
		 */
		if (hexcnt != 0 && hexcnt++ == sizeof(CHAR_T) * 2 + 1) {
			hexcnt = 0;
			if (txt_hex(sp, tp))
				goto err;
		}

		/*
		 * Check to see if we've crossed the margin.
		 *
		 * !!!
		 * In the historic vi, the wrapmargin value was figured out
		 * using the display widths of the characters, i.e. <tab>
		 * characters were counted as two characters if the list edit
		 * option is set, but as the tabstop edit option number of
		 * characters otherwise.  That's what the vs_column() function
		 * gives us, so we use it.
		 */
		if (margin != 0) {
			if (vs_column(sp, &tcol))
				goto err;
			if (tcol >= margin) {
				if (txt_margin(sp, tp, &wmt, &tmp, flags))
					goto err;
				if (tmp) {
					if (isblank(evp->e_c))
						wm_skip = 1;
					wm_set = 1;
					goto k_cr;
				}
			}
		}

		/*
		 * If we've reached the end of the buffer, then we need to
		 * switch into insert mode.  This happens when there's a
		 * change to a mark and the user puts in more characters than
		 * the length of the motion.
		 */
ebuf_chk:	if (sp->cno >= tp->len) {
			BINC_GOTO(sp, tp->lb, tp->lb_len, tp->len + 1);
			LF_SET(TXT_APPENDEOL);

			tp->lb[sp->cno] = CH_CURSOR;
			++tp->insert;
			++tp->len;
		}

		if (quote != Q_NOTSET) {
			if (quote == Q_BNEXT)
				quote = Q_BTHIS;
			if (quote == Q_VNEXT)
				quote = Q_VTHIS;
		}
		break;
	}

#if defined(DEBUG) && 1
	if (sp->cno + tp->insert + tp->owrite != tp->len)
		msgq(sp, M_ERR,
		    "len %u != cno: %u ai: %u insert %u overwrite %u",
		    tp->len, sp->cno, tp->ai, tp->insert, tp->owrite);
	tp->len = sp->cno + tp->insert + tp->owrite;
#endif

ret:	/* If replaying text, keep going. */
	if (LF_ISSET(TXT_REPLAY))
		goto replay;

	/*
	 * Reset the line and update the screen.  (The txt_showmatch() code
	 * refreshes the screen for us.)  Don't refresh unless we're about
	 * to wait on a character or we need to know where the cursor really
	 * is.
	 */
	if (showmatch || margin != 0 || !KEYS_WAITING(sp)) {
		if (vs_change(sp, tp->lno, LINE_RESET))
			return (1);
		if (showmatch) {
			if (txt_showmatch(sp))
				return (1);
		} else if (vs_refresh(sp))
			return (1);
	}

	/* Keep going. */
	goto next;

	if (0) {
done:		/* Leave input mode. */
		F_CLR(sp, S_INPUT);

		/* If recording for playback, save it. */
		if (LF_ISSET(TXT_RECORD))
			vip->rep_cnt = rcol;

		/* Set the final cursor position. */
		vp->m_final.lno = sp->lno;
		vp->m_final.cno = sp->cno;
	}
	return (0);

err:
binc_err:
	txt_err(sp, &sp->tiq);
	return (1);
}

/*
 * txt_abbrev --
 *	Handle abbreviations.
 */
static int
txt_abbrev(sp, tp, pushcp, isinfoline, didsubp, turnoffp)
	SCR *sp;
	TEXT *tp;
	CHAR_T *pushcp;
	int isinfoline, *didsubp, *turnoffp;
{
	VI_PRIVATE *vip;
	CHAR_T ch, *p;
	SEQ *qp;
	size_t len, off;

	/* Check to make sure we're not at the start of an append. */
	*didsubp = 0;
	if (sp->cno == tp->offset)
		return (0);

	vip = VIP(sp);

	/*
	 * Find the start of the "word".
	 *
	 * !!!
	 * We match historic practice, which, as far as I can tell, had an
	 * off-by-one error.  The way this worked was that when the inserted
	 * text switched from a "word" character to a non-word character,
	 * vi would check for possible abbreviations.  It would then take the
	 * type (i.e. word/non-word) of the character entered TWO characters
	 * ago, and move backward in the text until reaching a character that
	 * was not that type, or the beginning of the insert, the line, or
	 * the file.  For example, in the string "abc<space>", when the <space>
	 * character triggered the abbreviation check, the type of the 'b'
	 * character was used for moving through the string.  Maybe there's a
	 * reason for not using the first (i.e. 'c') character, but I can't
	 * think of one.
	 *
	 * Terminate at the beginning of the insert or the character after the
	 * offset character -- both can be tested for using tp->offset.
	 */
	off = sp->cno - 1;			/* Previous character. */
	p = tp->lb + off;
	len = 1;				/* One character test. */
	if (off == tp->offset || isblank(p[-1]))
		goto search;
	if (inword(p[-1]))			/* Move backward to change. */
		for (;;) {
			--off; --p; ++len;
			if (off == tp->offset || !inword(p[-1]))
				break;
		}
	else
		for (;;) {
			--off; --p; ++len;
			if (off == tp->offset ||
			    inword(p[-1]) || isblank(p[-1]))
				break;
		}

	/*
	 * !!!
	 * Historic vi exploded abbreviations on the command line.  This has
	 * obvious problems in that unabbreviating the string can be extremely
	 * tricky, particularly if the string has, say, an embedded escape
	 * character.  Personally, I think it's a stunningly bad idea.  Other
	 * examples of problems this caused in historic vi are:
	 *	:ab foo bar
	 *	:ab foo baz
	 * results in "bar" being abbreviated to "baz", which wasn't what the
	 * user had in mind at all.  Also, the commands:
	 *	:ab foo bar
	 *	:unab foo<space>
	 * resulted in an error message that "bar" wasn't mapped.  Finally,
	 * since the string was already exploded by the time the unabbreviate
	 * command got it, all it knew was that an abbreviation had occurred.
	 * Cleverly, it checked the replacement string for its unabbreviation
	 * match, which meant that the commands:
	 *	:ab foo1 bar
	 *	:ab foo2 bar
	 *	:unab foo2
	 * unabbreviate "foo1", and the commands:
	 *	:ab foo bar
	 *	:ab bar baz
	 * unabbreviate "foo"!
	 *
	 * Anyway, people neglected to first ask my opinion before they wrote
	 * macros that depend on this stuff, so, we make this work as follows.
	 * When checking for an abbreviation on the command line, if we get a
	 * string which is <blank> terminated and which starts at the beginning
	 * of the line, we check to see it is the abbreviate or unabbreviate
	 * commands.  If it is, turn abbreviations off and return as if no
	 * abbreviation was found.  Note also, minor trickiness, so that if
	 * the user erases the line and starts another command, we turn the
	 * abbreviations back on.
	 *
	 * This makes the layering look like a Nachos Supreme.
	 */
search:	if (isinfoline)
		if (off == tp->ai || off == tp->offset)
			if (ex_is_abbrev(p, len)) {
				*turnoffp = 1;
				return (0);
			} else
				*turnoffp = 0;
		else
			if (*turnoffp)
				return (0);

	/* Check for any abbreviations. */
	if ((qp = seq_find(sp, NULL, NULL, p, len, SEQ_ABBREV, NULL)) == NULL)
		return (0);

	/*
	 * Push the abbreviation onto the tty stack.  Historically, characters
	 * resulting from an abbreviation expansion were themselves subject to
	 * map expansions, O_SHOWMATCH matching etc.  This means the expanded
	 * characters will be re-tested for abbreviations.  It's difficult to
	 * know what historic practice in this case was, since abbreviations
	 * were applied to :colon command lines, so entering abbreviations that
	 * looped was tricky, although possible.  In addition, obvious loops
	 * didn't work as expected.  (The command ':ab a b|ab b c|ab c a' will
	 * silently only implement and/or display the last abbreviation.)
	 *
	 * This implementation doesn't recover well from such abbreviations.
	 * The main input loop counts abbreviated characters, and, when it
	 * reaches a limit, discards any abbreviated characters on the queue.
	 * It's difficult to back up to the original position, as the replay
	 * queue would have to be adjusted, and the line state when an initial
	 * abbreviated character was received would have to be saved.
	 */
	ch = *pushcp;
	if (v_event_push(sp, NULL, &ch, 1, CH_ABBREVIATED))
		return (1);
	if (v_event_push(sp, NULL, qp->output, qp->olen, CH_ABBREVIATED))
		return (1);

	/* Move to the start of the abbreviation, adjust the length. */
	sp->cno -= len;
	tp->len -= len;

	/* Copy any insert characters back. */
	if (tp->insert)
		memmove(tp->lb + sp->cno + tp->owrite,
		    tp->lb + sp->cno + tp->owrite + len, tp->insert);

	/*
	 * We return the length of the abbreviated characters.  This is so
	 * the calling routine can replace the replay characters with the
	 * abbreviation.  This means that subsequent '.' commands will produce
	 * the same text, regardless of intervening :[un]abbreviate commands.
	 * This is historic practice.
	 */
	*didsubp = len;
	return (0);
}

/*
 * txt_unmap --
 *	Handle the unmap command.
 */
static void
txt_unmap(sp, tp, ec_flagsp)
	SCR *sp;
	TEXT *tp;
	u_int32_t *ec_flagsp;
{
	size_t len, off;
	char *p;

	/* Find the beginning of this "word". */
	for (off = sp->cno - 1, p = tp->lb + off, len = 0;; --p, --off) {
		if (isblank(*p)) {
			++p;
			break;
		}
		++len;
		if (off == tp->ai || off == tp->offset)
			break;
	}

	/*
	 * !!!
	 * Historic vi exploded input mappings on the command line.  See the
	 * txt_abbrev() routine for an explanation of the problems inherent
	 * in this.
	 *
	 * We make this work as follows.  If we get a string which is <blank>
	 * terminated and which starts at the beginning of the line, we check
	 * to see it is the unmap command.  If it is, we return that the input
	 * mapping should be turned off.  Note also, minor trickiness, so that
	 * if the user erases the line and starts another command, we go ahead
	 * an turn mapping back on.
	 */
	if ((off == tp->ai || off == tp->offset) && ex_is_unmap(p, len))
		FL_CLR(*ec_flagsp, EC_MAPINPUT);
	else
		FL_SET(*ec_flagsp, EC_MAPINPUT);
}

/*
 * txt_ai_resolve --
 *	When a line is resolved by <esc> or <cr>, review autoindent
 *	characters.
 */
static void
txt_ai_resolve(sp, tp)
	SCR *sp;
	TEXT *tp;
{
	u_long ts;
	int del;
	size_t cno, len, new, old, scno, spaces, tab_after_sp, tabs;
	char *p;

	/*
	 * If the line is empty, has an offset, or no autoindent
	 * characters, we're done.
	 */
	if (!tp->len || tp->offset || !tp->ai)
		return;

	/*
	 * If the length is less than or equal to the autoindent
	 * characters, delete them.
	 */
	if (tp->len <= tp->ai) {
		tp->len = tp->ai = 0;
		if (tp->lno == sp->lno)
			sp->cno = 0;
		return;
	}

	/*
	 * The autoindent characters plus any leading <blank> characters
	 * in the line are resolved into the minimum number of characters.
	 * Historic practice.
	 */
	ts = O_VAL(sp, O_TABSTOP);

	/* Figure out the last <blank> screen column. */
	for (p = tp->lb, scno = 0, len = tp->len,
	    spaces = tab_after_sp = 0; len-- && isblank(*p); ++p)
		if (*p == '\t') {
			if (spaces)
				tab_after_sp = 1;
			scno += COL_OFF(scno, ts);
		} else {
			++spaces;
			++scno;
		}

	/*
	 * If there are no spaces, or no tabs after spaces and less than
	 * ts spaces, it's already minimal.
	 */
	if (!spaces || !tab_after_sp && spaces < ts)
		return;

	/* Count up spaces/tabs needed to get to the target. */
	for (cno = 0, tabs = 0; cno + COL_OFF(cno, ts) <= scno; ++tabs)
		cno += COL_OFF(cno, ts);
	spaces = scno - cno;

	/*
	 * Figure out how many characters we're dropping -- if we're not
	 * dropping any, it's already minimal, we're done.
	 */
	old = p - tp->lb;
	new = spaces + tabs;
	if (old == new)
		return;

	/* Shift the rest of the characters down, adjust the counts. */
	del = old - new;
	memmove(p - del, p, tp->len - old);
	tp->len -= del;

	/* If the cursor was on this line, adjust it as well. */
	if (sp->lno == tp->lno)
		sp->cno -= del;

	/* Fill in space/tab characters. */
	for (p = tp->lb; tabs--;)
		*p++ = '\t';
	while (spaces--)
		*p++ = ' ';
}

/*
 * v_txt_auto --
 *	Handle autoindent.  If aitp isn't NULL, use it, otherwise,
 *	retrieve the line.
 *
 * PUBLIC: int v_txt_auto __P((SCR *, recno_t, TEXT *, size_t, TEXT *));
 */
int
v_txt_auto(sp, lno, aitp, len, tp)
	SCR *sp;
	recno_t lno;
	TEXT *aitp, *tp;
	size_t len;
{
	size_t nlen;
	char *p, *t;

	if (aitp == NULL) {
		/*
		 * If the ex append command is executed with an address of 0,
		 * it's possible to get here with a line number of 0.  Return
		 * an indent of 0.
		 */
		if (lno == 0) {
			tp->ai = 0;
			return (0);
		}
		if ((t = file_gline(sp, lno, &len)) == NULL)
			return (1);
	} else
		t = aitp->lb;

	/* Count whitespace characters. */
	for (p = t; len > 0; ++p, --len)
		if (!isblank(*p))
			break;

	/* Set count, check for no indentation. */
	if ((nlen = (p - t)) == 0)
		return (0);

	/* Make sure the buffer's big enough. */
	BINC_RET(sp, tp->lb, tp->lb_len, tp->len + nlen);

	/* Copy the buffer's current contents up. */
	if (tp->len != 0)
		memmove(tp->lb + nlen, tp->lb, tp->len);
	tp->len += nlen;

	/* Copy the indentation into the new buffer. */
	memmove(tp->lb, t, nlen);

	/* Set the autoindent count. */
	tp->ai = nlen;
	return (0);
}

/*
 * txt_backup --
 *	Back up to the previously edited line.
 */
static TEXT *
txt_backup(sp, tiqh, tp, flagsp)
	SCR *sp;
	TEXTH *tiqh;
	TEXT *tp;
	u_int32_t *flagsp;
{
	VI_PRIVATE *vip;
	TEXT *ntp;

	/* Get a handle on the previous TEXT structure. */
	if ((ntp = tp->q.cqe_prev) == (void *)tiqh) {
		msgq(sp, M_BERR, "193|Already at the beginning of the insert");
		return (tp);
	}

	/* Reset the cursor, bookkeeping. */
	sp->lno = ntp->lno;
	sp->cno = ntp->sv_cno;
	ntp->len = ntp->sv_len;

	/* Handle appending to the line. */
	vip = VIP(sp);
	if (ntp->owrite == 0 && ntp->insert == 0) {
		ntp->lb[ntp->len] = CH_CURSOR;
		++ntp->insert;
		++ntp->len;
		FL_SET(*flagsp, TXT_APPENDEOL);
	} else
		FL_CLR(*flagsp, TXT_APPENDEOL);

	/* Release the current TEXT. */
	CIRCLEQ_REMOVE(tiqh, tp, q);
	text_free(tp);

	/* Update the old line on the screen. */
	if (vs_change(sp, ntp->lno + 1, LINE_DELETE))
		return (NULL);

	/* Return the new/current TEXT. */
	return (ntp);
}

/*
 * txt_err --
 *	Handle an error during input processing.
 */
static void
txt_err(sp, tiqh)
	SCR *sp;
	TEXTH *tiqh;
{
	recno_t lno;

	/*
	 * The problem with input processing is that the cursor is at an
	 * indeterminate position since some input may have been lost due
	 * to a malloc error.  So, try to go back to the place from which
	 * the cursor started, knowing that it may no longer be available.
	 *
	 * We depend on at least one line number being set in the text
	 * chain.
	 */
	for (lno = tiqh->cqh_first->lno;
	    !file_eline(sp, lno) && lno > 0; --lno);

	sp->lno = lno == 0 ? 1 : lno;
	sp->cno = 0;

	/* Redraw the screen, just in case. */
	F_SET(sp, S_SCR_REDRAW);
}

/*
 * txt_hex --
 *	Let the user insert any character value they want.
 *
 * !!!
 * This is an extension.  The pattern "^X[0-9a-fA-F]*" is a way
 * for the user to specify a character value which their keyboard
 * may not be able to enter.
 */
static int
txt_hex(sp, tp)
	SCR *sp;
	TEXT *tp;
{
	CHAR_T savec;
	size_t len, off;
	u_long value;
	char *p, *wp;

	/*
	 * Null-terminate the string.  Since nul isn't a legal hex value,
	 * this should be okay, and lets us use a local routine, which
	 * presumably understands the character set, to convert the value.
	 */
	savec = tp->lb[sp->cno];
	tp->lb[sp->cno] = 0;

	/* Find the previous CH_HEX character. */
	for (off = sp->cno - 1, p = tp->lb + off, len = 0;; --p, --off, ++len) {
		if (*p == CH_HEX) {
			wp = p + 1;
			break;
		}
		/* Not on this line?  Shouldn't happen. */
		if (off == tp->ai || off == tp->offset)
			goto nothex;
	}

	/* If length of 0, then it wasn't a hex value. */
	if (len == 0)
		goto nothex;

	/* Get the value. */
	errno = 0;
	value = strtol(wp, NULL, 16);
	if (errno || value > MAX_CHAR_T) {
nothex:		tp->lb[sp->cno] = savec;
		return (0);
	}

	/* Restore the original character. */
	tp->lb[sp->cno] = savec;

	/* Adjust the bookkeeping. */
	sp->cno -= len;
	tp->len -= len;
	tp->lb[sp->cno - 1] = value;

	/* Copy down any overwrite characters. */
	if (tp->owrite)
		memmove(tp->lb + sp->cno,
		    tp->lb + sp->cno + len, tp->owrite);

	/* Copy down any insert characters. */
	if (tp->insert)
		memmove(tp->lb + sp->cno + tp->owrite,
		    tp->lb + sp->cno + tp->owrite + len, tp->insert);

	return (0);
}

/*
 * Text indentation is truly strange.  ^T and ^D do movements to the next or
 * previous shiftwidth value, i.e. for a 1-based numbering, with shiftwidth=3,
 * ^T moves a cursor on the 7th, 8th or 9th column to the 10th column, and ^D
 * moves it back.
 *
 * !!!
 * The ^T and ^D characters in historical vi had special meaning only when they
 * were the first characters entered after entering text input mode.  As normal
 * erase characters couldn't erase autoindent characters (^T in this case), it
 * meant that inserting text into previously existing text was strange -- ^T
 * only worked if it was the first keystroke(s), and then could only be erased
 * using ^D.  This implementation treats ^T specially anywhere it occurs in the
 * input, and permits the standard erase characters to erase the characters it
 * inserts.
 *
 * !!!
 * A fun test is to try:
 *	:se sw=4 ai list
 *	i<CR>^Tx<CR>^Tx<CR>^Tx<CR>^Dx<CR>^Dx<CR>^Dx<esc>
 * Historic vi loses some of the '$' marks on the line ends, but otherwise gets
 * it right.
 *
 * XXX
 * Technically, txt_dent should be part of the screen interface, as it requires
 * knowledge of character sizes, including <space>s, on the screen.  It's here
 * because it's a complicated little beast, and I didn't want to shove it down
 * into the screen.  It's probable that KEY_LEN will call into the screen once
 * there are screens with different character representations.
 *
 * txt_dent --
 *	Handle ^T indents, ^D outdents.
 *
 * If anything changes here, check the ex version to see if it needs similar
 * changes.  It's in ex/ex_txt_ev.c:txt_dent().
 */
static int
txt_dent(sp, tp, isindent)
	SCR *sp;
	TEXT *tp;
	int isindent;
{
	u_long sw, ts;
	size_t cno, current, off, spaces, target, tabs;
	int ai_reset;

	ts = O_VAL(sp, O_TABSTOP);
	sw = O_VAL(sp, O_SHIFTWIDTH);

	/*
	 * Since we don't know what precedes the character(s) being inserted
	 * (or deleted), the preceding whitespace characters must be resolved.
	 * An example is a <tab>, which doesn't need a full shiftwidth number
	 * of columns because it's preceded by <space>s.  This is easy to get
	 * if the user sets shiftwidth to a value less than tabstop (or worse,
	 * something for which tabstop isn't a multiple) and then uses ^T to
	 * indent, and ^D to outdent.
	 *
	 * Figure out the current and target screen columns.  In the historic
	 * vi, the autoindent column was NOT determined using display widths
	 * of characters as was the wrapmargin column.  For that reason, we
	 * can't use the vs_column() function, but have to calculate it here.
	 * This is slow, but it's normally only on the first few characters of
	 * a line.
	 */
	for (current = cno = 0; cno < sp->cno; ++cno)
		current += tp->lb[cno] == '\t' ?
		    COL_OFF(current, ts) : KEY_LEN(sp, tp->lb[cno]);

	target = current;
	if (isindent)
		target += COL_OFF(target, sw);
	else
		target -= --target % sw;

	/*
	 * The AI characters will be turned into overwrite characters if the
	 * cursor immediately follows them.  We test both the cursor position
	 * and the indent flag because there's no single test.  (^T can only
	 * be detected by the cursor position, and while we know that the test
	 * is always true for ^D, the cursor can be in more than one place, as
	 * "0^D" and "^D" are different.)
	 */
	ai_reset = !isindent || sp->cno == tp->ai + tp->offset;

	/*
	 * Back up over any previous <blank> characters, changing them into
	 * overwrite characters (including any ai characters).  For ^D, we
	 * will move up to or past the target by definition, otherwise, the
	 * command wouldn't have gotten this far.
	 */
	for (; sp->cno > tp->offset; --sp->cno, ++tp->owrite)
		if (tp->lb[sp->cno - 1] == ' ')
			--current;
		else if (tp->lb[sp->cno - 1] == '\t')
			current -= COL_OFF(current, ts);
		else
			break;

	/*
	 * Count up the total spaces/tabs needed to get from the beginning of
	 * the line (or the last non-<blank> character) to the target.
	 */
	for (cno = current, tabs = 0; cno + COL_OFF(cno, ts) <= target; ++tabs)
		cno += COL_OFF(cno, ts);
	spaces = target - cno;

	/* If we overwrote ai characters, reset the ai count. */
	if (ai_reset)
		tp->ai = tabs + spaces;

	/*
	 * If there are insert characters in the line, shift them up or
	 * down depending on if we're gaining or losing characters.
	 *
	 * XXX
	 * I'm not sure this is right, we're overwriting characters with
	 * inserted whitespace, and that might not be the best interface.
	 * The alternative would be to only overwrite ai and previous
	 * <blank> characters with the inserted whitespace, and push the
	 * user's characters forward as usual, letting them be overwritten
	 * with normally entered characters, or discarded when <escape>
	 * or <carriage-return> is entered.
	 */
	if (tp->insert && tabs + spaces != tp->owrite)
		if (tabs + spaces > tp->owrite) {
			BINC_RET(sp,
			    tp->lb, tp->lb_len, tp->len + tabs + spaces);
			off = (tabs + spaces) - tp->owrite;
			memmove(tp->lb + sp->cno + tp->owrite + off,
			    tp->lb + sp->cno + tp->owrite, tp->insert);
			tp->len += off;
			tp->owrite += off;
		} else {
			off = tp->owrite - (tabs + spaces);
			memmove(tp->lb + sp->cno + tp->owrite - off,
			    tp->lb + sp->cno + tp->owrite, tp->insert);
			tp->len -= off;
			tp->owrite -= off;
		}

	/* Enter the replacement characters. */
	for (; tabs > 0; --tabs) {
		--tp->owrite;
		tp->lb[sp->cno++] = '\t';
	}
	for (; spaces > 0; --spaces) {
		--tp->owrite;
		tp->lb[sp->cno++] = ' ';
	}
	return (0);
}

/*
 * txt_resolve --
 *	Resolve the input text chain into the file.
 */
static int
txt_resolve(sp, tiqh, flags)
	SCR *sp;
	TEXTH *tiqh;
	u_int32_t flags;
{
	VI_PRIVATE *vip;
	TEXT *tp;
	recno_t lno;

	/*
	 * The first line replaces a current line, and all subsequent lines
	 * are appended into the file.  Resolve autoindented characters for
	 * each line before committing it.
	 */
	vip = VIP(sp);
	tp = tiqh->cqh_first;
	if (LF_ISSET(TXT_AUTOINDENT))
		txt_ai_resolve(sp, tp);
	if (file_sline(sp, tp->lno, tp->lb, tp->len))
		return (1);

	for (lno = tp->lno; (tp = tp->q.cqe_next) != (void *)&sp->tiq; ++lno) {
		if (LF_ISSET(TXT_AUTOINDENT))
			txt_ai_resolve(sp, tp);
		if (file_aline(sp, 0, lno, tp->lb, tp->len))
			return (1);
	}

	/*
	 * Clear the input flag, the look-aside buffer is no longer valid.
	 * Has to be done as part of text resolution, or upon return we'll
	 * be looking at incorrect data.
	 */
	F_CLR(sp, S_INPUT);

	return (0);
}

/*
 * txt_showmatch --
 *	Show a character match.
 *
 * !!!
 * Historic vi tried to display matches even in the :colon command line.
 * I think not.
 */
static int
txt_showmatch(sp)
	SCR *sp;
{
	GS *gp;
	struct timeval second;
	VCS cs;
	MARK m;
	fd_set zero;
	int cnt, endc, startc;

	gp = sp->gp;

	/*
	 * Do a refresh first, in case the v_ntext() code hasn't done
	 * one in awhile, so the user can see what we're complaining
	 * about.
	 */
	if (vs_refresh(sp))
		return (1);
	/*
	 * We don't display the match if it's not on the screen.  Find
	 * out what the first character on the screen is.
	 */
	if (vs_sm_position(sp, &m, 0, P_TOP))
		return (1);

	/* Initialize the getc() interface. */
	cs.cs_lno = sp->lno;
	cs.cs_cno = sp->cno - 1;
	if (cs_init(sp, &cs))
		return (1);
	startc = (endc = cs.cs_ch)  == ')' ? '(' : '{';

	/* Search for the match. */
	for (cnt = 1;;) {
		if (cs_prev(sp, &cs))
			return (1);
		if (cs.cs_flags != 0) {
			if (cs.cs_flags == CS_EOF || cs.cs_flags == CS_SOF) {
				msgq(sp, M_BERR,
				    "Unmatched %s", KEY_NAME(sp, endc));
				return (vs_refresh(sp));
			}
			continue;
		}
		if (cs.cs_ch == endc)
			++cnt;
		else if (cs.cs_ch == startc && --cnt == 0)
			break;
	}

	/* If the match is on the screen, move to it. */
	if (cs.cs_lno < m.lno ||
	    cs.cs_lno == m.lno && cs.cs_cno < m.cno)
		return (0);
	m.lno = sp->lno;
	m.cno = sp->cno;
	sp->lno = cs.cs_lno;
	sp->cno = cs.cs_cno;
	if (vs_refresh(sp))
		return (1);

	/*
	 * Sleep(3) is eight system calls.  Do it fast -- besides,
	 * I don't want to wait an entire second.
	 */
	FD_ZERO(&zero);
	second.tv_sec = O_VAL(sp, O_MATCHTIME) / 10;
	second.tv_usec = (O_VAL(sp, O_MATCHTIME) % 10) * 100000L;
	(void)select(0, &zero, &zero, &zero, &second);

	/* Return to the current location. */
	sp->lno = m.lno;
	sp->cno = m.cno;
	return (vs_refresh(sp));
}

/*
 * txt_margin --
 *	Handle margin wrap.
 */
static int
txt_margin(sp, tp, wmtp, didbreak, flags)
	SCR *sp;
	TEXT *tp, *wmtp;
	int *didbreak;
	u_int32_t flags;
{
	VI_PRIVATE *vip;
	size_t len, off;
	char *p, *wp;

	/* Find the nearest previous blank. */
	for (off = sp->cno - 1, p = tp->lb + off, len = 0;; --off, --p, ++len) {
		if (isblank(*p)) {
			wp = p + 1;
			break;
		}

		/*
		 * If reach the start of the line, there's nowhere to break.
		 *
		 * !!!
		 * Historic vi belled each time a character was entered after
		 * crossing the margin until a space was entered which could
		 * be used to break the line.  I don't as it tends to wake the
		 * cats.
		 */
		if (off == tp->ai || off == tp->offset) {
			*didbreak = 0;
			return (0);
		}
	}

	/*
	 * Store saved information about the rest of the line in the
	 * wrapmargin TEXT structure.
	 *
	 * !!!
	 * The offset field holds the length of the current characters
	 * that the user entered, but which are getting split to the new
	 * line -- it's going to be used to set the cursor value when we
	 * move to the new line.
	 */
	vip = VIP(sp);
	wmtp->lb = p + 1;
	wmtp->offset = len;
	wmtp->insert = LF_ISSET(TXT_APPENDEOL) ?  tp->insert - 1 : tp->insert;
	wmtp->owrite = tp->owrite;

	/* Correct current bookkeeping information. */
	sp->cno -= len;
	if (LF_ISSET(TXT_APPENDEOL)) {
		tp->len -= len + tp->owrite + (tp->insert - 1);
		tp->insert = 1;
	} else {
		tp->len -= len + tp->owrite + tp->insert;
		tp->insert = 0;
	}
	tp->owrite = 0;

	/*
	 * !!!
	 * Delete any trailing whitespace from the current line.
	 */
	for (;; --p, --off) {
		if (!isblank(*p))
			break;
		--sp->cno;
		--tp->len;
		if (off == tp->ai || off == tp->offset)
			break;
	}
	*didbreak = 1;
	return (0);
}

/*
 * txt_Rcleanup --
 *	Resolve the input line for the 'R' command.
 */
static void
txt_Rcleanup(sp, tiqh, tp, lp, olen)
	SCR *sp;
	TEXTH *tiqh;
	TEXT *tp;
	const char *lp;
	const size_t olen;
{
	TEXT *ttp;
	size_t ilen, tmp;

	/*
	 * Check to make sure that the cursor hasn't moved beyond
	 * the end of the line.
	 */
	if (tp->owrite == 0)
		return;

	/*
	 * Calculate how many characters the user has entered,
	 * plus the blanks erased by <carriage-return>/<newline>s.
	 */
	for (ttp = tiqh->cqh_first, ilen = 0;;) {
		ilen += ttp == tp ? sp->cno : ttp->len + ttp->R_erase;
		if ((ttp = ttp->q.cqe_next) == (void *)&sp->tiq)
			break;
	}

	/*
	 * If the user has entered less characters than the original line
	 * was long, restore any overwriteable characters to the original
	 * characters.  These characters are entered as "insert characters",
	 * because they're after the cursor and we don't want to lose them.
	 * (This is okay because the R command has no insert characters.)
	 * We set owrite to 0 so that the insert characters don't get copied
	 * to somewhere else, which means that the length has to be adjusted
	 * here as well.
	 */
	if (ilen < olen) {
		tmp = MIN(tp->owrite, olen - ilen);
		memmove(tp->lb + sp->cno, lp + ilen, tmp);
		tp->len -= tp->owrite - tmp;
		tp->owrite = 0;
		tp->insert += tmp;
	}
}

/*
 * txt_nomorech --
 *	No more characters message.
 */
static void
txt_nomorech(sp)
	SCR *sp;
{
	msgq(sp, M_BERR, "194|No more characters to erase");
}
