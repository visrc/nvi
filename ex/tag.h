/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: tag.h,v 5.7 1993/05/04 15:59:22 bostic Exp $ (Berkeley) $Date: 1993/05/04 15:59:22 $
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
