/*-
 * Copyright (c) 2000
 *	Sven Verdoolaege.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

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

static void	   perr __P((char *, char *));

/*
 * gs_init --
 *	Create and partially initialize the GS structure.
 * PUBLIC: GS * gs_init __P((char*));
 */
GS *
gs_init(name)
	char *name;
{
	GS *gp;
	char *p;

	/* Figure out what our name is. */
	if ((p = strrchr(name, '/')) != NULL)
		name = p + 1;

	/* Allocate the global structure. */
	CALLOC_NOMSG(NULL, gp, GS *, 1, sizeof(GS));
	if (gp == NULL)
		perr(name, NULL);

	gp->progname = name;

	/* Common global structure initialization. */
	/* others will need to be copied from main.c */
	CIRCLEQ_INIT(&gp->dq);

	thread_init(gp);

	return (gp);
}

/*
 * gs_new_win
 *	Create new window
 * PUBLIC: WIN * gs_new_win __P((GS *gp));
 */

WIN *
gs_new_win(GS *gp)
{
	WIN *wp;

	CALLOC_NOMSG(NULL, wp, WIN *, 1, sizeof(*wp));
	if (!wp)
		return NULL;

	/* Common global structure initialization. */
	LIST_INIT(&wp->ecq);
	LIST_INSERT_HEAD(&wp->ecq, &wp->excmd, q);

	CIRCLEQ_INSERT_TAIL(&gp->dq, wp, q);
	CIRCLEQ_INIT(&wp->scrq);

	wp->gp = gp;

	return wp;
}


/*
 * perr --
 *	Print system error.
 */
static void
perr(name, msg)
	char *name, *msg;
{
	(void)fprintf(stderr, "%s:", name);
	if (msg != NULL)
		(void)fprintf(stderr, "%s:", msg);
	(void)fprintf(stderr, "%s\n", strerror(errno));
	exit(1);
}
