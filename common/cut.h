/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: cut.h,v 8.17 1994/07/17 00:24:27 bostic Exp $ (Berkeley) $Date: 1994/07/17 00:24:27 $
 */

typedef struct _texth TEXTH;		/* TEXT list head structure. */
CIRCLEQ_HEAD(_texth, _text);

/* Cut buffers. */
struct _cb {
	LIST_ENTRY(_cb) q;		/* Linked list of cut buffers. */
	TEXTH	 textq;			/* Linked list of TEXT structures. */
	CHAR_T	 name;			/* Cut buffer name. */
	size_t	 len;			/* Total length of cut text. */

#define	CB_LMODE	0x01		/* Cut was in line mode. */
	u_int8_t flags;
};

/* Lines/blocks of text. */
struct _text {				/* Text: a linked list of lines. */
	CIRCLEQ_ENTRY(_text) q;		/* Linked list of text structures. */
	char	*lb;			/* Line buffer. */
	size_t	 lb_len;		/* Line buffer length. */
	size_t	 len;			/* Line length. */

	/* These fields are used by the vi text input routine. */
	recno_t	 lno;			/* 1-N: line number. */
	size_t	 ai;			/* 0-N: autoindent bytes. */
	size_t	 insert;		/* 0-N: bytes to insert (push). */
	size_t	 offset;		/* 0-N: initial, unerasable chars. */
	size_t	 owrite;		/* 0-N: chars to overwrite. */
	size_t	 R_erase;		/* 0-N: 'R' erase count. */
	size_t	 sv_cno;		/* 0-N: Saved line cursor. */
	size_t	 sv_len;		/* 0-N: Saved line length. */

	/* These fields are used by the ex text input routine. */
	u_char	*wd;			/* Width buffer. */
	size_t	 wd_len;		/* Width buffer length. */
};

/*
 * Get named buffer 'name'.
 * Translate upper-case buffer names to lower-case buffer names.
 */
#define	CBNAME(sp, cbp, nch) {						\
	CHAR_T __name;							\
	__name = isupper(nch) ? tolower(nch) : (nch);			\
	for (cbp = sp->gp->cutq.lh_first;				\
	    cbp != NULL; cbp = cbp->q.le_next)				\
		if (cbp->name == __name)				\
			break;						\
}

#define	CUT_DELETE	0x01		/* Delete (rotate numeric buffers). */
#define	CUT_LINEMODE	0x02		/* Cut in line mode. */
int	 cut __P((SCR *, EXF *, CHAR_T *, MARK *, MARK *, int));
int	 cut_line __P((SCR *, EXF *, recno_t, size_t, size_t, CB *));
int	 delete __P((SCR *, EXF *, MARK *, MARK *, int));
int	 put __P((SCR *, EXF *, CB *, CHAR_T *, MARK *, MARK *, int));

void	 text_free __P((TEXT *));
TEXT	*text_init __P((SCR *, const char *, size_t, size_t));
void	 text_lfree __P((TEXTH *));
