/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 5.42 1993/03/26 13:38:01 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:38:01 $
 */

#include <db.h>				/* Ordered before local includes. */
#include <limits.h>
#include <regex.h>
#include <stdio.h>

/*
 * Forward structure declarations.  Not pretty, but the include files
 * are far too interrelated for a clean solution.
 */
struct _cb;
struct _exf;
struct _gs;
struct _hdr;
struct _ib;
struct _mark;
struct _msg;
struct _option;
struct _scr;
struct _seq;
struct _tag;
struct _tagf;
struct _text;

#include "hdr.h"			/* Insert before local includes. */

#include "mark.h"			/* Insert before cut.h. */
#include "cut.h"

#include "search.h"			/* Insert before screen.h. */
#include "screen.h"

#include "exf.h"
#include "gs.h"
#include "log.h"
#include "msg.h"
#include "options.h"
#include "pathnames.h"
#include "seq.h"
#include "term.h"

#include "extern.h"

/* Macros to set/clear/test flags. */
#define	F_SET(p, f)	(p)->flags |= (f)
#define	F_CLR(p, f)	(p)->flags &= ~(f)
#define	F_ISSET(p, f)	((p)->flags & (f))

/* Buffer allocation. */
#define	BINC(sp, lp, llen, nlen) {					\
	if ((nlen) > llen &&						\
	    binc(sp, &(lp), &(llen), nlen))				\
		return (1);						\
}
int	binc __P((struct _scr *, void *, size_t *, size_t));

/* Filter type. */
enum filtertype { STANDARD, NOINPUT, NOOUTPUT };
int	filtercmd __P((struct _scr *, struct _exf *, struct _mark *,
	    struct _mark *, struct _mark *, u_char *, enum filtertype));

/* Portability stuff. */
#ifndef DEFFILEMODE			/* Default file permissions. */
#define	DEFFILEMODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif

/* Portability stuff. */
typedef void (*sig_ret_t) __P((int));	/* Type of signal function. */

/* Portability stuff. */
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
