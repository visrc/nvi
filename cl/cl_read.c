/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_read.c,v 10.1 1995/07/04 12:47:43 bostic Exp $ (Berkeley) $Date: 1995/07/04 12:47:43 $";
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

#include "compat.h"
#include <db.h>
#include <regex.h>
#include <pathnames.h>

#include "common.h"
#include "cl.h"
#include "../ex/script.h"

/*
 * cl_read --
 *	Read characters from the input.
 *
 * PUBLIC: input_t cl_read __P((SCR *,
 * PUBLIC: CHAR_T *, size_t, int *, struct timeval *));
 */
input_t
cl_read(sp, bp, blen, nrp, timeout)
	SCR *sp;
	CHAR_T *bp;
	size_t blen;
	int *nrp;
	struct timeval *timeout;
{
	static struct timeval poll;
	struct termios term1, term2;
	struct timeval t, *tp;
	GS *gp;
	input_t rval;
	int cready, maxfd, nr, term_reset;

	term_reset = 0;

	/*
	 * There are three cases here:
	 *
	 * 1: A read from a file or a pipe.  In this case, the reads
	 *    never timeout regardless.  This means that we can hang
	 *    when trying to complete a map, but we're going to hang
	 *    on the next read anyway.
	 */
	gp = sp->gp;
	if (!F_ISSET(gp, G_STDIN_TTY)) {
		if ((nr = read(STDIN_FILENO, bp, blen)) > 0) {
			*nrp = nr;
			return (INP_OK);
		}
		return (nr == 0 ? INP_EOF : INP_ERR);
	}

	/* Find out if there are any characters waiting. */
	if (F_ISSET(sp, S_SCRIPT))
		FD_CLR(sp->script->sh_master, &sp->rdfd);
	FD_SET(STDIN_FILENO, &sp->rdfd);
	cready = select(STDIN_FILENO + 1,
	    &sp->rdfd, NULL, NULL, timeout == NULL ? &poll : timeout);
	if (cready == -1) {
		if (errno == EINTR)
			return (INP_INTR);
		goto err;
	}

	/*
	 * 2: A read with an associated timeout.  In this case, we are trying
	 *    to complete a map sequence.  Ignore script windows and timeout
	 *    as specified.  If input arrives, we fall into #3, but because
	 *    timeout isn't NULL, don't read anything but command input.
	 */
	if (timeout != NULL && cready == 0)
		return (INP_TIMEOUT);
	
	/*
	 * Here's where we handle overwriting portions of the user's screen
	 * when displaying messages or ex output, or temporarily entering ex
	 * canonical mode.  If we're not in a map and there no keys waiting
	 * to be returned, we try and deal with messages.  The reason for the
	 * latter test is so that we never eat a key that the user intended
	 * for another purpose.
	 */
	if (timeout == NULL && cready == 0)
		if (cl_resolve(sp, 0)) {
			bp[0] = ':';
			*nrp = 1;
			return (0);
		}
		
	/*
	 * The user can enter a key in the editor to quote a character.  If we
	 * get here and the next key is supposed to be quoted, do what we can.
	 * Reset the tty so that the user can enter a ^C, ^Q, ^S.  There's an
	 * obvious race here, when the key has already been entered, but there's
	 * nothing that we can do to fix that problem.
	 */
	if (cready == 0 && FL_ISSET(gp->ec_flags, EC_QUOTED) &&
	    !tcgetattr(STDIN_FILENO, &term1)) {
		term2 = term1;
		term2.c_lflag &= ~ISIG;
		term2.c_iflag &= ~(IXON | IXOFF);
		term_reset = 1;
		(void)tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &term2);
	}

	/*
	 * 3: At this point, we'll take anything that comes.  Select on the
	 *    command file descriptor and the file descriptor for the script
	 *    window if there is one.  Poll the fd's, increasing the timeout
	 *    each time each time we don't get anything until we're blocked
	 *    on I/O.
	 */
	for (t.tv_sec = t.tv_usec = 0;;) {
		/*
		 * Reset each time -- sscr_input() may call other
		 * routines which could reset bits.
		 */
		if (timeout == NULL && F_ISSET(sp, S_SCRIPT)) {
			tp = &t;

			FD_SET(STDIN_FILENO, &sp->rdfd);
			if (F_ISSET(sp, S_SCRIPT)) {
				FD_SET(sp->script->sh_master, &sp->rdfd);
				maxfd =
				    MAX(STDIN_FILENO, sp->script->sh_master);
			} else
				maxfd = STDIN_FILENO;
		} else {
			tp = NULL;

			FD_SET(STDIN_FILENO, &sp->rdfd);
			if (F_ISSET(sp, S_SCRIPT))
				FD_CLR(sp->script->sh_master, &sp->rdfd);
			maxfd = STDIN_FILENO;
		}

		if (cready)
			cready = 0;
		else
			switch (select(maxfd + 1, &sp->rdfd, NULL, NULL, tp)) {
			case -1:		/* Error or interrupt. */
				if (errno == EINTR)
					rval = INP_INTR;
				else {
err:					msgq(sp, M_SYSERR, "select");
					rval = INP_ERR;
				}
				goto ret;
			case 0:			/* Timeout. */
				if (t.tv_usec) {
					++t.tv_sec;
					t.tv_usec = 0;
				} else
					t.tv_usec += 500000;
				continue;
			}

		if (timeout == NULL && F_ISSET(sp, S_SCRIPT) &&
		    FD_ISSET(sp->script->sh_master, &sp->rdfd)) {
			sscr_input(sp);
			continue;
		}

		/*
		 * !!!
		 * What's going on here is fairly scary stuff.  Ex runs the
		 * terminal in canonical mode.  This means that the <newline>
		 * character terminating a line of input is returned in the
		 * buffer, but a trailing <EOF> character is not similarly
		 * included.  Ex uses 0<EOF> and ^<EOF> as autoindent commands,
		 * so it has to see the trailing <EOF> characters to determine
		 * the difference between the user entering "0ab" or "0<EOF>ab".
		 * We leave an extra slot in the buffer, so that if the buffer
		 * isn't terminated by a <newline>, we can add a trailing <EOF>
		 * character.  We lose if the buffer is too small for the line
		 * and exactly N characters are entered followed by an <EOF>
		 * character.
		 */
#define	ONE_FOR_EOF	1
		switch (nr = read(STDIN_FILENO, bp, blen - ONE_FOR_EOF)) {
		case  0:			/* EOF. */
			/*
			 * ^D in canonical mode returns a read of 0, i.e. EOF.
			 * It's a valid command, but we don't want to run
			 * forever if the terminal driver is returning EOF
			 * after the user has disconnected.  We'll almost
			 * certainly try to write something before this fires,
			 * which should kill us, but You Never Know.
			 */
			if (++CLP(sp)->eof_count < 75) {
				bp[0] = gp->original_termios.c_cc[VEOF];
				*nrp = 1;
				rval = INP_OK;

			} else
				rval = INP_EOF;
			goto ret;
		case -1:			/* Error or interrupt. */
			if (errno == EINTR)
				rval = INP_INTR;
			else {
				rval = INP_ERR;
				msgq(sp, M_SYSERR, "read");
			}
			goto ret;
		default:			/* Input characters. */
			if (F_ISSET(sp, S_EX) && bp[nr - 1] != '\n')
				bp[nr++] = gp->original_termios.c_cc[VEOF];
			*nrp = nr;
			CLP(sp)->eof_count = 0;
			rval = INP_OK;
			goto ret;
		}
		/* NOTREACHED */
	}

ret:	/* Restore the terminal state if it was modified. */
	if (term_reset)
		(void)tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &term1);
	return (rval);
}
