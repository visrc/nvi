/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ip_send.c,v 8.2 1996/12/10 18:01:51 bostic Exp $ (Berkeley) $Date: 1996/12/10 18:01:51 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../common/common.h"
#include "../ip_vi/ip.h"
#include "ipc_extern.h"

static void nomem()
{
    puts( "out of memory" );
    exit(1);
}

/*
 * ip_send --
 *	Construct and send an IP buffer.
 *
 * PUBLIC: int ip_send __P((char *, IP_BUF *));
 */
int
ip_send(fmt, ipbp)
	char *fmt;
	IP_BUF *ipbp;
{
	extern int o_fd;
	static char *bp;
	static size_t blen;
	size_t off;
	u_int32_t ilen;
	int nlen, n, nw;
	char *p;

	/*
	 * Have not created the channel to vi yet?  -- RAZ
	 *
	 * XXX
	 * How is that possible!?!?
	 */
	if (o_fd == 0)
		return (0);

	if (blen == 0 && (bp = malloc(blen = 512)) == NULL)
		nomem();

	p = bp;
	nlen = 0;
	*p++ = ipbp->code;
	nlen += IPO_CODE_LEN;

	if (fmt != NULL)
		for (; *fmt != '\0'; ++fmt)
			switch (*fmt) {
			case '1':			/* Value 1. */
				ilen = htonl(ipbp->val1);
				goto value;
			case '2':			/* Value 2. */
				ilen = htonl(ipbp->val2);
value:				nlen += IPO_INT_LEN;
				if (nlen >= blen) {
					blen = blen * 2 + nlen;
					off = p - bp;
					if ((bp = realloc(bp, blen)) == NULL)
						nomem();
					p = bp + off;
				}
				memmove(p, &ilen, IPO_INT_LEN);
				p += IPO_INT_LEN;
				break;
			case 's':			/* String. */
				ilen = ipbp->len;	/* XXX: conversion. */
				ilen = htonl(ilen);
				nlen += IPO_INT_LEN + ipbp->len;
				if (nlen >= blen) {
					blen = blen * 2 + nlen;
					off = p - bp;
					if ((bp = realloc(bp, blen)) == NULL)
						nomem();
					p = bp + off;
				}
				memmove(p, &ilen, IPO_INT_LEN);
				p += IPO_INT_LEN;
				memmove(p, ipbp->str, ipbp->len);
				p += ipbp->len;
				break;
			}
#ifdef TR
	trace("WROTE: ");
	for (n = p - bp, p = bp; n > 0; --n, ++p)
		if (isprint(*p))
			(void)trace("%c", *p);
		else
			trace("<%x>", (u_char)*p);
	trace("\n");
#endif

	for (n = p - bp, p = bp; n > 0; n -= nw, p += nw)
		if ((nw = write(o_fd, p, n)) < 0) {
			perror("ip_cl: write");
			exit(1);
		}

	return (0);
}
