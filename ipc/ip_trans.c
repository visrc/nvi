/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ip_trans.c,v 8.15 1997/08/02 16:49:33 bostic Exp $ (Berkeley) $Date: 1997/08/02 16:49:33 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <bitstring.h>
#include <stdio.h>
#include <string.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "../common/common.h"
#include "ip.h"

static char ibuf[2048];				/* Input buffer. */
static size_t ibuf_len;				/* Length of current input. */

/*
 * vi_input --
 *	Read from the vi message queue.
 *
 * PUBLIC: int vi_input __P((int));
 */
int
vi_input(fd)
	int fd;
{
	ssize_t nr;

	/* Read waiting vi messages and translate to X calls. */
	switch (nr = read(fd, ibuf + ibuf_len, sizeof(ibuf) - ibuf_len)) {
	case 0:
		return (0);
	case -1:
		return (-1);
	default:
		break;
	}
	ibuf_len += nr;

	/* Parse to data end or partial message. */
	(void)vi_translate(ibuf, &ibuf_len, NULL);

	return (ibuf_len > 0);
}

/*
 * vi_wsend --
 *	Construct and send an IP buffer, and wait for an answer.
 *
 * PUBLIC: int vi_wsend __P((char *, IP_BUF *));
 */
int
vi_wsend(fmt, ipbp)
	char *fmt;
	IP_BUF *ipbp;
{
	fd_set rdfd;
	ssize_t nr;

	if (vi_send(fmt, ipbp))
		return (1);

	FD_ZERO(&rdfd);
	ipbp->code = CODE_OOB;

	for (;;) {
		FD_SET(vi_ifd, &rdfd);
		if (select(vi_ifd + 1, &rdfd, NULL, NULL, NULL) != 0)
			return (-1);

		/* Read waiting vi messages and translate to X calls. */
		switch (nr =
		    read(vi_ifd, ibuf + ibuf_len, sizeof(ibuf) - ibuf_len)) {
		case 0:
			return (0);
		case -1:
			return (-1);
		default:
			break;
		}
		ibuf_len += nr;

		/* Parse to data end or partial message. */
		(void)vi_translate(ibuf, &ibuf_len, ipbp);

		if (ipbp->code != CODE_OOB)
			break;
	}
	return (0);
}

/*
 * vi_translate --
 *	Translate vi messages into function calls.
 *
 * PUBLIC: int vi_translate __P((char *, size_t *, IP_BUF *));
 */
int
vi_translate(bp, lenp, ipbp)
	char *bp;
	size_t *lenp;
	IP_BUF *ipbp;
{
	extern int (*__vi_iplist[SI_EVENT_MAX - 1]) __P((IP_BUF *));
	IP_BUF ipb;
	size_t len, needlen;
	u_int32_t *vp;
	char *fmt, *p, *s_bp, **vsp;

	for (s_bp = bp, len = *lenp; len > 0;) {
		switch (ipb.code = bp[0]) {
		case SI_ADDSTR:
		case SI_BUSY_ON:
		case SI_RENAME:
		case SI_SELECT:
			fmt = "a";
			break;
		case SI_ATTRIBUTE:
		case SI_MOVE:
			fmt = "12";
			break;
		case SI_EDITOPT:
			fmt = "ab1";
			break;
		case SI_REPLY:
			fmt = "1a";
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

		/*
		 * XXX
		 * Protocol error!?!?
		 */
		if (ipb.code > SI_EVENT_MAX) {
			len = 0;
			break;
		}

		/*
		 * If we're waiting for a reply and we got it, return it, and
		 * leave any unprocessed data in the buffer.  If we got a reply
		 * and we're not waiting for one, discard it -- callers wait
		 * for responses.
		 */
		if (ipb.code == SI_REPLY) {
			if (ipbp == NULL)
				continue;
			*ipbp = ipb;
			break;
		}

		/* Call the underlying routine. */
		if (__vi_iplist[ipb.code - 1](&ipb))
			break;
	}
partial:
	if ((*lenp = len) != 0)
		memmove(s_bp, bp, len);
	return (0);
}
