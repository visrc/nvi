/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 5.45 1993/04/06 11:36:37 bostic Exp $ (Berkeley) $Date: 1993/04/06 11:36:37 $
 */

#include <db.h>				/* Ordered before local includes. */
#include <glob.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>

/*
 * Forward structure declarations.  Not pretty, but the include files
 * are far too interrelated for a clean solution.
 */
struct _cb;
struct _excmdlist;
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

#include "hdr.h"			/* Include before local includes. */

#include "mark.h"			/* Include before cut.h. */
#include "cut.h"

#include "search.h"			/* Include before screen.h. */
#include "options.h"			/* Include before screen.h. */
#include "screen.h"

#include "char.h"
#include "exf.h"
#include "gs.h"
#include "log.h"
#include "msg.h"
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
	    struct _mark *, struct _mark *, char *, enum filtertype));

/* Portability stuff. */
#ifndef DEFFILEMODE			/* Default file permissions. */
#define	DEFFILEMODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif

/* Portability stuff. */
typedef void (*sig_ret_t) __P((int));	/* Type of signal function. */
