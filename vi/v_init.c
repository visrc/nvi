/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_init.c,v 8.5 1993/08/19 15:01:20 bostic Exp $ (Berkeley) $Date: 1993/08/19 15:01:20 $";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <paths.h>

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
	 * The default address is line 1, column 0.  If the address set
	 * bit is on for this file, load the address, ensuring that it
	 * exists.
	 */
	if (F_ISSET(sp->frp, FR_CURSORSET)) {
		sp->lno = sp->frp->lno;
		sp->cno = sp->frp->cno;

		if (file_gline(sp, ep, sp->lno, &len) == NULL) {
			if (sp->lno != 1 || sp->cno != 0) {
				if (file_lline(sp, ep, &sp->lno))
					return (1);
				if (sp->lno == 0)
					sp->lno = 1;
				sp->cno = 0;
			}
		} else if (sp->cno >= len)
			sp->cno = 0;

	} else {
		sp->lno = 1;
		sp->cno = 0;

		if (O_ISSET(sp, O_COMMENT) && v_comment(sp, ep))
			return (1);
	}

	/*
	 * After address set, run any saved or initial command; failure
	 * doesn't halt the session.  Hopefully changing the cursor
	 * position didn't affect the success of the command, but it's
	 * possible and undetectible.
	 */
	if (sp->comm_len > 0) {
		(void)ex_cstring(sp, ep, sp->comm, sp->comm_len);
		F_SET(ep, S_REDRAW);
	}

	/*
	 * Now have the real address the user wants.
	 * Fill the screen map.
	 */
	if (F_ISSET(sp, S_REFORMAT)) {
		if (sp->s_fill(sp, ep, sp->lno, P_FILL))
			return (1);
		F_CLR(sp, S_REFORMAT);
		F_SET(sp, S_REDRAW);
	}

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
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

#ifdef FWOPEN_NOT_AVAILABLE
	sp->trapped_fd = -1;
#endif
	(void)fclose(sp->stdfp);
	return (0);
}
