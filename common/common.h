/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 5.26 1992/08/22 19:18:25 bostic Exp $ (Berkeley) $Date: 1992/08/22 19:18:25 $
 */

#include <db.h>				/* XXX for rptlines, below */

#define	ESCAPE	'\033'			/* Escape character. */
#define	TAB	8			/* XXX -- Settable? */

enum direction { FORWARD, BACKWARD };	/* Direction type. */

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
