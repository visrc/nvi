/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: tag.h,v 8.1 1993/06/09 22:26:41 bostic Exp $ (Berkeley) $Date: 1993/06/09 22:26:41 $
 */

typedef struct _tagf {				/* Tag file. */
	char *fname;				/* Tag file name. */
#define	TAGF_ERROR	0x01			/* Error reported. */
	u_char flags;
} TAGF;

typedef struct _tag {				/* Tag stack. */
	struct _tag *next, *prev;		/* Linked list of tags. */
	EXF *ep;				/* Saved file structure. */
	recno_t lno;				/* Saved line number. */
	size_t cno;				/* Saved column number. */
} TAG;

int	ex_tagfirst __P((SCR *, char *));
