/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ip_send.c,v 8.5 1996/12/14 14:03:09 bostic Exp $ (Berkeley) $Date: 1996/12/14 14:03:09 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "../ip/ip.h"
#include "ipc_extern.h"

extern int vi_ofd;				/* Output file descriptor. */

/*
 * __vi_send --
 *	Construct and send an IP buffer.
 *
 * PUBLIC: int __vi_send __P((char *, IP_BUF *));
 */
int
__vi_send(fmt, ipbp)
	char *fmt;
	IP_BUF *ipbp;
{
	static char *bp;
	static size_t blen;
	size_t off;
	u_int32_t ilen;
	int nlen, n, nw;
	char *p, *s;

	/*
	 * Have not created the channel to vi yet?  -- RAZ
	 *
	 * XXX
	 * How is that possible!?!?
	 */
	if (vi_ofd == 0)
		return (0);

	if (blen == 0 && (bp = malloc(blen = 512)) == NULL)
		return (1);

	p = bp;
	nlen = 0;
	*p++ = ipbp->code;
	nlen += IPO_CODE_LEN;

	if (fmt != NULL)
		for (; *fmt != '\0'; ++fmt)
			switch (*fmt) {
			case '1':				/* Value #1. */
				ilen = htonl(ipbp->val1);
				goto value;
			case '2':				/* Value #2. */
				ilen = htonl(ipbp->val2);
				goto value;
			case '3':				/* Value #3. */
				ilen = htonl(ipbp->val3);
value:				nlen += IPO_INT_LEN;
				if (nlen >= blen) {
					blen = blen * 2 + nlen;
					off = p - bp;
					if ((bp = realloc(bp, blen)) == NULL)
						return (1);
					p = bp + off;
				}
				memcpy(p, &ilen, IPO_INT_LEN);
				p += IPO_INT_LEN;
				break;
			case 'a':				/* String #1. */
			case 'b':				/* String #2. */
				ilen = *fmt == 'a' ? ipbp->len1 : ipbp->len2;
				nlen += IPO_INT_LEN + ilen;
				if (nlen >= blen) {
					blen = blen * 2 + nlen;
					off = p - bp;
					if ((bp = realloc(bp, blen)) == NULL)
						return (1);
					p = bp + off;
				}
				ilen = htonl(ilen);
				memcpy(p, &ilen, IPO_INT_LEN);
				p += IPO_INT_LEN;
				if (*fmt == 'a') {
					memcpy(p, ipbp->str1, ipbp->len1);
					p += ipbp->len1;
				} else {
					memcpy(p, ipbp->str2, ipbp->len2);
					p += ipbp->len2;
				}
				break;
			}
	for (n = p - bp, p = bp; n > 0; n -= nw, p += nw)
		if ((nw = write(vi_ofd, p, n)) < 0)
			return (1);
	return (0);
}
