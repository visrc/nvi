/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 5.28 1992/10/20 18:24:07 bostic Exp $ (Berkeley) $Date: 1992/10/20 18:24:07 $
 */

#include <db.h>				/* XXX for rptlines, below */

#define	CARRIAGE_RETURN	'\r'		/* Carriage return. */
#define	CHEND		'$'		/* End-of-change character. */
#define	ESCAPE		'\033'		/* Escape character. */
#define	SPACE		' '		/* Space character. */

#define	TAB	8			/* XXX -- Settable? */

enum direction { FORWARD, BACKWARD };	/* Direction type. */

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
#define	USTRNCMP(a, b, n)	strncmp((char *)(a), (char *)(b), n)
#define	USTRPBRK(a, b)		(u_char *)strpbrk((char *)(a), (char *)(b))
#define	USTRTOL(a, b, c)	strtol((char *)(a), (char **)(b), c)

/* misc housekeeping variables & functions				  */

extern long	changes;	/* counts changes, to prohibit short-cuts */
extern int	doingglobal;	/* boolean: are doing a ":g" command? */
extern recno_t	rptlines;	/* number of lines affected by a command */
extern char	*rptlabel;	/* description of how lines were affected */
