/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: tag.h,v 5.1 1992/05/01 18:42:43 bostic Exp $ (Berkeley) $Date: 1992/05/01 18:42:43 $
 */

typedef struct tag {
	struct tag *prev;			/* Previous tag. */
	char *tag;				/* Tag name. */
	char *fname;				/* File name. */
	char *line;				/* Search string. */
} TAG;

TAG *tag_pop __P((char *));
TAG *tag_push __P((char *));
