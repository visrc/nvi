/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 5.27 1992/10/10 13:34:59 bostic Exp $ (Berkeley) $Date: 1992/10/10 13:34:59 $
 */

#include <db.h>				/* XXX for rptlines, below */

#define	ESCAPE	'\033'			/* Escape character. */
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

extern long	nlines;		/* number of lines in the file */
extern long	changes;	/* counts changes, to prohibit short-cuts */
extern int	doingglobal;	/* boolean: are doing a ":g" command? */
extern recno_t	rptlines;	/* number of lines affected by a command */
extern char	*rptlabel;	/* description of how lines were affected */

/* Describe the current state. */
#define WHEN_VICMD	0x0001	/* getkey: reading a VI command */
#define WHEN_VIINP	0x0002	/* getkey: in VI's INPUT mode */
#define WHEN_VIREP	0x0004	/* getkey: in VI's REPLACE mode */
#define WHEN_EX		0x0008	/* getkey: in EX mode */
#define WHEN_MSG	0x0010	/* getkey: at a "more" prompt */
#define WHEN_REP1	0x0040	/* getkey: getting a single char for 'r' */
#define WHEN_CUT	0x0080	/* getkey: getting a cut buffer name */
#define WHEN_MARK	0x0100	/* getkey: getting a mark name */
#define WHEN_CHAR	0x0200	/* getkey: getting a destination for f/F/t/T */
#define WHENMASK	(WHEN_VICMD|WHEN_VIINP|WHEN_VIREP|WHEN_REP1|WHEN_CUT|WHEN_MARK|WHEN_CHAR)
