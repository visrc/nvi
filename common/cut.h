/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: cut.h,v 5.15 1993/04/05 07:12:20 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:12:20 $
 */

typedef struct _text {			/* Text: a linked list of lines. */
	struct _text *next;		/* Next buffer. */
	char	*lp;			/* Line buffer. */
	size_t	 len;			/* Line length. */
} TEXT;

typedef struct _cb {			/* Cut buffer. */
	struct _text *head;
	u_long	 len;

#define	CB_LMODE	0x001		/* Line mode. */
	u_char	 flags;
} CB;
		
typedef struct _ib {			/* Input buffer. */
	struct _text *head;		/* Linked list of input lines. */
	struct _mark start;		/* Starting cursor position. */
	struct _mark stop;		/* Ending cursor position. */
	char	*ilb;			/* Input line buffer. */
	size_t	 len;			/* Input line length. */
	size_t	 ilblen;		/* Input buffer length. */
	char	*rep;			/* Replay buffer. */
	size_t	 replen;		/* Replay buffer length. */
} IB;

#define	OOBCB	-1			/* Out-of-band cut buffer name. */
#define	DEFCB	UCHAR_MAX + 1		/* Default cut buffer. */
					/* CB to use. */
#define	VICB(vp)	((vp)->buffer == OOBCB ? DEFCB : (vp)->buffer)

/* Check to see if a buffer has contents. */
#define	CBEMPTY(sp, buf, cb) {						\
	if ((cb)->head == NULL) {					\
		if (buf == DEFCB)					\
			msgq(sp, M_ERROR,				\
			    "The default buffer is empty.");		\
		else							\
			msgq(sp, M_ERROR,				\
			    "Buffer %s is empty.", charname(sp, buf));	\
		return (1);						\
	}								\
}

/* Check a buffer name for validity. */
#define	CBNAME(sp, buf, cb) {						\
	if ((buf) > sizeof(sp->cuts) - 1) {				\
		msgq(sp, M_ERROR, "Invalid cut buffer name.");		\
		return (1);						\
	}								\
	cb = &sp->cuts[isupper(buf) ? tolower(buf) : buf];		\
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

/* Cut routines. */
int	 cut __P((struct _scr *,
	    struct _exf *, int, struct _mark *, struct _mark *, int));
int	 delete __P((struct _scr *,
	    struct _exf *, struct _mark *, struct _mark *, int));
int	 put __P((struct _scr *,
	    struct _exf *, int, struct _mark *, struct _mark *, int));
TEXT	*text_copy __P((struct _scr *, TEXT *));
void	 text_free __P((TEXT *));
