/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: cut.h,v 5.13 1993/03/25 14:59:00 bostic Exp $ (Berkeley) $Date: 1993/03/25 14:59:00 $
 */

typedef struct _text {			/* Text: a linked list of lines. */
	struct _text *next;		/* Next buffer. */
	u_char *lp;			/* Line buffer. */
	size_t len;			/* Line length. */
} TEXT;

typedef struct {			/* Cut buffer. */
	TEXT *head;
	u_long len;

#define	CB_LMODE	0x001		/* Line mode. */
	u_char flags;
} CB;
extern CB cuts[UCHAR_MAX + 2];		/* Set of cut buffers. */
		
typedef struct _ib {			/* Input buffer. */
	TEXT *head;			/* Linked list of input lines. */
	MARK start;			/* Starting cursor position. */
	MARK stop;			/* Ending cursor position. */
	u_char *ilb;			/* Input line buffer. */
	size_t len;			/* Input line length. */
	size_t ilblen;			/* Input buffer length. */
	u_char *rep;			/* Replay buffer. */
	size_t replen;			/* Replay buffer length. */
} IB;
extern IB ib;				/* Input buffer. */

#define	OOBCB	-1			/* Out-of-band cut buffer name. */
#define	DEFCB	UCHAR_MAX + 1		/* Default cut buffer. */
					/* CB to use. */
#define	VICB(vp)	((vp)->buffer == OOBCB ? DEFCB : (vp)->buffer)

/* Check a buffer name for validity. */
#define	CBNAME(sp, buf, cb) {						\
	if ((buf) > sizeof(cuts) - 1) {					\
		msgq(sp, M_ERROR, "Invalid cut buffer name.");	\
		return (1);						\
	}								\
	cb = &cuts[isupper(buf) ? tolower(buf) : buf];			\
}

/* Check to see if a buffer has contents. */
#define	CBEMPTY(sp, buf, cb) {						\
	if ((cb)->head == NULL) {					\
		if (buf == DEFCB)					\
			msgq(sp, M_ERROR,				\
			    "The default buffer is empty.");		\
		else							\
			msgq(sp, M_ERROR,				\
			    "Buffer %s is empty.", CHARNAME(sp, buf));	\
		return (1);						\
	}								\
}

/* Append a new TEXT structure into a CB or IB chain. */
#define	TEXTAPPEND(start, text) {					\
	register TEXT *__cblp;						\
	if ((__cblp = (start)->head) == NULL)				\
		(start)->head = (text);					\
	else {								\
		for (; __cblp->next; __cblp = __cblp->next);		\
		__cblp->next = (text);					\
	}								\
}
