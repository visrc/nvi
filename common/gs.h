/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: gs.h,v 8.25 1993/12/22 16:13:58 bostic Exp $ (Berkeley) $Date: 1993/12/22 16:13:58 $
 */

struct _gs {
	CIRCLEQ_HEAD(_dqh, _scr) dq;	/* Displayed screens. */
	CIRCLEQ_HEAD(_hqh, _scr) hq;	/* Hidden screens. */
	
	mode_t	 origmode;		/* Original terminal mode. */
	struct termios
		 original_termios;	/* Original terminal values. */
	struct termios
		 s5_curses_botch;	/* System V curses workaround. */

	MSGH	 msgq;			/* User message list. */

	char	*tmp_bp;		/* Temporary buffer. */
	size_t	 tmp_blen;		/* Size of temporary buffer. */

#ifdef DEBUG
	FILE	*tracefp;		/* Trace file pointer. */
#endif

/* INFORMATION SHARED BY ALL SCREENS. */
	IBUF	*tty;			/* Key input buffer. */

	LIST_HEAD(_cuth, _cb) cutq;	/* Linked list of cut buffers. */

#define	MAX_BIT_SEQ	128		/* Max + 1 fast check character. */
	LIST_HEAD(_seqh, _seq) seqq;	/* Linked list of maps, abbrevs. */
	bitstr_t bit_decl(seqb, MAX_BIT_SEQ);

#define	term_key_val(sp, ch)						\
	((ch) <= MAX_FAST_KEY ? sp->gp->special_key[ch] :		\
	    (ch) > sp->gp->max_special ? 0 : __term_key_val(sp, ch))
#define	MAX_FAST_KEY	255		/* Max + 1 fast check character.*/
	CHAR_T	 max_special;		/* Max special character. */
	u_char	*special_key;		/* Fast lookup table. */
	CHNAME	const *cname;		/* Display names of ASCII characters. */

#define	G_ABBREV	0x00001		/* If have abbreviations. */
#define	G_BELLSCHED	0x00002		/* Bell scheduled. */
#define	G_CURSES_INIT	0x00004		/* Curses: initialized. */
#define	G_CURSES_S5CB	0x00008		/* Curses: s5_curses_botch set. */
#define	G_ISFROMTTY	0x00010		/* Reading from a tty. */
#define	G_RECOVER_SET	0x00020		/* Recover system initialized. */
#define	G_SETMODE	0x00040		/* Tty mode changed. */
#define	G_SIGALRM	0x00080		/* SIGALRM arrived. */
#define	G_SIGHUP	0x00100		/* SIGHUP arrived. */
#define	G_SIGTERM	0x00200		/* SIGTERM arrived. */
#define	G_SIGWINCH	0x00400		/* SIGWINCH arrived. */
#define	G_SLEEPING	0x00800		/* Asleep (die on signal). */
#define	G_SNAPSHOT	0x01000		/* Always snapshot files. */
#define	G_TMP_INUSE	0x02000		/* Temporary buffer in use. */
	u_int	 flags;
};

extern GS *__global_list;		/* List of screens. */
