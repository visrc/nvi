/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_at.c,v 5.11 1992/12/05 11:10:37 bostic Exp $ (Berkeley) $Date: 1992/12/05 11:10:37 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "exf.h"
#include "vcmd.h"

u_long atkeybuflen;				/* Length of shared buffer. */
char *atkeybuf, *atkeyp;			/* Shared at buffer. */

int
v_at(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	static char rstack[UCHAR_MAX];
	CB *cb;
	TEXT *tp;
	size_t len, remain;
	int key;
	char *p, *start;

	key = vp->character;
	CBNAME(key, cb);
	CBEMPTY(key, cb);

	if (atkeybuflen == 0)
		bzero(rstack, sizeof(rstack));
	else if (rstack[key]) {
		msg("Buffer %c already occurs in this command.", key);
		return (1);
	}

	/* Get buffer for rest of at string plus cut buffer. */
	remain = atkeybuflen ? atkeybuflen - (atkeyp - atkeybuf) : 0;

	/* Check for overflow. */
	len = cb->len + remain;
	if (len < cb->len + remain) {
		msg("Buffer overflow.");
		return (1);
	}

	if ((start = p = malloc(len)) == NULL) {
		msg("Error: %s", strerror(errno));
		return (1);
	}

	/* Copy into the new buffer. */
	for (tp = cb->head;;) {
		bcopy(tp->lp, p, tp->len);
		p += tp->len;
		*p++ = '\n';
		if ((tp = tp->next) == NULL)
			break;
	}
	
	/* Copy the rest of the current at string into place. */
	if (atkeybuflen != 0) {
		bcopy(atkeyp, p, remain);
		free(atkeybuf);

	}
	/* Fix the pointers. */
	atkeybuf = atkeyp = start;
	atkeybuflen = len;

	rstack[key] = 1;

	return (0);
}
