/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ip_read.c,v 8.16 1996/12/18 10:28:03 bostic Exp $ (Berkeley) $Date: 1996/12/18 10:28:03 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
 
#include "../common/common.h"
#include "../ex/script.h"
#include "../ipc/ip.h"
#include "extern.h"

extern GS *__global_list;

typedef enum { INP_OK=0, INP_EOF, INP_ERR, INP_TIMEOUT } input_t;

static input_t	ip_read __P((SCR *, IP_PRIVATE *, struct timeval *));
static int	ip_resize __P((SCR *, u_int32_t, u_int32_t));
static int	ip_trans __P((SCR *, IP_PRIVATE *, EVENT *));

/*
 * ip_event --
 *	Return a single event.
 *
 * PUBLIC: int ip_event __P((SCR *, EVENT *, u_int32_t, int));
 */
int
ip_event(sp, evp, flags, ms)
	SCR *sp;
	EVENT *evp;
	u_int32_t flags;
	int ms;
{
	IP_PRIVATE *ipp;
	struct timeval t, *tp;

	if (LF_ISSET(EC_INTERRUPT)) {		/* XXX */
		evp->e_event = E_TIMEOUT;
		return (0);
	}

	ipp = sp == NULL ? GIPP(__global_list) : IPP(sp);

	/* Discard the last command. */
	if (ipp->iskip != 0) {
		ipp->iblen -= ipp->iskip;
		memmove(ipp->ibuf, ipp->ibuf + ipp->iskip, ipp->iblen);
		ipp->iskip = 0;
	}

	/* Set timer. */
	if (ms == 0)
		tp = NULL;
	else {
		t.tv_sec = ms / 1000;
		t.tv_usec = (ms % 1000) * 1000;
		tp = &t;
	}

	/* Read input events. */
	for (;;) {
		switch (ip_read(sp, ipp, tp)) {
		case INP_OK:
			if (!ip_trans(sp, ipp, evp))
				continue;
			break;
		case INP_EOF:
			evp->e_event = E_EOF;
			break;
		case INP_ERR:
			evp->e_event = E_ERR;
			break;
		case INP_TIMEOUT:
			evp->e_event = E_TIMEOUT;
			break;
		default:
			abort();
		}
		break;
	}
	return (0);
}

/*
 * ip_read --
 *	Read characters from the input.
 */
static input_t
ip_read(sp, ipp, tp)
	SCR *sp;
	IP_PRIVATE *ipp;
	struct timeval *tp;
{
	struct timeval poll;
	GS *gp;
	SCR *tsp;
	fd_set rdfd;
	input_t rval;
	size_t blen;
	int maxfd, nr;
	char *bp;

	gp = sp == NULL ? __global_list : sp->gp;
	bp = ipp->ibuf + ipp->iblen;
	blen = sizeof(ipp->ibuf) - ipp->iblen;

	/*
	 * 1: A read with an associated timeout, e.g., trying to complete
	 *    a map sequence.  If input exists, we fall into #2.
	 */
	FD_ZERO(&rdfd);
	poll.tv_sec = 0;
	poll.tv_usec = 0;
	if (tp != NULL) {
		FD_SET(ipp->i_fd, &rdfd);
		switch (select(ipp->i_fd + 1,
		    &rdfd, NULL, NULL, tp == NULL ? &poll : tp)) {
		case 0:
			return (INP_TIMEOUT);
		case -1:
			goto err;
		default:
			break;
		}
	}
	
	/*
	 * 2: Wait for input.
	 *
	 * Select on the command input and scripting window file descriptors.
	 * It's ugly that we wait on scripting file descriptors here, but it's
	 * the only way to keep from locking out scripting windows.
	 */
	if (sp != NULL && F_ISSET(gp, G_SCRWIN)) {
loop:		FD_ZERO(&rdfd);
		FD_SET(ipp->i_fd, &rdfd);
		maxfd = ipp->i_fd;
		for (tsp = gp->dq.cqh_first;
		    tsp != (void *)&gp->dq; tsp = tsp->q.cqe_next)
			if (F_ISSET(sp, SC_SCRIPT)) {
				FD_SET(sp->script->sh_master, &rdfd);
				if (sp->script->sh_master > maxfd)
					maxfd = sp->script->sh_master;
			}
		switch (select(maxfd + 1, &rdfd, NULL, NULL, NULL)) {
		case 0:
			abort();
		case -1:
			goto err;
		default:
			break;
		}
		if (!FD_ISSET(ipp->i_fd, &rdfd)) {
			if (sscr_input(sp))
				return (INP_ERR);
			goto loop;
		}
	}

	/*
	 * 3: Read the input.
	 */
	switch (nr = read(ipp->i_fd, bp, blen)) {
	case  0:				/* EOF. */
		rval = INP_EOF;
		break;
	case -1:				/* Error or interrupt. */
err:		rval = INP_ERR;
		msgq(sp, M_SYSERR, "input");
		break;
	default:				/* Input characters. */
		ipp->iblen += nr;
		rval = INP_OK;
		break;
	}
	return (rval);
}

/*
 * ip_trans --
 *	Translate messages into events.
 */
static int
ip_trans(sp, ipp, evp)
	SCR *sp;
	IP_PRIVATE *ipp;
	EVENT *evp;
{
	u_int32_t skip, val;
	char *fmt;

	switch (ipp->ibuf[0]) {
	case VI_C_BOL:
	case VI_C_BOTTOM:
	case VI_C_DEL:
	case VI_C_EOL:
	case VI_C_INSERT:
	case VI_C_LEFT:
	case VI_C_RIGHT:
	case VI_C_TOP:
	case VI_QUIT:
	case VI_TAG:
	case VI_TAGSPLIT:
	case VI_UNDO:
	case VI_WQ:
	case VI_WRITE:
		evp->e_event = E_IPCOMMAND;
		evp->e_ipcom = ipp->ibuf[0];
		ipp->iskip = IPO_CODE_LEN;
		return (1);
	case VI_C_DOWN:
	case VI_C_PGDOWN:
	case VI_C_PGUP:
	case VI_C_UP:
	case VI_C_SETTOP:
		evp->e_event = E_IPCOMMAND;
		evp->e_ipcom = ipp->ibuf[0];
		fmt = "1";
		break;
	case VI_C_SEARCH:
		evp->e_event = E_IPCOMMAND;
		evp->e_ipcom = ipp->ibuf[0];
		fmt = "a1";
		break;
	case VI_EDIT:
	case VI_EDITSPLIT:
	case VI_TAGAS:
	case VI_WRITEAS:
		evp->e_event = E_IPCOMMAND;
		evp->e_ipcom = ipp->ibuf[0];
		fmt = "a";
		break;
	case VI_EDITOPT:
		evp->e_event = E_IPCOMMAND;
		evp->e_ipcom = ipp->ibuf[0];
		fmt = "ab1";
		break;
	case VI_EOF:
		evp->e_event = E_EOF;
		ipp->iskip = IPO_CODE_LEN;
		return (1);
	case VI_ERR:
		evp->e_event = E_ERR;
		ipp->iskip = IPO_CODE_LEN;
		return (1);
	case VI_INTERRUPT:
		evp->e_event = E_INTERRUPT;
		ipp->iskip = IPO_CODE_LEN;
		return (1);
	case VI_MOUSE_MOVE:
	case VI_SEL_END:
	case VI_SEL_START:
		evp->e_event = E_IPCOMMAND;
		evp->e_ipcom = ipp->ibuf[0];
		fmt = "12";
		break;
	case VI_RESIZE:
		evp->e_event = E_WRESIZE;
		fmt = "12";
		break;
	case VI_SIGHUP:
		evp->e_event = E_SIGHUP;
		ipp->iskip = IPO_CODE_LEN;
		return (1);
	case VI_SIGTERM:
		evp->e_event = E_SIGTERM;
		ipp->iskip = IPO_CODE_LEN;
		return (1);
	case VI_STRING:
		 evp->e_event = E_STRING;
		 fmt = "a";
		 break;
	default:
		/*
		 * XXX: Protocol is out of sync?
		 */
		abort();
	}

	for (skip = IPO_CODE_LEN; *fmt != '\0'; ++fmt)
		switch (*fmt) {
		case '1':
		case '2':
			if (ipp->iblen < skip + IPO_INT_LEN)
				return (0);
			memcpy(&val, ipp->ibuf + skip, IPO_INT_LEN);
			val = ntohl(val);
			if (*fmt == '1')
				evp->e_val1 = val;
			else
				evp->e_val2 = val;
			skip += IPO_INT_LEN;
			break;
		case 'a':
		case 'b':
			if (ipp->iblen < skip + IPO_INT_LEN)
				return (0);
			memcpy(&val, ipp->ibuf + skip, IPO_INT_LEN);
			val = ntohl(val);
			skip += IPO_INT_LEN;
			if (ipp->iblen < skip + val)
				return (0);
			if (*fmt == 'a') {
				evp->e_str1 = ipp->ibuf + skip;
				evp->e_len1 = val;
			} else {
				evp->e_str2 = ipp->ibuf + skip;
				evp->e_len2 = val;
			}
			skip += val;
			break;
		}

	ipp->iskip = skip;

	if (evp->e_event == E_WRESIZE)
		(void)ip_resize(sp, evp->e_val1, evp->e_val2);

	return (1);
}

/* 
 * ip_resize --
 *	Reset the options for a resize event.
 */
static int
ip_resize(sp, lines, columns)
	SCR *sp;
	u_int32_t lines, columns;
{
	GS *gp;
	int rval;

	/*
	 * XXX
	 * The IP screen has to know the lines and columns before anything
	 * else happens.  So, we may not have a valid SCR pointer, and we
	 * have to deal with that.
	 */
	if (sp == NULL) {
		gp = __global_list;
		OG_VAL(gp, GO_LINES) = OG_D_VAL(gp, GO_LINES) = lines;
		OG_VAL(gp, GO_COLUMNS) = OG_D_VAL(gp, GO_COLUMNS) = columns;
		return (0);
	}

	rval = api_opts_set(sp, "lines", NULL, lines, 0);
	if (api_opts_set(sp, "columns", NULL, columns, 0))
		rval = 1;
	return (rval);
}
