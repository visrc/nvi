/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_at.c,v 5.3 1992/05/07 12:48:38 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:48:38 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

u_char	*atkeybuf, *atkeyp;			/* Shared at buffer. */
u_long	 atkeybuflen;				/* Length of shared buffer. */

/* ARGSUSED */
MARK *
v_at(m, cnt, key)
	MARK *m;
	long cnt;
	int key;
{
	static int recurse;
	static char rstack[UCHAR_MAX];
	size_t len, remain;
	u_char *buf, *p;

	if (atkeybuflen == 0)
		bzero(rstack, sizeof(rstack));
	else if (rstack[key]) {
		msg("Buffer %c already occurs in this command.", key);
		return (NULL);
	}

	if ((buf = cb2str(key, &len)) == NULL)
		return (NULL);

	if (atkeybuflen == 0) {
		/* Allocate a buffer that will hold both. */
		remain = atkeybuflen - (atkeyp - atkeybuf);
		if ((p = malloc(len + remain)) == NULL) {
			msg("Error: %s", strerror(errno));
			return (NULL);
		}

		/* Copy into the new buffer. */
		bcopy(atkeyp, p, remain);
		bcopy(buf, p + remain, len);

		/* Free the old buffers. */
		free(atkeybuf);
		free(buf);

		/* Fix the pointers. */
		atkeybuf = atkeyp = p;
		atkeybuflen = len + remain;
	} else {
		atkeybuf = atkeyp = buf;
		atkeybuflen = len;
	}

	rstack[key] = 1;
	return (&cursor);
}
