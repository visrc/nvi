/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: gs.h,v 8.33 1994/05/03 21:45:47 bostic Exp $ (Berkeley) $Date: 1994/05/03 21:45:47 $
 */

struct _gs {
	CIRCLEQ_HEAD(_dqh, _scr) dq;	/* Displayed screens. */
	CIRCLEQ_HEAD(_hqh, _scr) hq;	/* Hidden screens. */

	mode_t	 origmode;		/* Original terminal mode. */
	struct termios
		 original_termios;	/* Original terminal values. */

	MSGH	 msgq;			/* User message list. */

	char	*tmp_bp;		/* Temporary buffer. */
	size_t	 tmp_blen;		/* Size of temporary buffer. */

#ifdef DEBUG
	FILE	*tracefp;		/* Trace file pointer. */
#endif

/* INFORMATION SHARED BY ALL SCREENS. */
	IBUF	*tty;			/* Key input buffer. */

	CB	*dcbp;			/* Default cut buffer pointer. */
	CB	*dcb_store;		/* Default cut buffer storage. */
	LIST_HEAD(_cuth, _cb) cutq;	/* Linked list of cut buffers. */

#define	MAX_BIT_SEQ	128		/* Max + 1 fast check character. */
	LIST_HEAD(_seqh, _seq) seqq;	/* Linked list of maps, abbrevs. */
	bitstr_t bit_decl(seqb, MAX_BIT_SEQ);

#define	MAX_FAST_KEY	254		/* Max fast check character.*/
#define	KEY_LEN(sp, ch)							\
	((ch) <= MAX_FAST_KEY ?						\
	    sp->gp->cname[ch].len : __key_len(sp, ch))
#define	KEY_NAME(sp, ch)						\
	((ch) <= MAX_FAST_KEY ?						\
	    sp->gp->cname[ch].name : __key_name(sp, ch))
	struct {
		CHAR_T	 name[MAX_CHARACTER_COLUMNS + 1];
		u_int8_t len;
	} cname[MAX_FAST_KEY + 1];	/* Fast lookup table. */

#define	KEY_VAL(sp, ch)							\
	((ch) <= MAX_FAST_KEY ? sp->gp->special_key[ch] :		\
	    (ch) > sp->gp->max_special ? 0 : __key_val(sp, ch))
	CHAR_T	 max_special;		/* Max special character. */
	u_char				/* Fast lookup table. */
	    special_key[MAX_FAST_KEY + 1];

/* Interrupt macros. */
#define	INTERRUPTED(sp)							\
	(F_ISSET((sp), S_INTERRUPTED) || F_ISSET((sp)->gp, G_SIGINTR))
#define	CLR_INTERRUPT(sp) {						\
	F_CLR((sp), S_INTERRUPTED);					\
	F_CLR((sp)->gp, G_SIGINTR);					\
}

#define	G_ABBREV	0x00001		/* If have abbreviations. */
#define	G_BELLSCHED	0x00002		/* Bell scheduled. */
#define	G_CURSES_INIT	0x00004		/* Curses: initialized. */
#define	G_RECOVER_SET	0x00008		/* Recover system initialized. */
#define	G_SETMODE	0x00010		/* Tty mode changed. */
#define	G_SIGALRM	0x00020		/* SIGALRM arrived. */
#define	G_SIGHUP	0x00040		/* SIGHUP arrived. */
#define	G_SIGINTR	0x00080		/* SIGINT arrived. */
#define	G_SIGTERM	0x00100		/* SIGTERM arrived. */
#define	G_SIGTSTP	0x00200		/* SIGTSTP arrived. */
#define	G_SIGWINCH	0x00400		/* SIGWINCH arrived. */
#define	G_SLEEPING	0x00800		/* Asleep (die on signal). */
#define	G_SNAPSHOT	0x01000		/* Always snapshot files. */
#define	G_STDIN_TTY	0x02000		/* Standard input is a tty. */
#define	G_TERMIOS_SET	0x04000		/* Termios structure is valid. */
#define	G_TMP_INUSE	0x08000		/* Temporary buffer in use. */

	u_int	 flags;
};

extern GS *__global_list;		/* List of screens. */
