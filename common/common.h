/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 8.49 1994/10/09 14:03:44 bostic Exp $ (Berkeley) $Date: 1994/10/09 14:03:44 $
 */

/*
 * Forward structure declarations.  Not pretty, but the include files
 * are far too interrelated for a clean solution.
 */
typedef struct _cb		CB;
typedef struct _ch		CH;
typedef struct _excmdarg	EXCMDARG;
typedef struct _exf		EXF;
typedef struct _fref		FREF;
typedef struct _gs		GS;
typedef struct _lmark		LMARK;
typedef struct _mark		MARK;
typedef struct _msg		MSG;
typedef struct _option		OPTION;
typedef struct _optlist		OPTLIST;
typedef struct _scr		SCR;
typedef struct _script		SCRIPT;
typedef struct _seq		SEQ;
typedef struct _tag		TAG;
typedef struct _tagf		TAGF;
typedef struct _text		TEXT;

/*
 * Local includes.
 */
#include "term.h"		/* Required by args.h. */
#include "args.h"		/* Required by options.h. */
#include "options.h"		/* Required by screen.h. */
#include "search.h"		/* Required by screen.h. */

#include "msg.h"		/* Required by gs.h. */
#include "cut.h"		/* Required by gs.h. */
#include "seq.h"		/* Required by screen.h. */
#include "gs.h"			/* Required by screen.h. */
#include "screen.h"		/* Required by exf.h. */
#include "mark.h"		/* Required by exf.h. */
#include "exf.h"
#include "log.h"
#include "mem.h"
#include "util.h"

/* Macros to set/clear/test flags. */
#define	F_SET(p, f)	(p)->flags |= (f)
#define	F_CLR(p, f)	(p)->flags &= ~(f)
#define	F_ISSET(p, f)	((p)->flags & (f))

#define	LF_INIT(f)	flags = (f)
#define	LF_SET(f)	flags |= (f)
#define	LF_CLR(f)	flags &= ~(f)
#define	LF_ISSET(f)	(flags & (f))

/*
 * !!!
 * Fake the 4.4BSD fwopen(3) routines.  See PORT/clib/fwopen.c.
 */
#if FWOPEN_NOT_AVAILABLE
#define	EXCOOKIE	sp
int	 ex_fflush __P((SCR *));
int	 ex_printf __P((SCR *, const char *, ...));
FILE	*fwopen __P((SCR *, void *));
#else
#define	EXCOOKIE	sp->stdfp
#define	ex_fflush	fflush
#define	ex_printf	fprintf
#endif
