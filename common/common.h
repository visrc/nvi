/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 8.27 1993/11/27 15:01:23 bostic Exp $ (Berkeley) $Date: 1993/11/27 15:01:23 $
 */

/* System includes. */
#include <sys/queue.h>		/* Required by screen.h. */
#include <sys/time.h>		/* Required by screen.h. */

#include <bitstring.h>		/* Required by screen.h. */
#include <limits.h>		/* Required by screen.h. */
#include <signal.h>		/* Required by screen.h. */
#include <stdio.h>		/* Required by screen.h. */
#include <termios.h>		/* Required by gs.h. */

/*
 * Required by screen.h.  This is the first include that can pull
 * in "compat.h".  Should be after every other system include.
 */
#include <regex.h>

/*
 * Forward structure declarations.  Not pretty, but the include files
 * are far too interrelated for a clean solution.
 */
typedef struct _cb		CB;
typedef struct _chname		CHNAME;
typedef struct _excmdarg	EXCMDARG;
typedef struct _exf		EXF;
typedef struct _fref		FREF;
typedef struct _gs		GS;
typedef struct _ibuf		IBUF;
typedef struct _mark		MARK;
typedef struct _msg		MSG;
typedef struct _option		OPTION;
typedef struct _optlist		OPTLIST;
typedef struct _scr		SCR;
typedef struct _seq		SEQ;
typedef struct _tag		TAG;
typedef struct _tagf		TAGF;
typedef struct _text		TEXT;

/*
 * Fundamental character types.
 *
 * CHAR_T	An integral type that can hold any character.
 * ARG_CHAR_T	The type of a CHAR_T when passed as an argument using
 *		traditional promotion rules.  It should also be able
 *		to be compared against any CHAR_T for equality without
 *		problems.
 * MAX_CHAR_T	The maximum value of any character.
 *
 * If no integral type can hold a character, don't even try the port.
 */
typedef	u_char		CHAR_T;	
typedef	u_int		ARG_CHAR_T;
#define	MAX_CHAR_T	0xff

/*
 * Local includes.
 */
#include <db.h>			/* Required by exf.h; includes compat.h. */

#include "mark.h"		/* Required by most everybody. */

#include "search.h"		/* Required by screen.h. */
#include "options.h"		/* Required by screen.h. */
#include "term.h"		/* Required by screen.h. */

#include "msg.h"		/* Required by gs.h. */
#include "gs.h"			/* Required by screen.h. */
#include "cut.h"		/* Required by screen.h. */
#include "screen.h"		/* Required by exf.h. */

#include "exf.h"		
#include "log.h"

#if FWOPEN_NOT_AVAILABLE	/* See PORT/clib/fwopen.c. */
#define	EXCOOKIE	sp
int	 ex_fflush __P((SCR *));
int	 ex_printf __P((SCR *, const char *, ...));
FILE	*fwopen __P((SCR *, void *));
#else
#define	EXCOOKIE	sp->stdfp
#define	ex_fflush	fflush
#define	ex_printf	fprintf
#endif

/* Macros to set/clear/test flags. */
#define	F_SET(p, f)	(p)->flags |= (f)
#define	F_CLR(p, f)	(p)->flags &= ~(f)
#define	F_ISSET(p, f)	((p)->flags & (f))

#define	LF_INIT(f)	flags = (f)
#define	LF_SET(f)	flags |= (f)
#define	LF_CLR(f)	flags &= ~(f)
#define	LF_ISSET(f)	(flags & (f))

/* Memory allocation macros. */
#define	BINC(sp, lp, llen, nlen) {					\
	if ((nlen) > llen && binc(sp, &(lp), &(llen), nlen))		\
		return (1);						\
}
int	binc __P((SCR *, void *, size_t *, size_t));

#define	GET_SPACE(sp, bp, blen, nlen) {					\
	GS *__gp = (sp)->gp;						\
	if (F_ISSET(__gp, G_TMP_INUSE)) {				\
		bp = NULL;						\
		blen = 0;						\
		BINC(sp, bp, blen, nlen); 				\
	} else {							\
		BINC(sp, __gp->tmp_bp, __gp->tmp_blen, nlen);		\
		bp = __gp->tmp_bp;					\
		blen = __gp->tmp_blen;					\
		F_SET(__gp, G_TMP_INUSE);				\
	}								\
}

#define	ADD_SPACE(sp, bp, blen, nlen) {					\
	GS *__gp = (sp)->gp;						\
	if (bp == __gp->tmp_bp) {					\
		F_CLR(__gp, G_TMP_INUSE);				\
		BINC(sp, __gp->tmp_bp, __gp->tmp_blen, nlen);		\
		bp = __gp->tmp_bp;					\
		blen = __gp->tmp_blen;					\
		F_SET(__gp, G_TMP_INUSE);				\
	} else								\
		BINC(sp, bp, blen, nlen);				\
}

#define	FREE_SPACE(sp, bp, blen) {					\
	if (bp == sp->gp->tmp_bp)					\
		F_CLR(sp->gp, G_TMP_INUSE);				\
	else								\
		FREE(bp, blen);						\
}

#ifdef DEBUG
#define	FREE(p, sz) {							\
	memset(p, 0xff, sz);						\
	free(p);							\
}
#else
#define	FREE(p, sz)	free(p);
#endif

/*
 * XXX
 * MIN/MAX have traditionally been in <sys/param.h>.  Don't
 * try to get them from there, it's just not worth the effort.
 */
#ifndef	MAX
#define	MAX(_a,_b)	((_a)<(_b)?(_b):(_a))
#endif
#ifndef	MIN
#define	MIN(_a,_b)	((_a)<(_b)?(_a):(_b))
#endif

/* Function prototypes that don't seem to belong anywhere else. */
u_long	 baud_from_bval __P((SCR *));
char	*charname __P((SCR *, ARG_CHAR_T));
void	 busy_off __P((SCR *));
void	 busy_on __P((SCR *, int, char const *));
int	 nonblank __P((SCR *, EXF *, recno_t, size_t *));
void	 set_alt_name __P((SCR *, char *));
int	 set_window_size __P((SCR *, u_int, int));
int	 status __P((SCR *, EXF *, recno_t, int));
char	*tail __P((char *));

#ifdef DEBUG
void	TRACE __P((SCR *, const char *, ...));
#endif

/* Digraphs (not currently real). */
int	digraph __P((SCR *, int, int));
int	digraph_init __P((SCR *));
void	digraph_save __P((SCR *, int));
