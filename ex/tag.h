/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: tag.h,v 5.5 1993/03/25 15:00:30 bostic Exp $ (Berkeley) $Date: 1993/03/25 15:00:30 $
 */

typedef struct _tagf {
	char *fname;				/* Tag file name. */
#define	TAGF_ERROR	0x01			/* Error reported. */
	u_char flags;
} TAGF;

typedef struct _tag {
	struct _tag *prev;			/* Previous tag. */
	char *tag;				/* Tag name. */
	char *fname;				/* File name. */
	char *line;				/* Search string. */
} TAG;
