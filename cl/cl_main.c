/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_main.c,v 8.1 1995/06/08 19:01:00 bostic Exp $ (Berkeley) $Date: 1995/06/08 19:01:00 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <curses.h>
#include <errno.h>
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
#include "cl.h"
#include "../ex/script.h"

GS *__global_list;			/* GLOBAL: List of screens. */

static void killsig __P((SCR *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	static int reenter;
	struct timeval t, *tp;
	CHAR_T ch;
	CL_PRIVATE *clp;
	EVENT ev, *evp;
	GS *gp;
	SCR *next, *sp;
	recno_t rows;
	size_t cols;
	u_int32_t flags, timeout;
	int killset, nr, rval;

	/* If loaded at 0 and jumping through a NULL pointer, stop. */
	if (reenter++)
		abort();

	/* Figure out how big the screen is. */
	if (cl_ssize(NULL, 0, &rows, &cols))
		exit (1);

	/* Create first screen structure, parse the arguments. */
	switch (v_init(argc, argv, rows, cols, &__global_list)) {
	case INIT_OK:
		break;
	case INIT_DONE:
		exit (0);
	case INIT_ERR:
		exit (1);
	case INIT_USAGE:
		(void)fprintf(stderr,
	"usage: ex [-eFRrsv] [-c command] [-t tag] [-w size] [files ...]\n");
		(void)fprintf(stderr,
	"usage: vi [-eFlRrv] [-c command] [-t tag] [-w size] [files ...]\n");
		goto err;
	}

	/* Edit the first screen. */
	gp = __global_list;
	sp = gp->dq.cqh_first;

	/*
	 * !!!
	 * There's this little chicken and egg problem.  The screen isn't
	 * initialized until all of the exrc files have been read and the
	 * first file loaded.  Unfortunately, you can put any ex command
	 * you want into an exrc file, or as a command-line command.  There
	 * are two categories of problem.
	 *
	 * First, if something required ex to write to the screen, we'll
	 * want to wait before overwriting it.  Second, if ex had to do
	 * serious processing, then the terminal will have been set up.
	 * So, if the CL private structure has been allocated when we get
	 * here, we know that ex has been busy.  Clean up.
	 */
	clp = CLP(sp);
	if (clp != NULL) {
		if (F_ISSET(sp, S_EX_WROTE)) {
			(void)write(STDOUT_FILENO,
			    STR_CMSG, sizeof(STR_CMSG) - 1);
			if (cl_getkey(sp, &ch))
				goto err;
			F_CLR(sp, S_EX_WROTE);
		}
		/*
		 * XXX
		 * We may have just discarded some characters from the tty
		 * queue.
		 */
		if (cl_ex_end(sp))
			goto err;
	}

	/* Screen initialization. */
	if (F_ISSET(sp, S_EX) ? cl_ex_init(sp) : cl_vi_init(sp))
		goto err;
	clp = CLP(sp);

	/* Editor startup. */
	ev.e_event = E_START;
	if (v_event_handler(sp, &ev, &timeout))
		goto err;

	killset = 0;
	for (;;) {					/* Edit loop. */
		for (;;) {				/* Event loop. */
			/* Queue signal based events. */
			if (F_ISSET(clp, CL_SIGCONT |
			    CL_SIGHUP | CL_SIGINT | CL_SIGTERM | CL_SIGWINCH)) {
				if (F_ISSET(clp, CL_SIGCONT)) {
					F_CLR(clp, CL_SIGCONT);
					ev.e_event = E_SIGCONT;
					goto handle;
				}
				if (F_ISSET(clp, CL_SIGHUP)) {
					killset = 1;
					ev.e_event = E_SIGHUP;
					goto handle;
				}
				if (F_ISSET(clp, CL_SIGINT)) {
					F_CLR(clp, CL_SIGINT);
					ev.e_event = E_INTERRUPT;
					goto handle;
				}
				if (F_ISSET(clp, CL_SIGTERM)) {
					killset = 1;
					ev.e_event = E_SIGTERM;
					goto handle;
				}
				if (F_ISSET(clp, CL_SIGWINCH)) {
					F_CLR(clp, CL_SIGWINCH);
					if (!cl_ssize(sp, 1,
					    &ev.e_lno, &ev.e_cno)) {
						ev.e_event = E_RESIZE;
						goto handle;
					}
				}
			}

			/* Characters may have been queued. */
			if (clp->icnt != 0) {
				nr = clp->icnt;
				clp->icnt = 0;
				goto charq;
			}

			/*
			 * If the upper layer is timing out, set it up.
			 */
			if (timeout != 0) {
				t.tv_sec = timeout / 1000;
				t.tv_usec = timeout % 1000;
				tp = &t;
			} else
				tp = NULL;

			/* Read input characters. */
			switch (cl_read(sp,
			    clp->ibuf, sizeof(clp->ibuf), &nr, tp)) {
			case INP_OK:
charq:				ev.e_csp = clp->ibuf;
				ev.e_len = nr;
				ev.e_event = E_STRING;
				break;
			case INP_EOF:
				ev.e_event = E_EOF;
				break;
			case INP_ERR:
				ev.e_event = E_ERR;
				break;
			case INP_INTR:
				ev.e_event = EINTR;
				break;
			case INP_TIMEOUT:
				ev.e_event = E_TIMEOUT;
				break;
			}

			/*
			 * Send events until the screen exits or the editor
			 * mode changes.
			 */
handle:			timeout = 0;
			flags =
			    F_ISSET(sp, S_EX | S_VI | S_EXIT | S_EXIT_FORCE);
			if (v_event_handler(sp, &ev, &timeout))
				goto err;
			if (killset || flags !=
			    F_ISSET(sp, S_EX | S_VI | S_EXIT | S_EXIT_FORCE))
				break;

			/* Check for switched screens. */
			if (F_ISSET(sp, S_SSWITCH)) {
				F_CLR(sp, S_SSWITCH);
				sp = sp->nextdisp;
			}
		}

		/* If a killer signal arrived, we're done. */
		if (killset) {
			killsig(sp);
			/* NOTREACHED */
		}

		/*
		 * Edit mode change.  Shutdown the current editor and
		 * start the new one.
		 */
		if (F_ISSET(sp, S_EX | S_VI) != LF_ISSET(S_EX | S_VI)) {
			ev.e_event = E_STOP;
			if (v_event_handler(sp, &ev, &timeout))
				goto err;

			if (F_ISSET(sp, S_EX)) {
				if (cl_ex_end(sp) || cl_vi_init(sp))
					goto err;
			} else
				if (cl_vi_end(sp) || cl_ex_init(sp))
					goto err;
			clp = CLP(sp);

			ev.e_event = E_START;
			if (v_event_handler(sp, &ev, &timeout))
				goto err;
		}

		/*
		 * Screen exit.  If exiting the last screen, shutdown
		 * the editor.
		 */
		if (F_ISSET(sp, S_EXIT | S_EXIT_FORCE)) {
			if ((next = sp->nextdisp) == NULL) {
				ev.e_event = E_STOP;
				if (v_event_handler(sp, &ev, &timeout))
					goto err;

				if (F_ISSET(sp, S_EX))
					(void)cl_ex_end(sp);
				else
					(void)cl_vi_end(sp);
			}
			if (screen_end(sp) || (sp = next) == NULL)
				break;
		}
	}

	rval = 0;
	if (0) {
err:		rval = 1;
		if (F_ISSET(sp, S_EX))
			(void)cl_ex_end(sp);
		else
			(void)cl_vi_end(sp);
	}

	/* Shutdown screens/global area. */
	v_end(gp);
	exit (rval);
}

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
	struct termios term1, term2;
	struct timeval t, *tp;
	GS *gp;
	input_t rval;
	int maxfd, nr, term_reset;

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

	/*
	 * 2: A read with an associated timeout.  In this case, we are trying
	 *    to complete a map sequence.  Ignore script windows and timeout
	 *    as specified.  If input arrives, we fall into #3, but because
	 *    timeout isn't NULL, don't read anything but command input.
	 */
	if (timeout != NULL) {
		if (F_ISSET(sp, S_SCRIPT))
			FD_CLR(sp->script->sh_master, &sp->rdfd);
		FD_SET(STDIN_FILENO, &sp->rdfd);
		switch (select(STDIN_FILENO + 1,
		    &sp->rdfd, NULL, NULL, timeout)) {
		case -1:			/* Error or interrupt. */
			if (errno == EINTR)
				return (INP_INTR);
			goto err;
		case  1:			/* Characters ready. */
			break;
		case  0:			/* Timeout. */
			return (INP_TIMEOUT);
		}
	}

	/*
	 * The user can enter a key in the editor to quote a character.  If we
	 * get here and the next key is supposed to be quoted, do what we can.
	 * Reset the tty so that the user can enter a ^C, ^Q, ^S.  There's an
	 * obvious race here, when the key has already been entered, but there's
	 * nothing that we can do to fix that problem.
	 */
	if (FL_ISSET(gp->ec_flags, EC_QUOTED) &&
	    !tcgetattr(STDIN_FILENO, &term1)) {
		term2 = term1;
		term2.c_lflag &= ~ISIG;
		term2.c_iflag &= ~(IXON | IXOFF);
		term_reset = 1;
		(void)tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &term2);
	} else
		term_reset = 0;

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

		switch (select(maxfd + 1, &sp->rdfd, NULL, NULL, tp)) {
		case -1:		/* Error or interrupt. */
			if (errno == EINTR)
				rval = INP_INTR;
			else {
err:				msgq(sp, M_SYSERR, "select");
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

/*
 * killsig --
 *	Die with the proper exit status.  Don't bother using sigaction(2)
 *	'cause we want the default behavior.
 */
static void
killsig(sp)
	SCR *sp;
{
	CL_PRIVATE *clp;
	int signo;

	clp = CLP(sp);
	if (F_ISSET(clp, CL_SIGHUP))
		signo = SIGHUP;
	if (F_ISSET(clp, CL_SIGTERM))
		signo = SIGTERM;

	(void)signal(signo, SIG_DFL);
	(void)kill(getpid(), signo);
	/* NOTREACHED */

	exit (1);
}
