/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_at.c,v 5.4 1992/05/15 11:14:05 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:14:05 $";
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

int
v_at(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	static int recurse;
	static char rstack[UCHAR_MAX];
	size_t len, remain;
	int key;
	u_char *buf, *p;

	key = vp->character;
	if (atkeybuflen == 0)
		bzero(rstack, sizeof(rstack));
	else if (rstack[key]) {
		msg("Buffer %c already occurs in this command.", key);
		return (1);
	}

	if ((buf = cb2str(key, &len)) == NULL)
		return (1);

	if (atkeybuflen == 0) {
		/* Allocate a buffer that will hold both. */
		remain = atkeybuflen - (atkeyp - atkeybuf);
		if ((p = malloc(len + remain)) == NULL) {
			msg("Error: %s", strerror(errno));
			return (1);
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
	return (0);
}
