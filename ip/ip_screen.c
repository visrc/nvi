/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ip_screen.c,v 8.1 1996/09/20 19:36:06 bostic Exp $ (Berkeley) $Date: 1996/09/20 19:36:06 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>

#include "../common/common.h"
#include "ip.h"

/*
 * ip_screen --
 *	Initialize/shutdown the IP screen.
 *
 * PUBLIC: int ip_screen __P((SCR *, u_int32_t));
 */
int
ip_screen(sp, flags)
	SCR *sp;
	u_int32_t flags;
{
	IP_PRIVATE *ipp;

	ipp = IPP(sp);

	/* See if we're already in the right mode. */
	if (LF_ISSET(SC_VI) && F_ISSET(ipp, IP_SCR_VI_INIT))
		return (0);

	/* Ex isn't possible. */
	if (LF_ISSET(SC_EX))
		return (1);

	/* Initialize terminal based information. */
	if (ip_term_init(sp)) 
		return (1);

	/* Put up the first file name. */
	if (ip_rename(sp))
		return (1);

	F_SET(ipp, IP_SCR_VI_INIT);
	return (0);
}

/*
 * ip_quit --
 *	Shutdown the screens.
 *
 * PUBLIC: int ip_quit __P((GS *));
 */
int
ip_quit(gp)
	GS *gp;
{
	IP_PRIVATE *ipp;
	int rval;

	/* Clean up the terminal mappings. */
	rval = ip_term_end(gp);

	ipp = GIPP(gp);
	F_CLR(ipp, IP_SCR_VI_INIT);

	return (rval);
}
