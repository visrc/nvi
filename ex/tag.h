/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: tag.h,v 8.6 1993/10/28 08:55:42 bostic Exp $ (Berkeley) $Date: 1993/10/28 08:55:42 $
 */

typedef struct _tagf {			/* Tag file. */
	struct queue_entry q;		/* Linked list of tag files. */
	char	*fname;			/* Tag file name. */

#define	TAGF_DNE	0x01		/* Didn't exist. */
#define	TAGF_DNE_WARN	0x02		/* DNE error reported. */
	u_char	 flags;
} TAGF;

typedef struct _tag {			/* Tag stack. */
	struct queue_entry q;		/* Linked list of tags. */
	FREF	*frp;			/* Saved file name. */
	recno_t	 lno;			/* Saved line number. */
	size_t	 cno;			/* Saved column number. */
} TAG;

int	ex_tagfirst __P((SCR *, char *));
