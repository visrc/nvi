/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vi.c,v 10.4 1995/06/09 12:52:38 bostic Exp $ (Berkeley) $Date: 1995/06/09 12:52:38 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
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

static int	v_alias __P((SCR *, CHAR_T *));
static void	v_dtoh __P((SCR *));
static int	v_scr_init __P((SCR *));
static int	v_keyword __P((SCR *));

#if defined(DEBUG) && defined(COMLOG)
static void	v_comlog __P((SCR *, VICMD *));
#endif

/*
 * The O_TILDEOP option makes the ~ command take a motion instead
 * of a straight count.  This is the replacement structure we use
 * instead of the one currently in the VIKEYS table.
 */
VIKEYS const tmotion = {
	v_mulcase,	V_CNT|V_DOT|V_MOTION|VM_RCM_SET,
	"[count]~[count]motion",
	" ~ change case to motion"
};

/*
 * vi --
 * 	Vi event handler.
 *
 * The command structure for vi is less complex than ex (and don't think
 * I'm not grateful!)  The command syntax is:
 *
 *	[count] [buffer] [count] key [[motion] | [buffer] [character]]
 *
 * and there are several special cases.  The motion value is itself a vi
 * command, with the syntax:
 *
 *	[count] key [character]
 *
 * PUBLIC: int vi __P((SCR *, EVENT *));
 */
int
vi(sp, evp)
	SCR *sp;
	EVENT *evp;
{
	GS *gp;
	MARK abs, m;
	SCR *tsp;
	VICMD *vmp, *vp;
	VI_PRIVATE *vip;
	s_vi_t state;
	size_t len;
	u_long sv_cnt, tmp;
	int complete, intext, tilde_reset;

	/* Local variables. */
	gp = sp->gp;
	vip = VIP(sp);
	vmp = &vip->motion;

	/*
	 * The first time we enter the screen, we have to initialize the
	 * VICMD structure we're using.  Don't try and move this further
	 * down in the code, it won't work.
	 */
	if ((vp = vip->vp) == NULL)
		vp = vip->vp = &vip->cmd;

	tilde_reset = 0;

	switch (evp->e_event) {			/* !E_CHARACTER events. */
	case E_CHARACTER:
		break;
	case E_EOF:
	case E_ERR:
		return (0);
	case E_INTERRUPT:
		/* Interrupts get passed on to other handlers. */
		if (vip->cm_state == VS_RUNNING)
			break;
		goto interrupt;
	case E_RESIZE:
		v_dtoh(sp);
		if (v_scr_init(sp))
			return (1);
		goto done;
	case E_SIGCONT:
		return (0);
	case E_START:
		/* Initialize the screen .*/
		if (v_scr_init(sp))
			return (1);
		
		/* Reset strange attraction. */
		F_SET(&vip->cmd, VM_RCM_SET);
		goto cmd;
	case E_STOP:
		return (0);
	default:
		abort();
	}

next_state:
	switch (vip->cm_state) {		/* E_CHARACTER events. */
	/*
	 * VS_RUNNING:
	 *
	 * Something else is running -- a search loop or a text input loop.
	 */
	case VS_RUNNING:
		intext = F_ISSET(sp, S_INPUT);

		/*
		 * Pass the event on to the handler.  If the handler finishes,
		 * move to the next state.
		 */
		if (vip->run_func != NULL) {
			if (vip->run_func(sp, evp, &complete))
				return (1);
			if (!complete)
				return (0);
		}

		/*
		 * If we quit because of an interrupt, move to the command
		 * state.
		 *
		 * !!!
		 * Historically, vi beeped on command level and text input
		 * interrupts.  However, people use ^C to end text input
		 * mode because <escape> is hard to reach on terminals and
		 * it does the right thing.  So, we don't beep if we're
		 * ending text input.
		 *
		 * Historically, vi exited to ex mode if no file was named
		 * on the command line, and two interrupts were generated in
		 * a row.  (You may want to write that down, in case there's
		 * a test, later.)
		 */
		if (evp->e_event == E_INTERRUPT) {
			if (!intext) {
interrupt:			(void)gp->scr_bell(sp);
				ex_message(sp, NULL, EXM_INTERRUPT);
			}
			goto cmd;
		}

		vip->cm_state = vip->cm_next;
		goto next_state;

	/*
	 * VS_EX_TEARDOWN1:
	 *
	 * We have just finished getting input for an ex command.  Tear down
	 * the text input setup and execute it.  If v_ex_td1 returns without
	 * resetting the state, then we've finished.
	 */
	case VS_EX_TEARDOWN1:
		if (v_tcmd_td(sp, vp))
			return (1);
		if (v_ex_td1(sp, vp))
			return (1);
		if (vip->cm_state != VS_EX_TEARDOWN1)
			break;
		goto command_restart;

	/*
	 * VS_EX_TEARDOWN2:
	 *
	 * We are continuing to execute the ex command.  If v_ex_td2 returns
	 * without resetting the state, then we've finished.
	 */
	case VS_EX_TEARDOWN2:
		if (v_ex_td2(sp, vp))
			return (1);
		if (vip->cm_state != VS_EX_TEARDOWN2)
			break;
		goto command_restart;

	/*
	 * VS_FILTER_TEARDOWN:
	 *
	 * We have just finished getting input for an ex filter command.  Tear
	 * down the text input setup and execute it.
	 */
	case VS_FILTER_TEARDOWN:
		if (v_tcmd_td(sp, vp))
			return (1);
		if (v_filter_td(sp, vp))
			return (1);
		goto command_restart;

	/*
	 * VS_SEARCH_TEARDOWN1:
	 *
	 * We have just finished getting input for a ex address search command,
	 * used as a normal or motion command.  Tear down the text input setup
	 * and execute it.
	 */
	case VS_SEARCH_TEARDOWN:
		if (v_tcmd_td(sp, vp))
			return (1);
		if (v_a_td(sp, vp))
			return (1);
		if (ISMOTION(vp))
			goto motion_restart;
		goto command_restart;

	/*
	 * VS_REPLACE_CHAR1, VS_REPLACE_CHAR2:
	 *
	 * We have the replace command, and we're looking for a character.
	 * <escape> cancels the replacement, <literal> escapes the following
	 * character.
	 */
	case VS_REPLACE_CHAR1:
		if (evp->e_value == K_ESCAPE)
			goto cmd;
		if (evp->e_value == K_VLNEXT) {
			vip->cm_state = VS_REPLACE_CHAR2;
			break;
		}
		/* FALLTHROUGH */

	case VS_REPLACE_CHAR2:
		vip->rlast = evp->e_c;
		vip->rvalue = evp->e_value;
		if (v_replace_td(sp, vp))
			return (1);
		goto command_restart;

	/*
	 * VS_TEXT_TEARDOWN:
	 *
	 * We have just ended text input mode.  Finish the command.
	 */
	case VS_TEXT_TEARDOWN:
		goto command_restart;

	/*
	 * VS_GET_CMD1:
	 *
	 * Initial command state.
	 */
	case VS_GET_CMD1:
		/* Refresh the command structure. */
		memset(vp, 0, sizeof(VICMD));

		/*
		 * If not currently in a map, log the cursor position, and set
		 * a flag so that this command can become the dot command.
		 */
		if (F_ISSET(&evp->e_ch, CH_MAPPED))
			F_SET(vip, VIP_MAPPED);
		else {
			(void)log_cursor(sp);
			F_CLR(vip, VIP_MAPPED);
		}

		/*
		 * !!!
		 * Escape handling: <escape> will silently cancel partial
		 * commands, i.e. a command where at least the command
		 * character has been entered (buffers and counts aren't
		 * sufficient). Otherwise, it beeps the terminal.
		 *
		 * POSIX 1003.2-1992 explicitly disallows cancelling commands
		 * where all that's been entered is a number, requiring that
		 * the terminal be alerted.
		 *
		 * Vi historically flushed mapped characters on error, but
		 * entering extra <escape> characters at the beginning of
		 * a map wasn't considered an error -- I've found users that
		 * put leading <escape> characters in maps to clean up vi
		 * state before the map was interpreted.  Beauty!
		 */
		if (evp->e_value == K_ESCAPE) {
			msgq(sp, M_BERR, "207|Already in command mode");
			break;
		}

		/*
		 * !!!
		 * All commands support specifying a buffer in the syntax
		 * although all commands don't use it (e.g. "aj is the
		 * same as j alone).
		 */
		if (evp->e_c == '"') {			/* Buffer. */
			vip->cm_next = VS_GET_CMD2;
			vip->cm_state = VS_GET_BUFFER;
			break;
		}
		/* FALLTHROUGH */

	/*
	 * VS_GET_CMD2:
	 *
	 * Second command state.  We have retrieved the buffer name if
	 * specified, and are continuing with the command.
	 */
	case VS_GET_CMD2:
		if (evp->e_value == K_ESCAPE)
			goto esc;

		/*
		 * !!!
		 * All commands support specifying a count in the syntax
		 * although all commands don't use it (e.g. 35M is the
		 * same as M alone).
		 */
		if (isdigit(evp->e_c) && evp->e_c != '0') {
			FL_SET(gp->ec_flags, EC_MAPNODIGIT);
			F_SET(vp, VC_C1SET);

			vip->cm_next = VS_GET_CMD3;
			vip->cm_state = VS_GET_COUNT;
			vip->cm_countp = &vp->count;
			goto count;
		}
		/* FALLTHROUGH */

	/*
	 * VS_GET_CMD3:
	 *
	 * Third command state.  We have retrieved the count if specified,
	 * and are continuing with the command.
	 */
	case VS_GET_CMD3:
		if (evp->e_value == K_ESCAPE)
			goto esc;

		/* Check for command aliases. */
		if (v_alias(sp, &evp->e_c))
			return (1);
		
		/* Got a command!  Check for an OOB command key. */
		if (evp->e_c > MAXVIKEY) {
			v_message(sp, KEY_NAME(sp, evp->e_c), VIM_NOCOM);
			goto err;
		}
		vp->kp = &vikeys[vp->key = evp->e_c];

		/* Copy the base command flags into the command structure. */
		F_SET(vp, vp->kp->flags);

		/* Tildeop makes the ~ command take a motion. */
		if (evp->e_c == '~' && O_ISSET(sp, O_TILDEOP))
			vp->kp = &tmotion;

		/*
		 * The only legal command with no underlying function is dot.
		 * The code path breaks into two parts here, rejoined at the
		 * dot_done label. 
		 */
		if (vp->kp->func != NULL)
			goto notdot;

		/*
		 * It's historic practice that <escape> beeps the terminal as
		 * well as erasing the entered buffer/number.  It's a common
		 * problem, so just beep the terminal unless verbose was set.
		 */
		if (evp->e_c != '.') {
			v_message(sp, KEY_NAME(sp, evp->e_c),
			    evp->e_value == K_ESCAPE ? VIM_NOCOM_B : VIM_NOCOM);
			goto err;
		}

		/* A repeatable command must have been executed. */
		if (!F_ISSET(&vip->sdot, VC_ISDOT)) {
			msgq(sp, M_ERR, "208|No command to repeat");
			goto err;
		}

		/*
		 * !!!
		 * If a '.' is entered after an undo command, we replay the
		 * log instead of redoing the last command.  This is necessary
		 * because 'u' can't set the dot command -- see v_undo() for
		 * details.
		 */
		if (VIP(sp)->u_ccnt == sp->ccnt) {
			vp->kp = &vikeys['u'];
			F_SET(vp, VC_ISDOT);
		} else {
			/*
			 * Set new count/buffer.  It's historical practice that
			 * when a new count is specified to a dot command, any
			 * motion component goes away, e.g. "d3w2." will delete
			 * a total of 5 words.
			 */
			if (F_ISSET(vp, VC_C1SET)) {
				F_SET(&vip->sdot, VC_C1SET);
				vip->sdot.count = vp->count;
				vip->sdotmotion.count = 1;
			}
			if (F_ISSET(vp, VC_BUFFER)) {
				F_SET(&vip->sdot, VC_BUFFER);
				vip->sdot.buffer = vp->buffer;
			}
			*vp = vip->sdot;
		}
		goto dot_done;

notdot:		/* Check for an illegal count. */
		if (F_ISSET(vp, VC_C1SET) && !F_ISSET(vp->kp, V_CNT))
			goto usage;

		/* Check for an illegal buffer. */
		if (!F_ISSET(vp->kp, V_OBUF) && F_ISSET(vp, VC_BUFFER))
			goto usage;

		/*
		 * Handle the [, ] and Z commands.  Doesn't the fact that the
		 * single characters don't have any meaning, but the doubled
		 * characters do, just frost your shorts?
		 */
		if (vp->key == '[' || vp->key == ']' || vp->key == 'Z') {
			vip->cm_state = VS_GET_DOUBLE;
			break;
		}

get_double:	/* Handle the z command's additional arguments. */
		if (vp->key == 'z') {
			vip->cm_state = VS_GET_Z1;
			break;
		}

get_z1:		/* Get any required trailing buffer name. */
		if (F_ISSET(vp->kp, V_RBUF)) {
			vip->cm_state = VS_GET_RBUFFER;
			break;
		}

get_rbuffer:	/* Get any required trailing character. */
		if (F_ISSET(vp->kp, V_CHAR)) {
			vip->cm_state = VS_GET_CHAR;
			break;
		}

get_char:	/* Get any associated cursor word. */
		if (F_ISSET(vp->kp, V_KEYW) && v_keyword(sp))
			return (1);

dot_done:	/* Prepare to set the previous context. */
		if (F_ISSET(vp, V_ABS | V_ABS_C | V_ABS_L)) {
			abs.lno = sp->lno;
			abs.cno = sp->cno;
		}

		/*
		 * Set the three cursor locations to the current cursor.  The
		 * underlying routines don't bother if the cursor doesn't move.
		 * This also handles line commands (e.g. Y) defaulting to the
		 * current line.
		 */
		vp->m_start.lno = vp->m_stop.lno = vp->m_final.lno = sp->lno;
		vp->m_start.cno = vp->m_stop.cno = vp->m_final.cno = sp->cno;

		if (!F_ISSET(vp, V_MOTION))
			goto no_motion;

		/*
		 * If '.' command, use the dot motion, else get the motion
		 * command. 
		 */
		if (F_ISSET(vp, VC_ISDOT)) {
			*vmp = vip->sdotmotion;
			F_SET(vmp, VC_ISDOT);

			/*
			 * Clear any line motion flags, the subsequent motion
			 * isn't always the same, i.e. "/aaa" may or may not
			 * be a line motion.
			 */
			F_CLR(vmp, VM_COMMASK);
		} else {
			memset(vmp, 0, sizeof(VICMD));
			vip->cm_state = VS_GET_MOTION1;
			break;
		}

get_motion1:	/*
		 * At this point we have both the command and the motion, if
		 * any.
		 * 
		 * A count may be provided to both the command and the motion,
		 * in which case the count is multiplicative, e.g. "3y4y" is
		 * the same as "12yy".  This count is provided to the motion
		 * command and not to the regular function.
		 */
		sv_cnt = vmp->count;
		if (F_ISSET(vp, VC_C1SET)) {
			vmp->count *= vp->count;
			F_SET(vmp, VC_C1SET);

			/*
			 * Set flags to restore the original values of the
			 * command structure so dot commands can change the
			 * count values, e.g. "2dw" "3." deletes a total of
			 * five words.
			 */
			F_CLR(vp, VC_C1SET);
			F_SET(vp, VC_C1RESET);
		}

		/*
		 * Some commands can be repeated to indicate the current line.
		 * In this case, or if the command is a "line command", set the
		 * flags appropriately.  If not a doubled command, then run the
		 * function to get the resulting mark.
		 */
		if (vp->key == vmp->key) {
			F_SET(vp, VM_LDOUBLE | VM_LMODE);

			/* Set the origin of the command. */
			vp->m_start.lno = sp->lno;
			vp->m_start.cno = 0;

			/*
			 * Set the end of the command.
			 *
			 * If the current line is missing, i.e. the file is
			 * empty, historic vi permitted a "cc" or "!!" command
			 * to insert text.
			 */
			vp->m_stop.lno = sp->lno + vmp->count - 1;
			if (file_gline(sp, vp->m_stop.lno, &len) == NULL) {
				if (vp->m_stop.lno != 1 ||
				   vp->key != 'c' && vp->key != '!') {
					m.lno = sp->lno;
					m.cno = sp->cno;
					v_eof(sp, &m);
					goto err;
				}
				vp->m_stop.cno = 0;
			} else
				vp->m_stop.cno = len ? len - 1 : 0;
		} else {
			/*
			 * Motion commands can change the underlying movement
			 * (*snarl*).  For example, "l" is illegal at the end
			 * of a line, but "dl" is not.  Set some flags so the
			 * function knows the situation.
			 */
			vmp->rkp = vp->kp;

			/*
			 * XXX
			 * Use yank instead of creating a new motion command,
			 * it's a lot easier.
			 */
			if (vp->kp == &tmotion) {
				tilde_reset = 1;
				vp->kp = &vikeys['y'];
			}

			/*
			 * Copy the key flags into the local structure, EXCEPT
			 * for the RCM flags.  The motion command will set the
			 * RCM flags in the vp structure as necessary.
			 */
			F_SET(vmp, vmp->kp->flags & ~VM_RCM_MASK);

			/*
			 * Set the three cursor locations to the current cursor.
			 * This permits commands like 'j' and 'k', that are line
			 * oriented motions and that have special cursor suck
			 * semantics when they are used as standalone commands,
			 * to ignore column positioning.
			 */
			vmp->m_final.lno =
			    vmp->m_stop.lno = vmp->m_start.lno = sp->lno;
			vmp->m_final.cno =
			    vmp->m_stop.cno = vmp->m_start.cno = sp->cno;

			/* Execute the motion command. */
			state = vip->cm_state;
			if (vmp->kp->func(sp, vmp))
				goto err;

			/*
			 * If the state changed (e.g. the motion command needs
			 * input text), then have to reenter the main loop with
			 * a different state, and jump back here after that's
			 * done.  Note that we reset the current command/motion
			 * structure pointer, for the duration of that effort.
			 * Feel free to call me old-fashioned, but event driven
			 * programming just sucks.
			 */
			vip->vp = &vip->motion;
			if (state != vip->cm_state)
				break;
motion_restart:		vip->vp = &vip->cmd;

			/* If no motion provided, we're done. */
			if (F_ISSET(vmp, VM_CMDFAILED))
				goto err;

			/*
			 * Copy the cut buffer, line mode and cursor position
			 * information from the motion command structure, i.e.
			 * anything that the motion command can set for us.
			 * The commands can flag the movement as a line motion
			 * as well as set the VM_RCM_* flags explicitly.  Note,
			 * only copy the VM_RCM_* flags if they were set, the
			 * original command may have defaults.
			 */
			F_SET(vp, F_ISSET(vmp, VM_COMMASK));
			if (F_ISSET(vmp, VM_RCM_MASK)) {
				F_CLR(vp, VM_RCM_MASK);
				F_SET(vp, F_ISSET(vmp, VM_RCM_MASK));
			}

			/*
			 * Commands can behavior based on the motion command
			 * used, for example, the ! command repeated the last
			 * bang command if N or n was used as the motion.  Set
			 * a pointer so that the command knows what happened.
			 */
			vp->rkp = vmp->kp;
			
			/*
			 * Motion commands can reset the cursor information.
			 * If the motion is in the reverse direction, switch
			 * the from and to MARK's so that it's in a forward
			 * direction.  Motions are from the from MARK to the
			 * to MARK (inclusive).
			 */
			if (vmp->m_start.lno > vmp->m_stop.lno ||
			    vmp->m_start.lno == vmp->m_stop.lno &&
			    vmp->m_start.cno > vmp->m_stop.cno) {
				vp->m_start = vmp->m_stop;
				vp->m_stop = vmp->m_start;
			} else {
				vp->m_start = vmp->m_start;
				vp->m_stop = vmp->m_stop;
			}
			vp->m_final = vmp->m_final;

			/* XXX: See above. */
			if (tilde_reset)
				vp->kp = &tmotion;
		}

		/*
		 * If the command sets dot, save the motion structure.  This
		 * is done here because the motion count may have been changed
		 * above and will eventually be reset.
		 */
		if (F_ISSET(vp->kp, V_DOT)) {
			vip->sdotmotion = *vmp;
			vip->sdotmotion.count = sv_cnt;
		}

no_motion:	/*
		 * If a count is set and the command is line oriented, set the
		 * to MARK here relative to the cursor/from MARK.  This is for
		 * commands that take both counts and motions, i.e. "4yy" and
		 * "y%".  As there's no way the command can know which the user
		 * did, we have to do it here.  (There are commands that are
		 * line oriented and that take counts ("#G", "#H"), for which
		 * this calculation is either completely meaningless or wrong.
		 * Each command must handle that problem itself.
		 */
		if (F_ISSET(vp, VC_C1SET) && F_ISSET(vp, VM_LMODE))
			vp->m_stop.lno += vp->count - 1;

		/* Increment the command count. */
		++sp->ccnt;

#if defined(DEBUG) && defined(COMLOG)
		v_comlog(sp, vp);
#endif
		/* Execute the command. */
		state = vip->cm_state;
		if (vp->kp->func(sp, vp))
			goto err;
#ifdef DEBUG
		/* Make sure no function left the temporary space locked. */
		if (F_ISSET(gp, G_TMP_INUSE)) {
			F_CLR(gp, G_TMP_INUSE);
			msgq(sp, M_ERR,
			    "209|vi: temporary buffer not released");
		}
#endif
		/*
		 * If the state changed (e.g. the command needs input text),
		 * then have to reenter the main loop with a different state,
		 * and jump back here after that's done.
		 */
		if (state != vip->cm_state)
			break;

command_restart:/* If the command failed, we're done. */
		if (F_ISSET(vmp, VM_CMDFAILED))
			goto err;

		/*
		 * Set the dot command structure.
		 *
		 * !!!
		 * Historically, commands which used mapped keys did not
		 * set the dot command, with the exception of the text
		 * input commands.
		 */
		if (F_ISSET(vp, V_DOT) && !F_ISSET(vip, VIP_MAPPED)) {
			vip->sdot = *vp;
			F_SET(&vip->sdot, VC_ISDOT);

			/*
			 * If a count was supplied for both the command and
			 * its motion, the count was used only for the motion.
			 * Turn the count back on for the dot structure.
			 */
			if (F_ISSET(vp, VC_C1RESET))
				F_SET(&vip->sdot, VC_C1SET);

			/* VM flags aren't retained. */
			F_CLR(&vip->sdot, VM_COMMASK | VM_RCM_MASK);
		}

		/*
		 * Some vi row movements are "attracted" to the last position
		 * set, i.e. the VM_RCM commands are moths to the VM_RCM_SET
		 * commands' candle.  It's broken into two parts.  Here we deal
		 * with the command flags.  In sp->relative(), we deal with the
		 * screen flags.  If the movement is to the EOL the vi command
		 * handles it.  If it's to the beginning, we handle it here.
		 *
		 * Note, some commands (e.g. _, ^) don't set the VM_RCM_SETFNB
		 * flag, but do the work themselves.  The reason is that they
		 * have to modify the column in case they're being used as a
		 * motion component.  Other similar commands (e.g. +, -) don't
		 * have to modify the column because they are always line mode
		 * operations when used as motions, so the column number isn't
		 * of any interest.
		 *
		 * Does this totally violate the screen and editor layering?
		 * You betcha.  As they say, if you think you understand it,
		 * you don't.
		 */
		switch (F_ISSET(vp, VM_RCM_MASK)) {
		case 0:
		case VM_RCM_SET:
			break;
		case VM_RCM:
			vp->m_final.cno = vs_rcm(sp,
			    vp->m_final.lno, F_ISSET(vip, VIP_RCM_LAST));
			break;
		case VM_RCM_SETLAST:
			F_SET(vip, VIP_RCM_LAST);
			break;
		case VM_RCM_SETFNB:
			vp->m_final.cno = 0;
			/* FALLTHROUGH */
		case VM_RCM_SETNNB:
			if (nonblank(sp, vp->m_final.lno, &vp->m_final.cno))
				return (1);
			break;
		default:
			abort();
		}

		/* Update the cursor. */
		sp->lno = vp->m_final.lno;
		sp->cno = vp->m_final.cno;

		/*
		 * Set the absolute mark -- set even if a tags or similar
		 * command, since the tag may be moving to the same file.
		 */
		if ((F_ISSET(vp, V_ABS) ||
		    F_ISSET(vp, V_ABS_L) && sp->lno != abs.lno ||
		    F_ISSET(vp, V_ABS_C) &&
		    (sp->lno != abs.lno || sp->cno != abs.cno)) &&
		    mark_set(sp, ABSMARK1, &abs, 1))
			return (1);
		goto cmd;

	/*
	 * VS_GET_MOTION1:
	 *
	 * Initial motion command state.
	 */
	case VS_GET_MOTION1:
		if (evp->e_value == K_ESCAPE)
			goto esc;
		/*
		 * !!!
		 * All commands support specifying a count in the syntax
		 * although all commands don't use it (e.g. 35M is the
		 * same as M alone).  The motion commands always have a
		 * count of 1, it's simpler than tracking if it's set.
		 */
		if (isdigit(evp->e_c) && evp->e_c != '0') {
			FL_SET(gp->ec_flags, EC_MAPNODIGIT);
			F_SET(vmp, VC_C1SET);

			vip->cm_next = VS_GET_MOTION2;
			vip->cm_state = VS_GET_COUNT;
			vip->cm_countp = &vmp->count;
			goto count;
		} else
			vmp->count = 1;
		/* FALLTHROUGH */

	/*
	 * VS_GET_MOTION2:
	 *
	 * Second motion command state.  We have retrieved the count if
	 * specified, and are continuing with the command.
	 */
	case VS_GET_MOTION2:
		if (evp->e_value == K_ESCAPE)
			goto esc;

		/* Got a motion command; check for an OOB command key. */
		if (evp->e_c > MAXVIKEY) {
			v_message(sp, KEY_NAME(sp, evp->e_c), VIM_NOCOM);
			goto err;
		}
		vmp->kp = &vikeys[vmp->key = evp->e_c];

		/* Check for an illegal count. */
		if (F_ISSET(vmp, VC_C1SET) && !F_ISSET(vmp->kp, V_CNT)) {
			vp->kp = vmp->kp;		/* XXX */
			goto usage;
		}

		/*
		 * Commands that have motion components can be doubled to
		 * imply the current line.
		 */
		if (vmp->key != vp->key && !F_ISSET(vmp->kp, V_MOVE)) {
			msgq(sp, M_ERR,
		    "210|%s may not be used as a motion command",
			    KEY_NAME(sp, evp->e_c));
			goto err;
		}

		/* Handle the [ and ] commands. */
		if (vmp->key == '[' || vmp->key == ']') {
			vip->cm_state = VS_GET_MDOUBLE;
			break;
		}

get_mdouble:	/* Get any required trailing character. */
		if (F_ISSET(vmp->kp, V_CHAR)) {
			vip->cm_state = VS_GET_MCHAR;
			break;
		}

get_mchar:	/* Get any associated cursor word. */
		if (F_ISSET(vmp->kp, V_KEYW) && v_keyword(sp))
			return (1);
		goto get_motion1;

	/*
	 * VS_GET_CHAR:
	 *	The user has to enter a character for the command.  Get
	 *	it and jump back to continue with the command.
	 *
	 * VS_GET_MCHAR:
	 *	The user has to enter a character for a motion command.
	 *	Get it and jump back to continue with the command.
	 */
	case VS_GET_CHAR:
		if (evp->e_value == K_ESCAPE)
			goto esc;

		vp->character = evp->e_c;
		goto get_char;
		break;
	case VS_GET_MCHAR:
		if (evp->e_value == K_ESCAPE)
			goto esc;

		vmp->character = evp->e_c;
		goto get_mchar;
		break;

	/*
	 * VS_GET_BUFFER:
	 *	The user entered a " indicating a buffer name followed.
	 *	Get the buffer name and then continue with the command.
	 *
	 * VS_GET_RBUFFER:
	 *	The user has to enter a buffer name.  Get it and jump
	 *	back to continue with the command.
	 */
	case VS_GET_BUFFER:
		if (evp->e_value == K_ESCAPE)
			goto esc;

		F_SET(vp, VC_BUFFER);
		vp->buffer = evp->e_c;
		vip->cm_state = vip->cm_next;
		break;
	case VS_GET_RBUFFER:
		if (evp->e_value == K_ESCAPE)
			goto esc;

		F_SET(vp, VC_BUFFER);
		vp->buffer = evp->e_c;
		goto get_rbuffer;

	/*
	 * VS_GET_COUNT:
	 *	The user entered a digit indicating a count followed.  Get
	 *	the count and then continue with the command.  We get a
	 *	special non-digit at the end of a count, so simply discard
	 *	that event and continue with the next event.
	 *
	 * VS_GET_COUNT_DISCARD:
	 *	We were getting a count but it overflowed.  Discard events
	 *	until we get the non-digit ending the count, and then call
	 *	it an error.
	 */
	case VS_GET_COUNT:
		if (evp->e_value == K_ESCAPE)
			goto esc;

		if (!isdigit(evp->e_c)) {
			FL_CLR(gp->ec_flags, EC_MAPNODIGIT);
			vip->cm_state = vip->cm_next;
			break;
		}

count:		tmp = *vip->cm_countp;
		*vip->cm_countp = *vip->cm_countp * 10;

		/* Don't assume the characters are ordered. */
		switch (evp->e_c) {
		case '1': *vip->cm_countp += 1; break;
		case '2': *vip->cm_countp += 2; break;
		case '3': *vip->cm_countp += 3; break;
		case '4': *vip->cm_countp += 4; break;
		case '5': *vip->cm_countp += 5; break;
		case '6': *vip->cm_countp += 6; break;
		case '7': *vip->cm_countp += 7; break;
		case '8': *vip->cm_countp += 8; break;
		case '9': *vip->cm_countp += 9; break;
		}

		/* Assume that overflow results in a smaller number. */
		if (tmp >= *vip->cm_countp)
			vip->cm_state = VS_GET_COUNT_DISCARD;
		break;
	case VS_GET_COUNT_DISCARD:
		if (!isdigit(evp->e_c)) {
			msgq(sp, M_ERR,
			    "211|Number larger than %lu", ULONG_MAX);
			goto err;
		}
		break;

	/*
	 * VS_GET_DOUBLE, VS_GET_MDOUBLE:
	 *
	 * The user entered half a [[, ]] or ZZ command, or half a [[ or ]]
	 * command as a motion command.  Get the other half and jump back to
	 * the command.
	 */
	case VS_GET_DOUBLE:
		/*
		 * Historically, half entered [[, ]] or ZZ commands were not
		 * cancelled by <escape>, the terminal was beeped instead.
		 * POSIX.2-1992 probably didn't notice, and requires they be
		 * cancelled instead of beeping.  Seems right to me.
		 */
		if (evp->e_value == K_ESCAPE)
			goto esc;

		if (vp->key != evp->e_c)
			goto usage;
		goto get_double;
	case VS_GET_MDOUBLE:
		/* See the VS_GET_DOUBLE comment above. */
		if (evp->e_value == K_ESCAPE)
			goto esc;

		if (vmp->key != evp->e_c)
			goto usage;
		goto get_mdouble;

	/*
	 * VS_GET_Z1, VS_GET_Z2:
	 *
	 * The user entered a z command, which is optionally followed by a
	 * count and then by a flag character.  Get a count if the next
	 * character is a digit, then the flag character, else just return
	 * with the flag character.
	 */
	case VS_GET_Z1:
		if (evp->e_value == K_ESCAPE)
			goto esc;

		if (isdigit(evp->e_c)) {
			FL_SET(gp->ec_flags, EC_MAPNODIGIT);
			F_SET(vp, VC_C2SET);

			vip->cm_next = VS_GET_Z2;
			vip->cm_state = VS_GET_COUNT;
			vip->cm_countp = &vp->count2;
			goto count;
		}
		/* FALLTHROUGH */
	case VS_GET_Z2:
		if (evp->e_value == K_ESCAPE)
			goto esc;

		vp->character = evp->e_c;
		goto get_z1;
	default:
		abort();
	}
	goto done;
	
	if (0) {
usage:		v_message(sp, vp->key == '~' &&
		    O_ISSET(sp, O_TILDEOP) ? tmotion.usage : vp->kp->usage,
		    VIM_USAGE);
		goto err;
	}
	if (0) {
esc:		if (!F_ISSET(vip, VIP_PARTIAL))
			(void)gp->scr_bell(sp);
		goto err;
	}
	if (0) {
err:		if (v_event_flush(sp, CH_MAPPED))
			msgq(sp, M_ERR,
			    "212|Vi command failed: mapped keys discarded");
		goto cmd;
	}

	if (0) {
cmd:		vp = vip->vp = &vip->cmd;
		vip->cm_state = VS_GET_CMD1;
		sp->showmode = SM_COMMAND;
		FL_CLR(gp->ec_flags, EC_MAPINPUT);
		FL_SET(gp->ec_flags, EC_MAPCOMMAND);
	}

	/* Clear any interrupt, it's been dealt with. */
	F_CLR(sp, S_INTERRUPTED);

done:	/* If we're exiting the screen, clean up. */
	switch (F_ISSET(sp, S_EX | S_EXIT | S_EXIT_FORCE | S_SSWITCH)) {
	case S_EX:				/* Exit vi mode. */
		v_dtoh(sp);
		vip->cm_state = VS_GET_CMD1;
		return (0);
	case S_EXIT:				/* Exit the file. */
	case S_EXIT_FORCE:			/* Exit the file. */
		if (file_end(sp, NULL, F_ISSET(sp, S_EXIT_FORCE)))
			break;
		/*
		 * !!!
		 * NB: sp->frp may now be NULL, if it was a tmp file.
		 *
		 * Find a new screen, either by joining an old one or by
		 * switching with a hidden one.
		 */
		(void)vs_discard(sp, &tsp);
		if (tsp == NULL)
			(void)vs_swap(sp, &tsp, NULL);
		if ((sp->nextdisp = tsp) == NULL)
			return (0);
		goto sswitch;
	case S_SSWITCH:				/* Exit the screen. */
		/* Save the old screen's cursor information. */
		sp->frp->lno = sp->lno;
		sp->frp->cno = sp->cno;
		F_SET(sp->frp, FR_CURSORSET);

		/* Display a status line. */
		F_SET(sp, S_STATUS);

sswitch:	vip->cm_state = VS_GET_CMD1;

		/* Refresh based on the new screen, so the cursor's right. */
		F_SET(vip, VIP_CUR_INVALID);
		return (vs_refresh(sp->nextdisp));
	}

	/* If no mapped keys waiting, refresh the screen. */
	if (!MAPPED_KEYS_WAITING(sp))
		if (F_ISSET(vip, VIP_SKIPREFRESH))
			F_CLR(vip, VIP_SKIPREFRESH);
		else {
			if (vs_refresh(sp))
				return (1);

			/* Set the new favorite position. */
			if (F_ISSET(vp,
			    VM_RCM_SET | VM_RCM_SETFNB | VM_RCM_SETNNB)) {
				F_CLR(vip, VIP_RCM_LAST);
				(void)vs_column(sp, &sp->rcm);
			}
		}
	return (0);
}

/*
 * v_alias --
 *	Check for a command alias.
 */
static int
v_alias(sp, keyp)
	SCR *sp;
	CHAR_T *keyp;
{
	CHAR_T push;

	switch (*keyp) {
	case 'C':			/* C -> c$ */
		push = '$';
		*keyp = 'c';
		break;
	case 'D':			/* D -> d$ */
		push = '$';
		*keyp = 'd';
		break;
	case 'S':			/* S -> c_ */
		push = '_';
		*keyp = 'c';
		break;
	case 'Y':			/* Y -> y_ */
		push = '_';
		*keyp = 'y';
		break;
	default:
		return (0);
	}
	if (v_event_push(sp, &push, 1, CH_NOMAP | CH_QUOTED))
		return (1);
	return (0);
}

/*
 * v_keyword --
 *	Get the word (or non-word) the cursor is on.
 */
static int
v_keyword(sp)
	SCR *sp;
{
	VI_PRIVATE *vip;
	size_t beg, end, len;
	int moved, state;
	char *p;

	if ((p = file_gline(sp, sp->lno, &len)) == NULL)
		goto err;

	/*
	 * !!!
	 * Historically, tag commands skipped over any leading whitespace
	 * characters.  Make this true in general when using cursor words.
	 * If movement, getting a cursor word implies moving the cursor to
	 * its beginning.  Refresh now.
	 *
	 * !!!
	 * Find the beginning/end of the keyword.  Keywords are currently
	 * used for cursor-word searching and for tags.  Historical vi
	 * only used the word in a tag search from the cursor to the end
	 * of the word, i.e. if the cursor was on the 'b' in " abc ", the
	 * tag was "bc".  For consistency, we make cursor word searches
	 * follow the same rule.
	 */
	for (moved = 0,
	    beg = sp->cno; beg < len && isspace(p[beg]); moved = 1, ++beg);
	if (beg >= len) {
err:		msgq(sp, M_BERR, "213|Cursor not in a word");
		return (1);
	}
	if (moved) {
		sp->cno = beg;
		(void)vs_refresh(sp);
	}

	/* Find the end of the word. */
	for (state = inword(p[beg]),
	    end = beg; ++end < len && state == inword(p[end]););
	--end;

	vip = VIP(sp);
	len = (end - beg) + 2;				/* XXX */
	vip->klen = (end - beg) + 1;
	BINC_RET(sp, vip->keyw, vip->keywlen, len);
	memmove(vip->keyw, p + beg, vip->klen);
	vip->keyw[vip->klen] = '\0';			/* XXX */
	return (0);
}

/*
 * v_scr_init --
 *	Initialize the vi screen.
 */
static int
v_scr_init(sp)
	SCR *sp;
{
	VI_PRIVATE *vip;

	vip = VIP(sp);

	/* Invalidate the cursor, the line size cache. */
	VI_SCR_CFLUSH(vip);
	F_SET(vip, VIP_CUR_INVALID);

	/* Initialize terminal values. */
	vip->srows = O_VAL(sp, O_LINES);

	/*
	 * Initialize screen values.
	 *
	 * Small windows: see vs_refresh(), section 6a.
	 *
	 * Setup:
	 *	t_minrows is the minimum rows to display
	 *	t_maxrows is the maximum rows to display (rows - 1)
	 *	t_rows is the rows currently being displayed
	 */
	sp->rows = vip->srows;
	sp->cols = O_VAL(sp, O_COLUMNS);
	sp->t_rows = sp->t_minrows = O_VAL(sp, O_WINDOW);
	if (sp->rows != 1) {
		if (sp->t_rows > sp->rows - 1) {
			sp->t_minrows = sp->t_rows = sp->rows - 1;
			msgq(sp, M_INFO,
			    "214|Windows option value is too large, max is %u",
			    sp->t_rows);
		}
		sp->t_maxrows = sp->rows - 1;
	} else
		sp->t_maxrows = 1;

	/* Create a screen map. */
	CALLOC(sp, HMAP, SMAP *, SIZE_HMAP(sp), sizeof(SMAP));
	if (HMAP == NULL)
		return (1);
	TMAP = HMAP + (sp->t_rows - 1);

	/*
	 * Fill the current screen's map, incidentally losing any
	 * vs_line() cached information.
	 */
	if (vs_sm_fill(sp, sp->lno,
	    F_ISSET(sp, S_SCR_TOP) ? P_TOP :
	    F_ISSET(sp, S_SCR_CENTER) ? P_MIDDLE : P_FILL))
		return (1);

	F_SET(sp, S_SCR_REFORMAT);
	return (0);
}

/*
 * v_dtoh --
 *	Move all but the current screen to the hidden queue.
 */
static void
v_dtoh(sp)
	SCR *sp;
{
	GS *gp;
	SCR *tsp;
	int hidden;

	/* Move all screens to the hidden queue, tossing screen maps. */
	for (hidden = 0, gp = sp->gp;
	    (tsp = gp->dq.cqh_first) != (void *)&gp->dq; ++hidden) {
		if (_HMAP(tsp) != NULL) {
			FREE(_HMAP(tsp), SIZE_HMAP(tsp) * sizeof(SMAP));
			_HMAP(tsp) = NULL;
		}
		SIGBLOCK(gp);
		CIRCLEQ_REMOVE(&gp->dq, tsp, q);
		CIRCLEQ_INSERT_TAIL(&gp->hq, tsp, q);
		SIGUNBLOCK(gp);
	}

	/* Move current screen back to the display queue. */
	SIGBLOCK(gp);
	CIRCLEQ_REMOVE(&gp->hq, sp, q);
	CIRCLEQ_INSERT_TAIL(&gp->dq, sp, q);
	SIGUNBLOCK(gp);

	/*
	 * XXX
	 * Don't bother internationalizing this message, it's going to
	 * go away as soon as we have one-line screens.  --TK
	 */
	if (hidden > 1)
		msgq(sp, M_INFO,
		    "%d screens backgrounded; use :display to list them",
		    hidden - 1);
}

#if defined(DEBUG) && defined(COMLOG)
/*
 * v_comlog --
 *	Log the contents of the command structure.
 */
static void
v_comlog(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	TRACE(sp, "vcmd: %c", vp->key);
	if (F_ISSET(vp, VC_BUFFER))
		TRACE(sp, " buffer: %c", vp->buffer);
	if (F_ISSET(vp, VC_C1SET))
		TRACE(sp, " c1: %lu", vp->count);
	if (F_ISSET(vp, VC_C2SET))
		TRACE(sp, " c2: %lu", vp->count2);
	TRACE(sp, " flags: 0x%x\n", vp->flags);
}
#endif
