/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ip_trans.c,v 8.7 1996/12/05 23:05:53 bostic Exp $ (Berkeley) $Date: 1996/12/05 23:05:53 $";
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
 * PUBLIC: int ip_trans __P((char *, size_t *));
 */
int
ip_trans(bp, lenp)
	char *bp;
	size_t *lenp;
{
	extern int (*iplist[IPO_EVENT_MAX - 1]) __P((IP_BUF *));
	IP_BUF ipb;
	size_t len, needlen;
	u_int32_t *vp;
	int foff;
	char *fmt, *p, *s_bp;

	for (s_bp = bp, len = *lenp; len > 0;) {
		switch (foff = bp[0]) {
		case IPO_ADDSTR:
		case IPO_RENAME:
			fmt = "s";
			break;
		case IPO_ATTRIBUTE:
		case IPO_MOVE:
			fmt = "12";
			break;
		case IPO_BUSY_ON:
			fmt = "s1";
			break;
		case IPO_REWRITE:
			fmt = "1";
			break;
		case IPO_SCROLLBAR:
			fmt = "123";
			break;
		default:
			fmt = "";
		}

		p = bp + IPO_CODE_LEN;
		needlen = IPO_CODE_LEN;
		for (; *fmt != '\0'; ++fmt)
			switch (*fmt) {
			case '1':
				vp = &ipb.val1;
				goto value;
			case '2':
				vp = &ipb.val2;
				goto value;
			case '3':
				vp = &ipb.val3;
value:				needlen += IPO_INT_LEN;
				if (len < needlen)
					goto partial;
				memcpy(vp, p, IPO_INT_LEN);
				*vp = ntohl(*vp);
				p += IPO_INT_LEN;
				break;
			case 's':
				needlen += IPO_INT_LEN;
				if (len < needlen)
					goto partial;
				memcpy(&ipb.len, p, IPO_INT_LEN);
				ipb.len = ntohl(ipb.len);
				p += IPO_INT_LEN;
				needlen += ipb.len;
				if (len < needlen)
					goto partial;
				ipb.str = p;
				p += ipb.len;
				break;
			}
		bp += needlen;
		len -= needlen;

		/* Call the underlying routine. */
		if (foff <= IPO_EVENT_MAX && iplist[foff - 1](&ipb))
			break;
	}
partial:
	if ((*lenp = len) != 0)
		memmove(s_bp, bp, len);
	return (0);
}
