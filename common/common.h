/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 10.1 1995/04/13 17:18:10 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:18:10 $
 */

/*
 * Forward structure declarations.  Not pretty, but the include files
 * are far too interrelated for a clean solution.
 */
typedef struct _cb		CB;
typedef struct _event		EVENT;
typedef struct _excmd		EXCMD;
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

/* Autoindent state. */
typedef enum { C_NOTSET, C_CARATSET, C_NOCHANGE, C_ZEROSET } carat_t;

/*
 * Routines that return a confirmation return:
 *
 *	CONF_NO		User answered no.
 *	CONF_QUIT	User answered quit, eof or an error.
 *	CONF_YES	User answered yes.
 */
typedef enum { CONF_NO, CONF_QUIT, CONF_YES } conf_t;

/* Directions. */
typedef enum { NOTSET, FORWARD, BACKWARD } dir_t;

/* Line operations. */
typedef enum { LINE_APPEND, LINE_DELETE, LINE_INSERT, LINE_RESET } lnop_t;

/* Lock return values. */
typedef enum { LOCK_FAILED, LOCK_SUCCESS, LOCK_UNAVAIL } lock_t;

/* Sequence types. */
typedef enum { SEQ_ABBREV, SEQ_COMMAND, SEQ_INPUT } seq_t;

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
#include "util.h"		/* Required by excmd.h. */
#include "mark.h"		/* Required by gs.h. */
#include "excmd.h"		/* Required by gs.h. */
#include "gs.h"			/* Required by screen.h. */
#include "screen.h"		/* Required by exf.h. */
#include "exf.h"
#include "log.h"
#include "mem.h"

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
