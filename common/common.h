/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 5.37 1993/02/19 13:39:05 bostic Exp $ (Berkeley) $Date: 1993/02/19 13:39:05 $
 */

#include <limits.h>		/* XXX */
#include <db.h>
#include <regex.h>

#include "mark.h"
#include "msg.h"
#include "exf.h"
#include "cut.h"

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
#define	USTRCMP(a, b)		strcmp((char *)(a), (char *)(b))
#define	USTRDUP(a)		(u_char *)strdup((char *)(a))
#define	USTRLEN(a)		strlen((char *)(a))
#define	USTRNCMP(a, b, n)	strncmp((char *)(a), (char *)(b), n)
#define	USTRPBRK(a, b)		(u_char *)strpbrk((char *)(a), (char *)(b))
#define	USTRRCHR(a, b)		(u_char *)strrchr((char *)(a), b)
#define	USTRSEP(a, b)		(u_char *)strsep((char **)(a), b)
#define	USTRTOL(a, b, c)	strtol((char *)(a), (char **)(b), c)

/* Confirmation stuff. */
#define	CONFIRMCHAR	'y'		/* Make change character. */
#define	QUITCHAR	'q'		/* Quit character. */
enum confirmation { YES, NO, QUIT };

#ifndef DEFFILEMODE			/* Default file permissions. */
#define	DEFFILEMODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif

typedef void (*sig_ret_t) __P((int));	/* Type of signal function. */

/* Buffer allocation. */
#define	BINC(ep, lp, llen, nlen) {					\
	if ((nlen) > llen &&						\
	    binc(ep, &(lp), &(llen), nlen))				\
		return (1);						\
}
int	binc __P((EXF *, void *, size_t *, size_t));

/* Filter type. */
enum filtertype { STANDARD, NOINPUT, NOOUTPUT };
int	filtercmd __P((EXF *, MARK *, MARK *, u_char *, enum filtertype));

/* Visual bell. */
extern char *VB;

/* Editor mode. */
enum editmode {MODE_EX, MODE_VI, MODE_QUIT};
extern enum editmode mode;

/* Display characters. */
#define	CHARNAME(c)	(asciiname[c & 0xff])
extern char *asciiname[UCHAR_MAX + 1];
extern u_char asciilen[UCHAR_MAX + 1];

#include "extern.h"
