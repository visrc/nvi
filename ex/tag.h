/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	$Id: tag.h,v 10.2 1996/03/06 19:53:00 bostic Exp $ (Berkeley) $Date: 1996/03/06 19:53:00 $
 */

struct _tagf {				/* Tag file. */
	TAILQ_ENTRY(_tagf) q;		/* Linked list of tag files. */
	char	*name;			/* Tag file name. */
	int	 errno;			/* Error. */

#define	TAGF_ERR	0x01		/* Error occurred. */
#define	TAGF_ERR_WARN	0x02		/* Error reported. */
	u_int8_t flags;
};

struct _tag {				/* Tag stack. */
	TAILQ_ENTRY(_tag) q;		/* Linked list of tags. */
	FREF	*frp;			/* Saved file name. */
	recno_t	 lno;			/* Saved line number. */
	size_t	 cno;			/* Saved column number. */
	char	*search;		/* Search string. */
	size_t	 slen;			/* Search string length. */
};
