/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: key.c,v 5.2 1991/12/17 12:11:45 bostic Exp $ (Berkeley) $Date: 1991/12/17 12:11:45 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>

#include "config.h"
#include "vi.h"

/*
 * For BSD, we use select() to wait for characters to become available,
 * and then do a read() to actually get the characters.  We also try to
 * handle SIGWINCH -- if the signal arrives during the select() call, then
 * we adjust the o_columns and o_lines variables, and fake a control-L.
 */
int
ttyread(buf, len, time)
	char	*buf;	/* where to store the gotten characters */
	int	len;	/* maximum number of characters to read */
	int	time;	/* maximum time to allow for reading */
{
	fd_set	rd;	/* the file descriptors that we want to read from */
	static	tty;	/* 'y' if reading from tty, or 'n' if not a tty */
	int	i;
	struct timeval t;
	struct timeval *tp;


	/* do we know whether this is a tty or not? */
	if (!tty)
	{
		tty = (isatty(0) ? 'y' : 'n');
	}

	/* compute the timeout value */
	if (time)
	{
		t.tv_sec = time / 10;
		t.tv_usec = (time % 10) * 100000L;
		tp = &t;
	}
	else
	{
		tp = (struct timeval *)0;
	}

	/* loop until we get characters or a definite EOF */
	for (;;)
	{
		if (tty == 'y')
		{
			/* wait until timeout or characters are available */
			FD_ZERO(&rd);
			FD_SET(0, &rd);
			i = select(1, &rd, (fd_set *)0, (fd_set *)0, tp);
		}
		else
		{
			/* if reading from a file or pipe, never timeout!
			 * (This also affects the way that EOF is detected)
			 */
			i = 1;
		}
	
		/* react accordingly... */
		switch (i)
		{
		  case -1:	/* assume we got an EINTR because of SIGWINCH */
			if (*o_lines != LINES || *o_columns != COLS)
			{
				*o_lines = LINES;
				*o_columns = COLS;
				if (mode != MODE_EX)
				{
					/* pretend the user hit ^L */
					*buf = ctrl('L');
					return 1;
				}
			}
			break;
	
		  case 0:	/* timeout */
			return 0;
	
		  default:	/* characters available */
			return read(0, buf, len);
		}
	}
}
