/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_at.c,v 8.1 1993/06/09 22:26:38 bostic Exp $ (Berkeley) $Date: 1993/06/09 22:26:38 $";
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
		msgq(sp, M_ERR, "Buffer %s already occurs in this command.",
		    charname(sp, key));
		return (1);
	}

	/* Get buffer for rest of at string plus cut buffer. */
	remain = sp->atkey_len ?
	    sp->atkey_len - (sp->atkey_cur - sp->atkey_buf) : 0;

	/* Check for overflow. */
	len = cb->len + remain;
	if (len < cb->len + remain) {
		msgq(sp, M_ERR, "Buffer overflow.");
		return (1);
	}

	if ((start = p = malloc(len)) == NULL) {
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		return (1);
	}

	/* Copy into the new buffer. */
	for (tp = cb->txthdr.next; tp != (TEXT *)&cb->txthdr; tp = tp->next) {
		memmove(p, tp->lb, tp->len);
		p += tp->len;
		*p++ = '\n';
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

	*rp = *fm;
	return (0);
}
