/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_read.c,v 10.6 1995/10/28 10:36:08 bostic Exp $ (Berkeley) $Date: 1995/10/28 10:36:08 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "../common/pathnames.h"
#include "../ex/script.h"
#include "cl.h"

static input_t cl_read __P((SCR *,
    int, CHAR_T *, size_t, int *, struct timeval *));

/*
 * cl_event --
 *	Return a single event.
 *
 * PUBLIC: int cl_event __P((SCR *, EVENT *, u_int32_t, int));
 */
int
cl_event(sp, evp, flags, ms)
	SCR *sp;
	EVENT *evp;
	u_int32_t flags;
	int ms;
{
	struct timeval t, *tp;
	CL_PRIVATE *clp;
	int changed, nr;

	/*
	 * Queue signal based events.  We never clear SIGHUP or SIGTERM events,
	 * so that we just keep returning them until the editor dies.
	 */
	clp = CLP(sp);
retest:	if (LF_ISSET(EC_INTERRUPT) || F_ISSET(clp, CL_SIGINT)) {
		if (F_ISSET(clp, CL_SIGINT)) {
			F_CLR(clp, CL_SIGINT);
			evp->e_event = E_INTERRUPT;
		} else
			evp->e_event = E_TIMEOUT;
		return (0);
	}
	if (F_ISSET(clp, CL_SIGHUP | CL_SIGTERM | CL_SIGWINCH)) {
		if (F_ISSET(clp, CL_SIGHUP)) {
			evp->e_event = E_SIGHUP;
			return (0);
		}
		if (F_ISSET(clp, CL_SIGTERM)) {
			evp->e_event = E_SIGTERM;
			return (0);
		}
		if (F_ISSET(clp, CL_SIGWINCH)) {
			F_CLR(clp, CL_SIGWINCH);
			if (cl_ssize(sp, 1, &evp->e_lno, &evp->e_cno, &changed))
				return (1);
			if (changed) {
				evp->e_event = E_RESIZE;
				return (0);
			}
			/* No real change, ignore the signal. */
		}
	}

	/* Set timer. */
	if (ms == 0)
		tp = NULL;
	else {
		t.tv_sec = ms / 1000;
		t.tv_usec = (ms % 1000) * 1000;
		tp = &t;
	}

	/* Read input characters. */
	switch (cl_read(sp,
	    LF_ISSET(EC_QUOTED), clp->ibuf, sizeof(clp->ibuf), &nr, tp)) {
	case INP_OK:
		evp->e_csp = clp->ibuf;
		evp->e_len = nr;
		evp->e_event = E_STRING;
		break;
	case INP_EOF:
		evp->e_event = E_EOF;
		break;
	case INP_ERR:
		evp->e_event = E_ERR;
		break;
	case INP_INTR:
		goto retest;
	case INP_TIMEOUT:
		evp->e_event = E_TIMEOUT;
		break;
	default:
		abort();
	}
	return (0);
}

/*
 * cl_read --
 *	Read characters from the input.
 */
static input_t
cl_read(sp, quoted, bp, blen, nrp, tp)
	SCR *sp;
	int quoted;
	CHAR_T *bp;
	size_t blen;
	int *nrp;
	struct timeval *tp;
{
	struct termios term1, term2;
	struct timeval poll;
	CL_PRIVATE *clp;
	GS *gp;
	SCR *tsp;
	fd_set rdfd;
	input_t rval;
	int maxfd, nr, term_reset;

	term_reset = 0;

	/*
	 * 1: A read from a file or a pipe.  In this case, the reads
	 *    never timeout regardless.  This means that we can hang
	 *    when trying to complete a map, but we're going to hang
	 *    on the next read anyway.
	 */
	gp = sp->gp;
	if (!F_ISSET(gp, G_STDIN_TTY)) {
		switch (nr = read(STDIN_FILENO, bp, blen)) {
		case 0:
			return (INP_EOF);
		case -1:
			goto err;
		default:
			*nrp = nr;
			return (INP_OK);
		}
		/* NOTREACHED */
	}

	/*
	 * 2: A read with an associated timeout, e.g., trying to complete
	 *    a map sequence.  If input exists, we fall into #3.
	 */
	FD_ZERO(&rdfd);
	poll.tv_sec = 0;
	poll.tv_usec = 0;
	if (tp != NULL) {
		FD_SET(STDIN_FILENO, &rdfd);
		switch (select(STDIN_FILENO + 1,
		    &rdfd, NULL, NULL, tp == NULL ? &poll : tp)) {
		case 0:
			return (INP_TIMEOUT);
		case -1:
			goto err;
		default:
			break;
		}
	}
	
	/*
	 * The user can enter a key in the editor to quote a character.  If we
	 * get here and the next key is supposed to be quoted, do what we can.
	 * Reset the tty so that the user can enter a ^C, ^Q, ^S.  There's an
	 * obvious race here, when the key has already been entered, but there's
	 * nothing that we can do to fix that problem.
	 */
	if (quoted && !tcgetattr(STDIN_FILENO, &term1)) {
		term2 = term1;
		term2.c_lflag &= ~ISIG;
		term2.c_iflag &= ~(IXON | IXOFF);
		term_reset = 1;
		(void)tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &term2);
	}

	/*
	 * 3: Wait for input.
	 *
	 * Select on the command input and scripting window file descriptors.
	 * It's ugly that we wait on scripting file descriptors here, but it's
	 * the only way to keep from locking out scripting windows.
	 */
	if (F_ISSET(gp, G_SCRIPT)) {
loop:		FD_ZERO(&rdfd);
		FD_SET(STDIN_FILENO, &rdfd);
		maxfd = STDIN_FILENO;
		for (tsp = gp->dq.cqh_first;
		    tsp != (void *)&gp->dq; tsp = tsp->q.cqe_next)
			if (F_ISSET(sp, S_SCRIPT)) {
				FD_SET(sp->script->sh_master, &rdfd);
				if (sp->script->sh_master > maxfd)
					maxfd = sp->script->sh_master;
			}
		switch (select(maxfd + 1, &rdfd, NULL, NULL, NULL)) {
		case 0:
			abort();
		case -1:
			goto err;
		default:
			break;
		}
		if (!FD_ISSET(STDIN_FILENO, &rdfd)) {
			if (sscr_input(sp))
				return (INP_ERR);
			goto loop;
		}
	}

	/*
	 * 4: Read the input.
	 *
	 * !!!
	 * What's going on here is some scary stuff.  Ex runs the terminal in
	 * canonical mode.  So, the <newline> character terminating a line of
	 * input is returned in the buffer, but a trailing <EOF> character is
	 * not similarly included.  As ex uses 0<EOF> and ^<EOF> as autoindent
	 * commands, it has to see the trailing <EOF> characters to determine
	 * the difference between the user entering "0ab" and "0<EOF>ab".  We
	 * leave an extra slot in the buffer, so that we can add a trailing
	 * <EOF> character if the buffer isn't terminated by a <newline>.  We
	 * lose if the buffer is too small for the line and exactly N characters
	 * are entered followed by an <EOF> character.
	 */
#define	ONE_FOR_EOF	1
	clp = CLP(sp);
	switch (nr = read(STDIN_FILENO, bp, blen - ONE_FOR_EOF)) {
	case  0:				/* EOF. */
		/*
		 * ^D in canonical mode returns a read of 0, i.e. EOF.  EOF is
		 * a valid command, but we don't want to loop forever because
		 * the terminal driver is returning EOF because the user has
		 * disconnected. The editor will almost certainly try to write
		 * something before this fires, which should kill us, but You
		 * Never Know.
		 */
		if (++clp->eof_count < 50) {
			bp[0] = clp->orig.c_cc[VEOF];
			*nrp = 1;
			rval = INP_OK;

		} else
			rval = INP_EOF;
		break;
	case -1:				/* Error or interrupt. */
err:		if (errno == EINTR)
			rval = INP_INTR;
		else {
			rval = INP_ERR;
			msgq(sp, M_SYSERR, "input");
		}
		break;
	default:				/* Input characters. */
		if (F_ISSET(sp, S_EX) && bp[nr - 1] != '\n')
			bp[nr++] = clp->orig.c_cc[VEOF];
		*nrp = nr;
		clp->eof_count = 0;
		rval = INP_OK;
		break;
	}

	/* Restore the terminal state if it was modified. */
	if (term_reset)
		(void)tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &term1);
	return (rval);
}
