/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 5.30 1992/11/06 20:07:02 bostic Exp $ (Berkeley) $Date: 1992/11/06 20:07:02 $
 */

#include <db.h>				/* XXX for rptlines, below */

/* Confirmation stuff. */
#define	CONFIRMCHAR	'y'		/* Make change character. */
#define	QUITCHAR	'q'		/* Quit character. */
enum confirmation { YES, NO, QUIT };

/*
 * Most of the arrays, names, etc. in ex/vi are u_char's, since we want
 * it to be 8-bit clean.  Several of the C-library routines take char *'s,
 * though.  The following #defines try to handle this problem.
 *
 * XXX
 * I think that the correct approach is to have a set of data types as
 * follows:
 *	typedef char 		CFNAME	(file name character)
 *	typedef unsigned char	CDATA	(data character)
 *	typedef unsigned char	CINPUT	(input, command character)
 */
#define	UATOI(a)		atoi((char *)(a))
#define	URINDEX(a, b)		(u_char *)rindex((char *)(a), b)
#define	USTRCMP(a, b)		strcmp((char *)(a), (char *)(b))
#define	USTRDUP(a)		(u_char *)strdup((char *)(a))
#define	USTRLEN(a)		strlen((char *)(a))
#define	USTRSEP(a, b)		(u_char *)strsep((char **)(a), b)
#define	USTRNCMP(a, b, n)	strncmp((char *)(a), (char *)(b), n)
#define	USTRPBRK(a, b)		(u_char *)strpbrk((char *)(a), (char *)(b))
#define	USTRTOL(a, b, c)	strtol((char *)(a), (char **)(b), c)

/* misc housekeeping variables & functions				  */

extern int	doingglobal;	/* boolean: are doing a ":g" command? */
extern recno_t	rptlines;	/* number of lines affected by a command */
extern char	*rptlabel;	/* description of how lines were affected */
