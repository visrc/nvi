/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: args.h,v 8.1 1993/12/02 10:27:47 bostic Exp $ (Berkeley) $Date: 1993/12/02 10:27:47 $
 */

/*
 * Structure for building "argc/argv" vector of arguments.
 *
 * !!!
 * All arguments are nul terminated as well as having an
 * associated length.  The argument vector is NULL terminated.
 */
typedef struct _args {
	CHAR_T	*bp;		/* Argument. */
	size_t	 blen;		/* Buffer length. */
	size_t	 len;		/* Argument length. */

#define	A_ALLOCATED	0x01	/* If allocated space. */
	u_char	 flags;
} ARGS;
