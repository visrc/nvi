/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_at.c,v 5.17 1993/03/26 13:40:18 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:40:18 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

int
v_at(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	CB *cb;
	TEXT *tp;
	size_t len, remain;
	int key;
	char *p, *start;

	key = vp->character;
	CBNAME(sp, key, cb);
	CBEMPTY(sp, key, cb);

	if (sp->atkey_len == 0)
		memset(sp->atkey_stack, 0, sizeof(sp->atkey_stack));
	else if (sp->atkey_stack[key]) {
		msgq(sp, M_ERROR, "Buffer %s already occurs in this command.",
		    charname(sp, key));
		return (1);
	}

	/* Get buffer for rest of at string plus cut buffer. */
	remain = sp->atkey_len ?
	    sp->atkey_len - (sp->atkey_cur - sp->atkey_buf) : 0;

	/* Check for overflow. */
	len = cb->len + remain;
	if (len < cb->len + remain) {
		msgq(sp, M_ERROR, "Buffer overflow.");
		return (1);
	}

	if ((start = p = malloc(len)) == NULL) {
		msgq(sp, M_ERROR, "Error: %s", strerror(errno));
		return (1);
	}

	/* Copy into the new buffer. */
	for (tp = cb->head;;) {
		memmove(p, tp->lp, tp->len);
		p += tp->len;
		*p++ = '\n';
		if ((tp = tp->next) == NULL)
			break;
	}
	
	/* Copy the rest of the current at string into place. */
	if (sp->atkey_len != 0) {
		memmove(p, sp->atkey_cur, remain);
		free(sp->atkey_buf);

	}
	/* Fix the pointers. */
	sp->atkey_buf = sp->atkey_cur = start;
	sp->atkey_len = len;

	sp->atkey_stack[key] = 1;

	return (0);
}
