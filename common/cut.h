/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: cut.h,v 5.1 1992/05/22 10:03:43 bostic Exp $ (Berkeley) $Date: 1992/05/22 10:03:43 $
 */

typedef struct cbline {			/* Single line in a cut buffer. */
	struct cbline *next;		/* Next buffer. */
	char *lp;			/* Line buffer. */
	size_t len;			/* Line length. */
} CBLINE;

typedef struct {			/* Cut buffer. */
	CBLINE *head;
	u_long	len;

#define	CB_LMODE	0x001		/* Line mode. */
	u_char	flags;
} CB;
		
#define	OOBCB	-1			/* Out-of-band cut buffer name. */
#define	DEFCB	UCHAR_MAX + 1		/* Default cut buffer. */

extern CB cuts[UCHAR_MAX + 2];		/* Set of cut buffers. */

/* Check a buffer name for validity. */
#define	CBNAME(buffer, cb) {						\
	if ((buffer) > sizeof(cuts) - 1) {				\
		bell();							\
		msg("Invalid cut buffer name.");			\
		return (1);						\
	}								\
	cb = &cuts[isupper(buffer) ? tolower(buffer) : buffer];		\
}

/* Check to see if a buffer has contents. */
#define	CBEMPTY(buffer, cb) {						\
	if ((cb)->head == NULL) {					\
		if (buffer == DEFCB)					\
			msg("The default buffer is empty.");		\
		else							\
			msg("Buffer %s is empty.", charname(buffer));	\
		return (1);						\
	}								\
}

int	cut __P((int, MARK *, MARK *, int));
int	put __P((int, MARK *, MARK *, int));
