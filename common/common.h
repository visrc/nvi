/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 5.22 1992/05/07 12:45:55 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:45:55 $
 */

#ifndef TRUE					/* XXX -- Delete. */
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif


#define	TAB	8				/* XXX -- Settable? */
#define	REG	register			/* XXX -- Delete. */

#define	inword(ch)	(isalnum(ch) || (ch) == '_')

#define INFINITY	2000000001L	/* a very large integer */

typedef struct {
#define	OOBLC		0			/* Out-of-band line/column. */
	u_long lno;				/* Line number. */
	u_long cno;				/* Column number. */
} MARK;
extern MARK cursor;				/* Current position. */

#define NMARKS	29
extern MARK	mark[NMARKS];	/* marks a-z, plus mark ' and two temps */

/*------------------------------------------------------------------------*/
/* misc housekeeping variables & functions				  */

extern long	nlines;		/* number of lines in the file */
extern long	changes;	/* counts changes, to prohibit short-cuts */
extern int	doingdot;	/* boolean: are we doing the "." command? */
extern int	doingglobal;	/* boolean: are doing a ":g" command? */
extern int	force_lnmd;	/* boolean: force a command to work in line mode? */
extern long	rptlines;	/* number of lines affected by a command */
extern char	*rptlabel;	/* description of how lines were affected */
#define		ctrl(ch) ((ch)&037)

/*------------------------------------------------------------------------*/
/* macros that are used as control structures                             */

#ifdef NOTDEF
#define BeforeAfter(before, after) for((before),bavar=1;bavar;(after),bavar=0)
#define ChangeText	BeforeAfter(beforedo(FALSE),afterdo())
#else
#define BeforeAfter(before, after) for((before),bavar=1;bavar;(after),bavar=0)
#define ChangeText	BeforeAfter(bavar=1,bavar=0)
#endif

extern int	bavar;		/* used only in BeforeAfter macros */

/* These are used to minimize calls to fetchline() */
#ifdef NOTDEF
extern int	plen;	/* length of the line */
extern long	pline;	/* line number that len refers to */
extern long	pchgs;	/* "changes" level that len refers to */
extern char	*ptext;	/* text of previous line, if valid */
extern void	pfetch();
#endif
extern char	digraph();

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

extern MARK	*V_from;
extern int	V_linemd;
