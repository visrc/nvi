/*-
 * Copyright (c) 1996
 *	Rob Zimmermann.  All rights reserved.
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/uio.h>

#include "../common/common.h"
#include "ip.h"

static int vi_close __P((IPVI *));
static int vi_new_window __P((IPVI *, IPVIWIN **));

/* 
 * vi_create
 *
 * PUBLIC: int vi_create __P((IPVI **, u_int32_t));
 */
int
vi_create(ipvip, flags)
	IPVI	    **ipvip;
	u_int32_t     flags;
{
	IPVI	*ipvi;

	MALLOC_GOTO(NULL, ipvi, IPVI*, sizeof(IPVI));
	memset(ipvi, 0, sizeof(IPVI));

	ipvi->flags = flags;

	ipvi->run = vi_run;
	ipvi->new_window = vi_new_window;
	ipvi->close = vi_close;

	*ipvip = ipvi;

	return 0;

alloc_err:
	return 1;
}

static int 
vi_new_window (IPVI *ipvi, IPVIWIN **ipviwinp)
{
	IPVIWIN	*ipviwin;

	MALLOC_GOTO(NULL, ipviwin, IPVIWIN*, sizeof(IPVIWIN));
	memset(ipviwin, 0, sizeof(IPVIWIN));

	if (0) {
	ipviwin->ifd = ipvi->ifd;
	ipviwin->ofd = ipvi->ofd;
	} else {
	int sockets[2];
	struct msghdr   mh;
	IPCMSGHDR	    ch;
	char	    dummy;
	struct iovec    iov;

	socketpair(AF_LOCAL, SOCK_STREAM, 0, sockets);

	mh.msg_namelen = 0;
	mh.msg_iovlen = 1;
	mh.msg_iov = &iov;
	mh.msg_controllen = sizeof(ch);
	mh.msg_control = (void *)&ch;

	iov.iov_len = 1;
	iov.iov_base = &dummy;

	ch.header.cmsg_level = SOL_SOCKET;
	ch.header.cmsg_type = SCM_RIGHTS;
	ch.header.cmsg_len = sizeof(ch);

	*(int *)CMSG_DATA(&ch.header) = sockets[1];
	sendmsg(ipvi->ofd, &mh, 0);
	*(int *)CMSG_DATA(&ch.header) = sockets[1];
	sendmsg(ipvi->ofd, &mh, 0);
	close(sockets[1]);

	ipviwin->ifd = sockets[0];
	ipviwin->ofd = sockets[0];
	}

	*ipviwinp = ipviwin;

	return 0;

alloc_err:
	return 1;
}

static int  vi_close(ipvi)
	IPVI *ipvi;
{
	memset(ipvi, 6, sizeof(IPVI));
	free(ipvi);
	return 0;
}

