/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: cut.h,v 5.5 1992/10/18 13:02:23 bostic Exp $ (Berkeley) $Date: 1992/10/18 13:02:23 $
 */

typedef struct text {			/* Text: a linked list of lines. */
	struct text *next;		/* Next buffer. */
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
		
typedef struct {			/* Input buffer. */
	TEXT *head;			/* Start of the input. */
	MARK start;			/* Starting cursor position. */
	MARK stop;			/* Ending cursor position. */
	u_long len;			/* Total length (unused, for macro). */
	size_t insert;			/* Characters to push. */
	u_char *ilb;			/* Input line buffer. */
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

/* Append a new TEXT structure into a CB or IB chain. */
#define	TEXTAPPEND(start, text) {					\
	register TEXT *__cblp;						\
	if ((__cblp = (start)->head) == NULL)				\
		(start)->head = (text);					\
	else {								\
		for (; __cblp->next; __cblp = __cblp->next);		\
		__cblp->next = (text);					\
	}								\
	(start)->len += (text)->len;					\
}

int	add __P((MARK *, u_char *, size_t));
int	change __P((MARK *, MARK *, u_char *, size_t));
int	cut __P((int, MARK *, MARK *, int));
void	freetext __P((TEXT *));
int	put __P((int, MARK *, MARK *, int));
