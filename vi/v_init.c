/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_init.c,v 5.31 1993/05/15 11:24:05 bostic Exp $ (Berkeley) $Date: 1993/05/15 11:24:05 $";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <paths.h>

/*
 * Uncomment the following for FWOPEN_NOT_AVAILABLE.
 *
 * #define FWOPEN_NOT_AVAILABLE
 */
#include "vi.h"
#include "vcmd.h"
#include "excmd.h"

#ifdef	FWOPEN_NOT_AVAILABLE
#include <sys/types.h>
#include <sys/syscall.h>			/* SYS_write. */

#include <unistd.h>

/*
 * The fwopen call substitutes a local routine for the write system call.
 * This allows vi to trap all of the writes done via stdio by ex.  If you
 * don't have fwopen or a similar way of replacing stdio's calls to write,
 * the following kludge may work.  It depends on your loader not realizing
 * that it's being tricked and linking the C library with the real write
 * system call.  Getting the real write system call is a bit tricky, there
 * are two possible ways listed below.
 */
int
write(fd, buf, n)
	int fd;
	const void *buf;
	size_t n;
{
	SCR *sp;

	/*
	 * Walk the list of screens, and see if anyone is trapping
	 * this file descriptor.
	 */
	for (sp = __global_list->scrhdr.next;
	    sp != (SCR *)&__global_list->scrhdr; sp = sp->next)
		if (fd == sp->trapped_fd)
			return (sp->s_ex_write(sp, buf, n));

#ifdef SYS_write
	return (syscall(SYS_write, fd, buf, n));
#else
	return (_write(fd, buf, n));
#endif
}
#endif

/*
 * v_init --
 *	Initialize vi.
 */
int
v_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	size_t len;

	/* Make ex display to a special function. */
#ifdef FWOPEN_NOT_AVAILABLE
	if ((sp->stdfp = fopen(_PATH_DEVNULL, "w")) == NULL)
		return (1);
	sp->trapped_fd = fileno(sp->stdfp);
#else
	if ((sp->stdfp = fwopen(sp, sp->s_ex_write)) == NULL)
		return (1);
#endif
	(void)setvbuf(sp->stdfp, NULL, _IOLBF, 0);

	/*
	 * If no starting location specified, vi starts at the beginning.
	 * Otherwise, check to make sure that the location exists.
	 */
	if (F_ISSET(ep, F_NOSETPOS)) {
		sp->lno = 1;
		sp->cno = 0;
		if (O_ISSET(sp, O_COMMENT) && v_comment(sp, ep))
			return (1);
		F_CLR(ep, F_NOSETPOS);
	} else {
		sp->lno = ep->lno;
		sp->cno = ep->cno;
		if (file_gline(sp, ep, sp->lno, &len) == NULL) {
			if (sp->lno != 1 || sp->cno != 0) {
				if ((sp->lno = file_lline(sp, ep)) == 0)
					sp->lno = 1;
				sp->cno = 0;
			}
		} else if (sp->cno >= len)
			sp->cno = 0;
	}

	/*
	 * After location established, run any initial command.  Failure
	 * doesn't halt the session.  Don't worry about the cursor being
	 * repositioned affecting the success of this command, it's
	 * pretty unlikely.
	 */
	if (F_ISSET(ep, F_ICOMMAND)) {
		(void)ex_cstring(sp, ep, ep->icommand, strlen(ep->icommand));
		free(ep->icommand);
		F_CLR(ep, F_ICOMMAND);
		if (sp->lno != ep->lno || sp->cno != ep->cno) {
			ep->lno = sp->lno;
			ep->cno = sp->cno;
		}
	}

	/*
	 * Now have the real location the user wants.
	 * Fill the screen map.
	 */
	if (sp->s_fill(sp, ep, sp->lno, P_FILL))
		return (1);
	F_SET(sp, S_REDRAW);

	/* Display the status line. */
	return (status(sp, ep, sp->lno, 0));
}

/*
 * v_end --
 *	End vi session.
 */
int
v_end(sp)
	SCR *sp;
{
	/* Save the cursor location. */
	sp->ep->lno = sp->lno;
	sp->ep->cno = sp->cno;

#ifdef FWOPEN_NOT_AVAILABLE
	sp->trapped_fd = -1;
#endif
	(void)fclose(sp->stdfp);
	return (0);
}
