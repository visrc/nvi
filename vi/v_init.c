/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_init.c,v 5.10 1993/02/13 13:09:11 bostic Exp $ (Berkeley) $Date: 1993/02/13 13:09:11 $";
#endif /* not lint */

#include <curses.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"
#include "options.h"
#include "screen.h"
#include "term.h"

#ifdef	FWOPEN_NOT_AVAILABLE
#include <sys/syscall.h>			/* SYS_write. */

#include <unistd.h>

/*
 * The fwopen call substitutes a local routine for the write system call.
 * This allows vi to trap all of the writes done via stdio by ex.  If you
 * don't have a fwopen or a similar way of replacing stdio's calls to write,
 * the following kludge may work.  It depends on your loader not realizing
 * that it's being tricked and linking the C library with the real write
 * system call.  Getting the real write system call is a bit tricky, there
 * are two possible ways listed below.
 */
static EXF	*v_ep;
static int	 v_fd = -1;

ssize_t
write(fd, buf, n)
	int fd;
	const void *buf;
	size_t n;
{
        if (fd == v_fd)
                return (v_exwrite(v_ep, buf, n));
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
v_init(ep)
	EXF *ep;
{
	ep->s_confirm = v_confirm;
	ep->s_end = v_end;

#ifdef FWOPEN_NOT_AVAILABLE
	if ((ep->stdfp = fopen(_PATH_DEVNULL, "w")) == NULL)
		return (1);
	v_fd = fileno(ep->stdfp);
	v_ep = ep;
#else
	ep->stdfp = fwopen(ep, v_exwrite);
#endif

	if (ISSET(O_COMMENT) && v_comment(ep))
		return (1);

	FF_SET(ep, F_REDRAW);

	ep->lines = LINES;		/* XXX: Way ugly. */
	ep->cols = COLS;

	scr_begin(ep);

	return (0);
}

/*
 * v_end --
 *	End vi session.
 */
int
v_end(ep)
	EXF *ep;
{
#ifdef FWOPEN_NOT_AVAILABLE
	v_ep = NULL;
	v_fd = -1;
#endif
	(void)fclose(ep->stdfp);
	ep->stdfp = stdout;

	scr_end(ep);

	return (0);
}
