/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_init.c,v 5.18 1993/03/26 13:40:29 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:40:29 $";
#endif /* not lint */

#include <curses.h>

#include "vi.h"
#include "vcmd.h"

#ifdef	FWOPEN_NOT_AVAILABLE
#include <sys/types.h>
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
static SCR	*v_sp;
static int	 v_fd = -1;

int
write(fd, buf, n)
	int fd;
	const void *buf;
	size_t n;
{
        if (fd == v_fd)
                return (v_exwrite(v_sp, buf, n));
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
	/* Set up ex functions. */
	sp->confirm = v_confirm;
	sp->end = v_end;

	/* Make ex display to a special function. */
#ifdef FWOPEN_NOT_AVAILABLE
	if ((sp->stdfp = fopen(_PATH_DEVNULL, "w")) == NULL)
		return (1);
	v_fd = fileno(sp->stdfp);
	v_sp = sp;
#else
	sp->stdfp = fwopen(sp, v_exwrite);
#endif

	if (F_ISSET(ep, F_NEWSESSION) &&
	    ISSET(O_COMMENT) && v_comment(sp, ep))
		return (1);
	ep->cno = 0;
	return (0);
}

/*
 * v_end --
 *	End vi session.
 */
int
v_end(sp)
	SCR *sp;
{
#ifdef FWOPEN_NOT_AVAILABLE
	v_sp = NULL;
	v_fd = -1;
	(void)fclose(sp->stdfp);
#endif
	return (0);
}
