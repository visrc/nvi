/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: cut.h,v 8.6 1993/11/18 08:16:59 bostic Exp $ (Berkeley) $Date: 1993/11/18 08:16:59 $
 */

/* Cut buffers. */
struct _cb {
	LIST_ENTRY(_cb) q;		/* Linked list of cut buffers. */
	CHAR_T	name;			/* Cut buffer name. */
	HDR	txthdr;			/* Linked list of TEXT structures. */
	size_t	len;			/* Total length of cut text. */

#define	CB_LMODE	0x01		/* Cut was in line mode. */
	u_char	 flags;
};
		
struct _text {				/* Text: a linked list of lines. */
	TEXT *next, *prev;		/* Linked list of text structures. */
	char	*lb;			/* Line buffer. */
	size_t	 lb_len;		/* Line buffer length. */
	size_t	 len;			/* Line length. */

	/* These fields are used by the vi text input routine. */
	recno_t	 lno;			/* 1-N: line number. */
	size_t	 ai;			/* 0-N: autoindent bytes. */
	size_t	 insert;		/* 0-N: bytes to insert (push). */
	size_t	 offset;		/* 0-N: initial, unerasable bytes. */
	size_t	 owrite;		/* 0-N: bytes to overwrite. */

	/* These fields are used by the ex text input routine. */
	u_char	*wd;			/* Width buffer. */
	size_t	 wd_len;		/* Width buffer length. */
};

#define	DEFCB	'1'			/* Buffer '1' is the default buffer. */

/*
 * Get named buffer 'name'.
 * Translate upper-case buffer names to lower-case buffer names.
 */
#define	CBNAME(sp, cbp, name) {						\
	name = isupper(name) ? tolower(name) : (name);			\
	for (cbp = sp->gp->cutq.lh_first;				\
	    cbp != NULL; cbp = cbp->q.le_next)				\
		if (cbp->name == name)					\
			break;						\
}

/* Get a cut buffer, and check to see if it's empty. */
#define	CBEMPTY(sp, cbp, name) {					\
	CBNAME(sp, cbp, name);						\
	if (cbp == NULL) {						\
		if ((name) == '1')					\
			msgq(sp, M_ERR,					\
			    "The default buffer is empty.");		\
		else							\
			msgq(sp, M_ERR,					\
			    "Buffer %s is empty.", charname(sp, name));	\
		return (1);						\
	}								\
}

int	 cut __P((SCR *, EXF *, ARG_CHAR_T, MARK *, MARK *, int));
int	 delete __P((SCR *, EXF *, MARK *, MARK *, int));
void	 hdr_text_free __P((HDR *));
int	 put __P((SCR *, EXF *, ARG_CHAR_T, MARK *, MARK *, int));
void	 text_free __P((TEXT *));
TEXT	*text_init __P((SCR *, char *, size_t, size_t));
