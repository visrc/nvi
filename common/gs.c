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

	return (gp);
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
