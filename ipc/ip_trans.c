/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ip_trans.c,v 8.11 1996/12/14 14:03:22 bostic Exp $ (Berkeley) $Date: 1996/12/14 14:03:22 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>
#include <string.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "../common/common.h"
#include "../ip/ip.h"
#include "ipc_extern.h"

/*
 * __vi_trans --
 *	Translate vi messages into function calls.
 *
 * PUBLIC: int __vi_trans __P((char *, size_t *));
 */
int
__vi_trans(bp, lenp)
	char *bp;
	size_t *lenp;
{
	extern int (*__vi_iplist[SI_EVENT_MAX - 1]) __P((IP_BUF *));
	IP_BUF ipb;
	size_t len, needlen;
	u_int32_t *vp;
	int foff;
	char *fmt, *p, *s_bp, **vsp;

	for (s_bp = bp, len = *lenp; len > 0;) {
		switch (foff = bp[0]) {
		case SI_ADDSTR:
		case SI_RENAME:
			fmt = "a";
			break;
		case SI_ATTRIBUTE:
		case SI_MOVE:
			fmt = "12";
			break;
		case SI_BUSY_ON:
			fmt = "a1";
			break;
		case SI_EDITOPT:
			fmt = "ab1";
			break;
		case SI_REWRITE:
			fmt = "1";
			break;
		case SI_SCROLLBAR:
			fmt = "123";
			break;
		default:
			fmt = "";
		}

		p = bp + IPO_CODE_LEN;
		needlen = IPO_CODE_LEN;
		for (; *fmt != '\0'; ++fmt)
			switch (*fmt) {
			case '1':				/* Value #1. */
				vp = &ipb.val1;
				goto value;
			case '2':				/* Value #2. */
				vp = &ipb.val2;
				goto value;
			case '3':				/* Value #3. */
				vp = &ipb.val3;
value:				needlen += IPO_INT_LEN;
				if (len < needlen)
					goto partial;
				memcpy(vp, p, IPO_INT_LEN);
				*vp = ntohl(*vp);
				p += IPO_INT_LEN;
				break;
			case 'a':				/* String #1. */
				vp = &ipb.len1;
				vsp = &ipb.str1;
				goto string;
			case 'b':				/* String #2. */
				vp = &ipb.len2;
				vsp = &ipb.str2;
string:				needlen += IPO_INT_LEN;
				if (len < needlen)
					goto partial;
				memcpy(vp, p, IPO_INT_LEN);
				*vp = ntohl(*vp);
				p += IPO_INT_LEN;
				needlen += *vp;
				if (len < needlen)
					goto partial;
				*vsp = p;
				p += *vp;
				break;
			}
		bp += needlen;
		len -= needlen;

		/* Call the underlying routine. */
		if (foff <= SI_EVENT_MAX && __vi_iplist[foff - 1](&ipb))
			break;
	}
partial:
	if ((*lenp = len) != 0)
		memmove(s_bp, bp, len);
	return (0);
}
