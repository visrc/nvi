/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vi.c,v 9.1 1994/11/09 18:36:40 bostic Exp $ (Berkeley) $Date: 1994/11/09 18:36:40 $";
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

#include "vi.h"
#include "vcmd.h"

enum gcret { GC_ERR, GC_ERR_NOFLUSH, GC_OK } gcret;
static enum gcret getcmd
	__P((SCR *, VICMDARG *, VICMDARG *, VICMDARG *, int *, int *));
static __inline int
	   getcount __P((SCR *, ARG_CHAR_T, u_long *));
static __inline int
	   getkey __P((SCR *, CH *, u_int));
static int getkeyword __P((SCR *, VICMDARG *, u_int));
static int getmotion __P((SCR *, VICMDARG *, VICMDARG *, int *));

/*
 * Side-effect:
 *	The dot structure can be set by the underlying vi functions,
 *	see v_Put() and v_put().
 */
#define	DOT		(&VIP(sp)->sdot)
#define	DOTMOTION	(&VIP(sp)->sdotmotion)

/*
 * vi --
 * 	Main vi command loop.
 */
int
vi(sp)
	SCR *sp;
{
	MARK abs;
	VICMDARG cmd, *vp;
	u_int flags, saved_mode;
	int comcount, eval, mapped;

	/* Start vi and paint the screen. */
	if (v_init(sp))
		return (1);

	/* Initialize the command structure. */
	memset(&cmd, 0, sizeof(VICMDARG));

	for (eval = 0, vp = &cmd;;) {
		/* Refresh the screen. */
		sp->showmode = "Command";
		if (sp->s_refresh(sp)) {
			eval = 1;
			break;
		}

		/* Set the new favorite position. */
		if (F_ISSET(vp, VM_RCM_SET | VM_RCM_SETFNB | VM_RCM_SETNNB)) {
			sp->rcm_last = 0;
			(void)sp->s_column(sp, &sp->rcm);
		}

		/*
		 * If not currently in a map, log the cursor position,
		 * and set a flag so that this command can become the
		 * DOT command.
		 */
		if (MAPPED_KEYS_WAITING(sp))
			mapped = 1;
		else {
			if (log_cursor(sp))
				goto err;
			mapped = 0;
		}

		/*
		 * We get a command, which may or may not have an associated
		 * motion.  If it does, we get it too, calling its underlying
		 * function to get the resulting mark.  We then call the
		 * command setting the cursor to the resulting mark.
		 *
		 * Refresh the command structure.
		 */
		memset(&vp->vp_startzero, 0,
		    (u_int8_t *)&vp->vp_endzero -
		    (u_int8_t *)&vp->vp_startzero);
		/*
		 * !!!
		 * Vi historically flushed mapped characters on error, but
		 * entering extra <escape> characters at the beginning of
		 * a map wasn't considered an error -- in fact, users would
		 * put leading <escape> characters in maps to clean up vi
		 * state before the map was interpreted.
		 */
		switch (getcmd(sp, DOT, vp, NULL, &comcount, &mapped)) {
		case GC_ERR:
			goto err;
		case GC_ERR_NOFLUSH:
			goto enoflush;
		case GC_OK:
			break;
		}

		/*
		 * Historical practice: if a dot command gets a new count,
		 * any motion component goes away, i.e. "d3w2." deletes a
		 * total of 5 words.
		 */
		if (F_ISSET(vp, VC_ISDOT) && comcount)
			DOTMOTION->count = 1;

		/* Copy the key flags into the local structure. */
		F_SET(vp, vp->kp->flags);

		/* Prepare to set the previous context. */
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

		/*
		 * Do any required motion; getmotion sets the from MARK and the
		 * line mode flag.  We save off the RCM mask and only restore
		 * it if it no RCM flags are set by the motion command.  This
		 * means that the motion command is expected to determine where
		 * the cursor ends up!
		 */
		if (F_ISSET(vp, V_MOTION)) {
			flags = F_ISSET(vp, VM_RCM_MASK);
			F_CLR(vp, VM_RCM_MASK);
			if (getmotion(sp, DOTMOTION, vp, &mapped))
				goto err;
			if (F_ISSET(vp, VM_NOMOTION))
				goto err;
			if (!F_ISSET(vp, VM_RCM_MASK))
				F_SET(vp, flags);
		}

		/*
		 * If a count is set and the command is line oriented, set the
		 * to MARK here relative to the cursor/from MARK.  This is for
		 * commands that take both counts and motions, i.e. "4yy" and
		 * "y%".  As there's no way the command can know which the user
		 * did, we have to do it here.  (There are commands that are
		 * line oriented and that take counts ("#G", "#H"), for which
		 * this calculation is either completely meaningless or wrong.
		 * Each command must validate the value for itself.
		 */
		if (F_ISSET(vp, VC_C1SET) && F_ISSET(vp, VM_LMODE))
			vp->m_stop.lno += vp->count - 1;

		/* Increment the command count. */
		++sp->ccnt;

		/* Save the mode and call the function. */
		saved_mode = F_ISSET(sp, S_SCREENS | S_MAJOR_CHANGE);
		if ((vp->kp->func)(sp, vp))
			goto err;
#ifdef DEBUG
		/* Make sure no function left the temporary space locked. */
		if (F_ISSET(sp->gp, G_TMP_INUSE)) {
			msgq(sp, M_ERR,
			    "202|vi: temporary buffer not released");
			return (1);
		}
#endif
		/*
		 * If that command took us out of vi or changed the screen,
		 * then exit the loop without further action.
		 */
		 if (saved_mode != F_ISSET(sp, S_SCREENS | S_MAJOR_CHANGE))
			break;

		/*
		 * Set the dot command structure.
		 *
		 * !!!
		 * Historically, commands which used mapped keys did not
		 * set the dot command, with the exception of the text
		 * input commands.
		 */
		if (F_ISSET(vp, V_DOT) && !mapped) {
			*DOT = cmd;
			F_SET(DOT, VC_ISDOT);

			/*
			 * If a count was supplied for both the command and
			 * its motion, the count was used only for the motion.
			 * Turn the count back on for the dot structure.
			 */
			if (F_ISSET(vp, VC_C1RESET))
				F_SET(DOT, VC_C1SET);

			/* VM flags aren't retained. */
			F_CLR(DOT, VM_COMMASK | VM_RCM_MASK);
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
			vp->m_final.cno = sp->s_rcm(sp, vp->m_final.lno);
			break;
		case VM_RCM_SETLAST:
			sp->rcm_last = 1;
			break;
		case VM_RCM_SETFNB:
			vp->m_final.cno = 0;
			/* FALLTHROUGH */
		case VM_RCM_SETNNB:
			if (nonblank(sp, vp->m_final.lno, &vp->m_final.cno))
				goto err;
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
			goto err;

		if (!MAPPED_KEYS_WAITING(sp)) {
			if (0) {
err:				term_flush(sp, "Vi error", CH_MAPPED);
			}
enoflush:		(void)msg_rpt(sp, 1);
		}

		/*
		 * Check and clear the interrupts.  There's an obvious race,
		 * but it's not worth cleaning up.  This is done after the
		 * err: lable, so that if the "error" was an interupt it gets
		 * cleaned up.
		 */
		if (INTERRUPTED(sp))
			term_flush(sp, "Interrupted", CH_MAPPED);
		CLR_INTERRUPT(sp);
	}

	/* Free allocated keyword memory. */
	if (cmd.keyword != NULL)
		free(cmd.keyword);

	return (v_end(sp) || eval);
}

#define	KEY(key, map) {							\
	if (getkey(sp, &ikey, map))					\
		return (1);						\
	if (ikey.value == K_ESCAPE)					\
		goto esc;						\
	if (F_ISSET(&ikey, CH_MAPPED))					\
		*mappedp = 1;						\
	key = ikey.ch;							\
}

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
 * getcmd --
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
 */
static enum gcret
getcmd(sp, dp, vp, ismotion, comcountp, mappedp)
	SCR *sp;
	VICMDARG *dp, *vp;
	VICMDARG *ismotion;	/* Previous key if getting motion component. */
	int *comcountp, *mappedp;
{
	enum { COMMANDMODE, ISPARTIAL, NOTPARTIAL } cpart;
	VIKEYS const *kp;
	u_int flags;
	CH ikey;
	CHAR_T key;
	char *s;

	/*
	 * Get a key.
	 *
	 * <escape> cancels partial commands, i.e. a command where at least
	 * one non-numeric character has been entered.  Otherwise, it beeps
	 * the terminal.
	 *
	 * !!!
	 * POSIX 1003.2-1992 explicitly disallows cancelling commands where
	 * all that's been entered is a number, requiring that the terminal
	 * be alerted.
	 */
	cpart = ismotion == NULL ? COMMANDMODE : ISPARTIAL;
	KEY(key, TXT_MAPCOMMAND);
	if (ismotion == NULL)
		cpart = NOTPARTIAL;

	/* Pick up optional buffer. */
	if (key == '"') {
		cpart = ISPARTIAL;
		if (ismotion != NULL) {
			msgq(sp, M_BERR,
		    "203|Buffers should be specified before the command");
			return (GC_ERR);
		}
		KEY(vp->buffer, 0);
		F_SET(vp, VC_BUFFER);

		KEY(key, TXT_MAPCOMMAND);
	}

	/*
	 * Pick up optional count, where a leading 0 is not a count,
	 * it's a command.
	 */
	if (isdigit(key) && key != '0') {
		if (getcount(sp, key, &vp->count))
			return (GC_ERR);
		F_SET(vp, VC_C1SET);
		*comcountp = 1;

		KEY(key, TXT_MAPCOMMAND);
	} else
		*comcountp = 0;

	/* Pick up optional buffer. */
	if (key == '"') {
		cpart = ISPARTIAL;
		if (F_ISSET(vp, VC_BUFFER)) {
			msgq(sp, M_ERR, "204|Only one buffer can be specified");
			return (GC_ERR);
		}
		if (ismotion != NULL) {
			msgq(sp, M_BERR,
		    "205|Buffers should be specified before the command");
			return (GC_ERR);
		}
		KEY(vp->buffer, 0);
		F_SET(vp, VC_BUFFER);

		KEY(key, TXT_MAPCOMMAND);
	}

	/* Check for an OOB command key. */
	cpart = ISPARTIAL;
	if (key > MAXVIKEY) {
		msgq(sp, M_BERR,
		    "206|%s isn't a vi command", KEY_NAME(sp, key));
		return (GC_ERR);
	}
	kp = &vikeys[vp->key = key];

	/* The tildeop option makes the ~ command take a motion. */
	if (key == '~' && O_ISSET(sp, O_TILDEOP))
		kp = &tmotion;

	vp->kp = kp;

	/*
	 * Find the command.  The only legal command with no underlying
	 * function is dot.  It's historic practice that <escape> doesn't
	 * just erase the preceding number, it beeps the terminal as well.
	 * It's a common problem, so just beep the terminal unless verbose
	 * was set.
	 */
	if (kp->func == NULL) {
		if (key != '.') {
			msgq(sp, ikey.value == K_ESCAPE ? M_BERR : M_ERR,
			    "207|%s isn't a vi command", KEY_NAME(sp, key));
			return (GC_ERR);
		}

		/* If called for a motion command, stop now. */
		if (dp == NULL)
			goto usage;

		/* A repeatable command must have been executed. */
		if (!F_ISSET(dp, VC_ISDOT)) {
			msgq(sp, M_ERR, "208|No command to repeat");
			return (GC_ERR);
		}

		/*
		 * !!!
		 * If a '.' is immediately entered after an undo command, we
		 * replay the log instead of redoing the last command.  This
		 * is necessary because 'u' can't set the dot command -- see
		 * vi/v_undo.c:v_undo for details.
		 */
		if (VIP(sp)->u_ccnt == sp->ccnt) {
			vp->kp = &vikeys['u'];
			F_SET(vp, VC_ISDOT);
			return (GC_OK);
		}

		/* Set new count/buffer, if any, and return. */
		if (F_ISSET(vp, VC_C1SET)) {
			F_SET(dp, VC_C1SET);
			dp->count = vp->count;
		}
		if (F_ISSET(vp, VC_BUFFER))
			dp->buffer = vp->buffer;
		*vp = *dp;
		return (GC_OK);
	}

	/* Set the flags based on the command flags. */
	flags = kp->flags;

	/* Check for illegal count. */
	if (F_ISSET(vp, VC_C1SET) && !LF_ISSET(V_CNT))
		goto usage;

	/* Illegal motion command. */
	if (ismotion == NULL) {
		/* Illegal buffer. */
		if (!LF_ISSET(V_OBUF) && F_ISSET(vp, VC_BUFFER))
			goto usage;

		/* Required buffer. */
		if (LF_ISSET(V_RBUF)) {
			KEY(vp->buffer, 0);
			F_SET(vp, VC_BUFFER);
		}
	}

	/*
	 * Special case: '[', ']' and 'Z' commands.  Doesn't the fact that
	 * the *single* characters don't mean anything but the *doubled*
	 * characters do just frost your shorts?
	 */
	if (vp->key == '[' || vp->key == ']' || vp->key == 'Z') {
		/*
		 * Historically, half entered [[, ]] or Z commands weren't
		 * cancelled by <escape>, the terminal was beeped instead.
		 * POSIX.2-1992 probably didn't notice, and requires that
		 * they be cancelled instead of beeping.  Seems fine to me.
		 */
		KEY(key, TXT_MAPCOMMAND);

		if (vp->key != key) {
usage:			if (ismotion == NULL)
				s = kp->usage;
			else if (ismotion->key == '~' && O_ISSET(sp, O_TILDEOP))
				s = tmotion.usage;
			else
				s = vikeys[ismotion->key].usage;
			msgq(sp, M_ERR, "209|Usage: %s", s);
			return (GC_ERR);
		}
	}
	/* Special case: 'z' command. */
	if (vp->key == 'z') {
		KEY(vp->character, 0);
		if (isdigit(vp->character)) {
			if (getcount(sp, vp->character, &vp->count2))
				return (GC_ERR);
			F_SET(vp, VC_C2SET);
			KEY(vp->character, 0);
		}
	}

	/*
	 * Commands that have motion components can be doubled to
	 * imply the current line.
	 */
	if (ismotion != NULL && ismotion->key != key && !LF_ISSET(V_MOVE)) {
		msgq(sp, M_ERR, "210|%s may not be used as a motion command",
		    KEY_NAME(sp, key));
		return (GC_ERR);
	}

	/* Required character. */
	if (LF_ISSET(V_CHAR))
		KEY(vp->character, 0);

	/* Get any associated keyword. */
	if (F_ISSET(kp, V_KEYNUM | V_KEYW) &&
	    getkeyword(sp, vp, F_ISSET(kp, V_KEYNUM | V_KEYW)))
		return (GC_ERR);

	return (GC_OK);

esc:	switch (cpart) {
	case COMMANDMODE:
		msgq(sp, M_BERR, "211|Already in command mode");
		return (GC_ERR_NOFLUSH);
	case ISPARTIAL:
		break;
	case NOTPARTIAL:
		(void)sp->s_bell(sp);
		break;
	}
	return (GC_ERR);
}

/*
 * getmotion --
 *
 * Get resulting motion mark.
 */
static int
getmotion(sp, dm, vp, mappedp)
	SCR *sp;
	VICMDARG *dm, *vp;
	int *mappedp;
{
	MARK m;
	VICMDARG motion;
	size_t len;
	u_long cnt;
	int notused, rval;

	/*
	 * If '.' command, use the dot motion, else get the motion command.
	 * Clear any line motion flags, the subsequent motion isn't always
	 * the same, i.e. "/aaa" may or may not be a line motion.
	 */
	if (F_ISSET(vp, VC_ISDOT)) {
		motion = *dm;
		F_SET(&motion, VC_ISDOT);
		F_CLR(&motion, VM_COMMASK);
	} else {
		memset(&motion, 0, sizeof(VICMDARG));
		if (getcmd(sp, NULL, &motion, vp, &notused, mappedp) != GC_OK)
			return (1);
	}

	/*
	 * A count may be provided both to the command and to the motion, in
	 * which case the count is multiplicative.  For example, "3y4y" is the
	 * same as "12yy".  This count is provided to the motion command and
	 * not to the regular function.
	 */
	cnt = motion.count = F_ISSET(&motion, VC_C1SET) ? motion.count : 1;
	if (F_ISSET(vp, VC_C1SET)) {
		motion.count *= vp->count;
		F_SET(&motion, VC_C1SET);

		/*
		 * Set flags to restore the original values of the command
		 * structure so dot commands can change the count values,
		 * e.g. "2dw" "3." deletes a total of five words.
		 */
		F_CLR(vp, VC_C1SET);
		F_SET(vp, VC_C1RESET);
	}

	/*
	 * Some commands can be repeated to indicate the current line.  In
	 * this case, or if the command is a "line command", set the flags
	 * appropriately.  If not a doubled command, run the function to get
	 * the resulting mark.
 	 */
	if (vp->key == motion.key) {
		F_SET(vp, VM_LDOUBLE | VM_LMODE);

		/* Set the origin of the command. */
		vp->m_start.lno = sp->lno;
		vp->m_start.cno = 0;

		/*
		 * Set the end of the command.
		 *
		 * If the current line is missing, i.e. the file is empty,
		 * historic vi permitted a "cc" or "!!" command to insert
		 * text.
		 */
		vp->m_stop.lno = sp->lno + motion.count - 1;
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
		 * Motion commands change the underlying movement (*snarl*).
		 * For example, "l" is illegal at the end of a line, but "dl"
		 * is not.  Set flags so the function knows the situation.
		 */
		motion.rkp = vp->kp;

		/*
		 * XXX
		 *
		 * THIS IS HORRIBLY WRONG!!!
		 *
		 * Use yank instead of creating a new motion command, it's a
		 * lot easier for now.  This MUST be fixed when yank is fixed
		 * to quit doing all the bizarre checks against the line having
		 * changed.  The checks are NOT historic practice, it only
		 * looks that way!
		 */
		if (vp->kp == &tmotion)
			vp->kp = &vikeys['y'];

		/*
		 * Copy the key flags into the local structure, except for
		 * the RCM flags, the motion command will set the RCM flags
		 * in the vp structure as necessary.
		 */
		F_SET(&motion, motion.kp->flags & ~VM_RCM_MASK);

		/*
		 * Set the three cursor locations to the current cursor.  This
		 * permits commands like 'j' and 'k', that are line oriented
		 * motions and have special cursor suck semantics when they are
		 * used as standalone commands, to ignore column positioning.
		 */
		motion.m_final.lno =
		    motion.m_stop.lno = motion.m_start.lno = sp->lno;
		motion.m_final.cno =
		    motion.m_stop.cno = motion.m_start.cno = sp->cno;

		/* Run the function. */
		if ((motion.kp->func)(sp, &motion))
			goto err;

		/*
		 * Copy cut buffer, line mode and cursor position information
		 * from the motion command structure, i.e. anything that the
		 * motion command can set for us.  The commands can flag the
		 * movement as a line motion (see v_sentence) as well as set
		 * the VM_RCM_* flags explicitly.
		 */
		F_SET(vp, F_ISSET(&motion, VM_COMMASK | VM_RCM_MASK));

		/*
		 * Commands can change behaviors based on the motion command
		 * used, for example, the ! command repeated the last bang
		 * command if N or n was used as the motion.
		 */
		vp->rkp = motion.kp;
		
		/*
		 * Motion commands can reset all of the cursor information.
		 * If the motion is in the reverse direction, switch the
		 * from and to MARK's so that it's in a forward direction.
		 * Motions are from the from MARK to the to MARK (inclusive).
		 */
		if (motion.m_start.lno > motion.m_stop.lno ||
		    motion.m_start.lno == motion.m_stop.lno &&
		    motion.m_start.cno > motion.m_stop.cno) {
			vp->m_start = motion.m_stop;
			vp->m_stop = motion.m_start;
		} else {
			vp->m_start = motion.m_start;
			vp->m_stop = motion.m_stop;
		}
		vp->m_final = motion.m_final;
	}

	/*
	 * If the command sets dot, save the motion structure.  The motion
	 * count was changed above and needs to be reset, that's why this
	 * is done here, and not in the calling routine.
	 */
	if (F_ISSET(vp->kp, V_DOT)) {
		*dm = motion;
		dm->count = cnt;
	}

	rval = 0;
	if (0) {
err:		rval = 1;
	}

	/* Free allocated keyword memory. */
	if (motion.keyword != NULL)
		free(motion.keyword);

	return (rval);
}

#define	innum(c)	(isdigit(c) || strchr("abcdefABCDEF", c))

/*
 * getkeyword --
 *	Get the "word" the cursor is on.
 */
static int
getkeyword(sp, vp, flags)
	SCR *sp;
	VICMDARG *vp;
	u_int flags;
{
	recno_t lno;
	size_t beg, end, len;
	char *p;

	if ((p = file_gline(sp, sp->lno, &len)) == NULL) {
		if (file_lline(sp, &lno))
			return (1);
		if (lno == 0)
			v_eof(sp, NULL);
		else
			GETLINE_ERR(sp, sp->lno);
		return (1);
	}

	/*
	 * !!!
	 * Historically, tag commands skipped over any leading whitespace
	 * characters.
	 */
	for (beg = sp->cno; beg < len && isspace(p[beg]); ++beg);

	if (beg >= len ||
	    LF_ISSET(V_KEYW) && !inword(p[beg]) ||
	    LF_ISSET(V_KEYNUM) && !innum(p[beg]) &&
	    p[beg] != '-' && p[beg] != '+')
		goto noword;

	/*
	 * !!!
	 * Find the beginning/end of the keyword.  Keywords (V_KEYW) are
	 * used for cursor-word searching and for tags.  Historical vi
	 * only used the word in a tag search from the cursor to the end
	 * of the word, i.e. if the cursor was on the 'b' in " abc ", the
	 * tag was "bc".  For no particular reason, we make the cursor
	 * word searches follow the same rule.
	 */
	if (beg != 0)
		if (LF_ISSET(V_KEYW)) {
#ifdef	MOVE_TO_KEYWORD_BEGINNING
			for (;;) {
				--beg;
				if (!inword(p[beg])) {
					++beg;
					break;
				}
				if (beg == 0)
					break;
			}
#endif
		} else {
			for (;;) {
				--beg;
				if (!innum(p[beg])) {
					if (beg > 0 && p[beg - 1] == '0' &&
					    (p[beg] == 'X' || p[beg] == 'x'))
						--beg;
					else
						++beg;
					break;
				}
				if (beg == 0)
					break;
			}

			/* Skip possible leading sign. */
			if (beg != 0 && p[beg] != '0' &&
			    (p[beg - 1] == '+' || p[beg - 1] == '-'))
				--beg;
		}

	if (LF_ISSET(V_KEYW)) {
		for (end = beg; ++end < len && inword(p[end]););
		--end;
	} else {
		for (end = beg; ++end < len;) {
			if (p[end] == 'X' || p[end] == 'x') {
				if (end != beg + 1 || p[beg] != '0')
					break;
				continue;
			}
			if (!innum(p[end]))
				break;
		}

		/* Just a sign isn't a number. */
		if (end == beg && (p[beg] == '+' || p[beg] == '-')) {
noword:			msgq(sp, M_BERR,
			    LF_ISSET(V_KEYW) ?
			    "212|Cursor not in a word" :
			    "213|Cursor not in a number");
			return (1);
		}
		--end;
	}

	/*
	 * Getting a keyword implies moving the cursor to its beginning.
	 * Refresh now.
	 */
	if (beg != sp->cno) {
		sp->cno = beg;
		(void)sp->s_refresh(sp);
	}

	/*
	 * XXX
	 * 8-bit clean problem.  Numeric keywords are handled using strtol(3)
	 * and friends.  This would have to be fixed in v_increment and here
	 * to not depend on a trailing NULL.
	 */
	len = (end - beg) + 2;				/* XXX */
	vp->klen = (end - beg) + 1;
	BINC_RET(sp, vp->keyword, vp->kbuflen, len);
	memmove(vp->keyword, p + beg, vp->klen);
	vp->keyword[vp->klen] = '\0';			/* XXX */
	return (0);
}

/*
 * getcount --
 *	Return the next count.
 */
static __inline int
getcount(sp, fkey, countp)
	SCR *sp;
	ARG_CHAR_T fkey;
	u_long *countp;
{
	u_long count, tc;
	CH ikey;

	ikey.ch = fkey;
	count = tc = 0;
	do {
		/* Assume that overflow results in a smaller number. */
		tc = count * 10 + ikey.ch - '0';
		if (count > tc) {
			/* Toss to the next non-digit. */
			do {
				if (getkey(sp, &ikey,
				    TXT_MAPCOMMAND | TXT_MAPNODIGIT))
					return (1);
			} while (isdigit(ikey.ch));
			msgq(sp, M_ERR,
			    "214|Number larger than %lu", ULONG_MAX);
			return (1);
		}
		count = tc;
		if (getkey(sp, &ikey, TXT_MAPCOMMAND | TXT_MAPNODIGIT))
			return (1);
	} while (isdigit(ikey.ch));
	*countp = count;
	return (0);
}

/*
 * getkey --
 *	Return the next key.
 */
static __inline int
getkey(sp, ikeyp, map)
	SCR *sp;
	CH *ikeyp;
	u_int map;
{
	switch (term_key(sp, ikeyp, map)) {
	case INP_EOF:
	case INP_ERR:
		F_SET(sp, S_EXIT_FORCE);
		return (1);
	case INP_INTR:
		/*
		 * !!!
		 * Historically, vi beeped on command level interrupts.
		 *
		 * Historically, vi exited to ex mode if no file was named
		 * on the command line, and two interrupts were generated
		 * in a row.  (Just figured you might want to know that.)
		 */
		(void)sp->s_bell(sp);
		return (1);
	case INP_OK:
		return (0);
	}
	/* NOTREACHED */
}
