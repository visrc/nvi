/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 5.21 1992/05/04 11:50:19 bostic Exp $ (Berkeley) $Date: 1992/05/04 11:50:19 $
 */

#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif

#define	TAB	8				/* XXX */
#define	BLKSIZE	2048
#define	REG	register
#define	UCHAR(c)	((u_char)(c))

#define	inword(ch)	(isalnum(ch) || (ch) == '_')

#define INFINITY	2000000001L	/* a very large integer */

#ifdef NOTDEF
/*------------------------------------------------------------------------*/
/* These describe how temporary files are divided into blocks             */

#define MAXBLKS	(BLKSIZE / sizeof(unsigned short))
typedef union
{
	char		c[BLKSIZE];	/* for text blocks */
	unsigned short	n[MAXBLKS];	/* for the header block */
}
BLK;

/*------------------------------------------------------------------------*/
/* These are used manipulate BLK buffers.                                 */

extern BLK	hdr;		/* buffer for the header block */
extern BLK	*blkget();	/* given index into hdr.c[], reads block */
extern BLK	*blkadd();	/* inserts a new block into hdr.c[] */
void blkflush();
void blkdirty();
#endif

typedef long MARK;

#define markline(x)	(long)((x) / BLKSIZE)
#define markidx(x)	(int)((x) & (BLKSIZE - 1))
#define MARK_UNSET	((MARK)0)
#define MARK_FIRST	((MARK)BLKSIZE)
#define MARK_LAST	((MARK)(nlines * BLKSIZE))
#define MARK_AT_LINE(x)	((MARK)((x) * BLKSIZE))

#define NMARKS	29
extern MARK	mark[NMARKS];	/* marks a-z, plus mark ' and two temps */
extern MARK	cursor;		/* mark where line is */

/*------------------------------------------------------------------------*/
/* These are used to keep track of the current & previous files.	  */

MARK paste();
/*------------------------------------------------------------------------*/
/* misc housekeeping variables & functions				  */

#ifdef NOTDEF
extern int	tmpfd;		/* fd used to access the tmp file */
extern int	tmpnum;		/* counter used to generate unique filenames */
extern long	lnum[MAXBLKS];	/* last line# of each block */
#endif
extern long	nlines;		/* number of lines in the file */
extern long	changes;	/* counts changes, to prohibit short-cuts */
#ifdef NOTDEF
extern BLK	tmpblk;		/* a block used to accumulate changes */
#endif
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
extern int	plen;	/* length of the line */
extern long	pline;	/* line number that len refers to */
extern long	pchgs;	/* "changes" level that len refers to */
extern char	*ptext;	/* text of previous line, if valid */
extern void	pfetch();
extern char	digraph();

/* This is used to build a MARK that corresponds to a specific point in the
 * line that was most recently pfetch'ed.
 */
#define buildmark(text)	(MARK)(BLKSIZE * pline + (int)((text) - ptext))

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

extern MARK	V_from;
extern int	V_linemd;
