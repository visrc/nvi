/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: tag.h,v 5.3 1993/02/16 20:10:42 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:10:42 $
 */

typedef struct tag {
	struct tag *prev;			/* Previous tag. */
	char *tag;				/* Tag name. */
	char *fname;				/* File name. */
	char *line;				/* Search string. */
} TAG;

TAG	*tag_head __P((void));
TAG	*tag_pop __P((EXF *));
TAG	*tag_push __P((EXF *, char *));
