/*-
 * Copyright (c) 2000
 *	Sven Verdoolaege.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: nothread.c,v 1.1 2000/07/02 20:36:27 skimo Exp $ (Berkeley) $Date: 2000/07/02 20:36:27 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"

static void vi_nothread_run __P((WIN *wp, void (*fun)(void*), void *data));

/*
 * thread_init
 *
 * PUBLIC: void thread_init __P((WIN *wp));
 */
void 
thread_init(GS *gp)
{
	gp->run = vi_nothread_run;
}

static void
vi_nothread_run(WIN *wp, void (*fun)(void*), void *data)
{
	fun(data);
}
