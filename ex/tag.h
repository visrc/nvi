/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: tag.h,v 5.6 1993/03/26 13:39:29 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:39:29 $
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

/* Tag routines. */
struct _tag *
	tag_head __P((struct _scr *));
struct _tag *
	tag_pop __P((struct _scr *));
struct _tag *
	tag_push __P((struct _scr *, char *));
