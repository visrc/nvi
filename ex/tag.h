/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: tag.h,v 8.8 1993/11/18 08:17:48 bostic Exp $ (Berkeley) $Date: 1993/11/18 08:17:48 $
 */

struct _tagf {				/* Tag file. */
	TAILQ_ENTRY(_tagf) q;		/* Linked list of tag files. */
	char	*fname;			/* Tag file name. */

#define	TAGF_DNE	0x01		/* Didn't exist. */
#define	TAGF_DNE_WARN	0x02		/* DNE error reported. */
	u_char	 flags;
};

struct _tag {				/* Tag stack. */
	TAILQ_ENTRY(_tag) q;		/* Linked list of tags. */
	FREF	*frp;			/* Saved file name. */
	recno_t	 lno;			/* Saved line number. */
	size_t	 cno;			/* Saved column number. */
};

int	ex_tagalloc __P((SCR *, char *));
int	ex_tagcopy __P((SCR *, SCR *));
int	ex_tagfirst __P((SCR *, char *));
int	ex_tagfree __P((SCR *));
