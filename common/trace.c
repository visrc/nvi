/*-
 * Copyright (c) 1996
 *	Rob Zimmermann.  All rights reserved.
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: trace.c,v 8.1 1996/12/12 09:16:24 bostic Exp $ (Berkeley) $Date: 1996/12/12 09:16:24 $";
#endif /* not lint */

#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "common.h"

#ifdef TRACE

static FILE *tfp;

/*
 * trace_end --
 *	End tracing.
 *
 * PUBLIC: void trace_end __P((void));
 */
void
trace_end()
{
	if (tfp != NULL && tfp != stderr)
		(void)fclose(tfp);
}

/*
 * trace_init --
 *	Initialize tracing.
 *
 * PUBLIC: void trace_init __P((char *));
 */
void
trace_init(name)
	char *name;
{
	if (name == NULL || (tfp = fopen(name, "w")) == NULL)
		tfp = stderr;
	trace("\n=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\nTRACE\n");
}

/*
 * trace --
 *	Debugging trace routine.
 *
 * PUBLIC: void trace __P((const char *, ...));
 */
void
#ifdef __STDC__
trace(const char *fmt, ...)
#else
trace(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

	if (tfp == NULL)
		trace_init(NULL);

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(tfp, fmt, ap);
	va_end(ap);

	(void)fflush(tfp);
}
#endif
