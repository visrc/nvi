/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ip_trans.c,v 8.3 1996/12/03 10:22:23 bostic Exp $ (Berkeley) $Date: 1996/12/03 10:22:23 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "../common/common.h"
#include "../ip_vi/ip.h"
#include "ipc_extern.h"

/*
 * ip_trans --
 *	Translate vi messages into function calls.
 *
 * PUBLIC: int ip_trans __P((char *, size_t, size_t *));
 */
int
ip_trans(bp, len, skipp)
	char *bp;
	size_t len, *skipp;
{
	extern int (*iplist[IPO_EVENT_MAX - 1]) __P((IP_BUF *));
	IP_BUF ipb;
	size_t nlen;
	char *fmt, *p;

	switch (bp[0]) {
	case IPO_ADDSTR:
	case IPO_RENAME:
		fmt = "s";
		break;
	case IPO_BUSY:
		fmt = "s1";
		break;
	case IPO_ATTRIBUTE:
	case IPO_MOVE:
		fmt = "12";
		break;
	case IPO_REWRITE:
		fmt = "1";
		break;
	default:
		fmt = "";
	}

	nlen = IPO_CODE_LEN;
	p = bp + IPO_CODE_LEN;
	for (; *fmt != '\0'; ++fmt)
		switch (*fmt) {
		case '1':
			nlen += IPO_INT_LEN;
			if (len < nlen)
				return (0);
			memcpy(&ipb.val1, p, IPO_INT_LEN);
			ipb.val1 = ntohl(ipb.val1);
			p += IPO_INT_LEN;
			break;
		case '2':
			nlen += IPO_INT_LEN;
			if (len < nlen)
				return (0);
			memcpy(&ipb.val2, p, IPO_INT_LEN);
			ipb.val2 = ntohl(ipb.val2);
			p += IPO_INT_LEN;
			break;
		case 's':
			nlen += IPO_INT_LEN;
			if (len < nlen)
				return (0);
			memcpy(&ipb.len, p, IPO_INT_LEN);
			ipb.len = ntohl(ipb.len);
			p += IPO_INT_LEN;
			nlen += ipb.len;
			if (len < nlen)
				return (0);
			ipb.str = p;
			p += ipb.len;
			break;
		}
	*skipp += nlen;

	/* Check for out-of-band events. */
	if (bp[0] > IPO_EVENT_MAX)
		abort();

	/* Call the underlying routine. */
	return (iplist[bp[0] - 1](&ipb));
}
