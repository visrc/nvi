/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 5.41 1993/03/25 14:59:18 bostic Exp $ (Berkeley) $Date: 1993/03/25 14:59:18 $
 */

#include <limits.h>		/* XXX */
#include <termios.h>
#include <regex.h>
#include <db.h>

#include "mark.h"
#include "msg.h"
#include "search.h"
#include "tag.h"

/* Undo direction. */
enum udirection { UBACKWARD, UFORWARD };

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
#define	BINC(sp, lp, llen, nlen) {					\
	if ((nlen) > llen &&						\
	    binc(sp, &(lp), &(llen), nlen))				\
		return (1);						\
}
int	binc __P((SCR *, void *, size_t *, size_t));

/* Filter type. */
enum filtertype { STANDARD, NOINPUT, NOOUTPUT };

/* Display characters. */
#define	CHARNAME(sp, c)	((sp)->cname[c & UCHAR_MAX])

#include "extern.h"
