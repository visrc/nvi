/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: tag.h,v 5.2 1992/11/06 12:23:03 bostic Exp $ (Berkeley) $Date: 1992/11/06 12:23:03 $
 */

typedef struct tag {
	struct tag *prev;			/* Previous tag. */
	char *tag;				/* Tag name. */
	char *fname;				/* File name. */
	char *line;				/* Search string. */
} TAG;

TAG	*tag_head __P((void));
TAG	*tag_pop __P((void));
TAG	*tag_push __P((char *));
