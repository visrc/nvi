/*-
 * Copyright (c) 2000
 *	Sven Verdoolaege.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: pthread.c,v 1.3 2000/07/14 14:29:17 skimo Exp $ (Berkeley) $Date: 2000/07/14 14:29:17 $";
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

#include <pthread.h>

#include "../common/common.h"

static void vi_pthread_run __P((WIN *wp, void *(*fun)(void*), void *data));

/*
 * thread_init
 *
 * PUBLIC: void thread_init __P((GS *gp));
 */
void 
thread_init(GS *gp)
{
	gp->run = vi_pthread_run;
}

static void
vi_pthread_run(WIN *wp, void *(*fun)(void*), void *data)
{
	pthread_t *t = malloc(sizeof(pthread_t));
	pthread_create(t, NULL, fun, data);
}
