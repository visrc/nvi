/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: gs.h,v 9.9 1995/02/16 11:12:25 bostic Exp $ (Berkeley) $Date: 1995/02/16 11:12:25 $
 */

/*
 * Structure for holding file references.  The structure contains the name
 * of the file, along with the information that follows the name.
 *
 * !!!
 * The read-only bit follows the file name, not the file itself.
 */
struct _fref {
	CIRCLEQ_ENTRY(_fref) q;		/* Linked list of file references. */
	char	*name;			/* File name. */
	char	*tname;			/* Backing temporary file name. */

	recno_t	 lno;			/* 1-N: file cursor line. */
	size_t	 cno;			/* 0-N: file cursor column. */

#define	FR_CURSORSET	0x001		/* If lno/cno values valid. */
#define	FR_DONTDELETE	0x002		/* Don't delete the temporary file. */
#define	FR_EXNAMED	0x004		/* Read/write renamed the file. */
#define	FR_NAMECHANGE	0x008		/* If the name changed. */
#define	FR_NEWFILE	0x010		/* File doesn't really exist yet. */
#define	FR_RDONLY	0x020		/* File is read-only. */
#define	FR_RECOVER	0x040		/* File is being recovered. */
#define	FR_TMPEXIT	0x080		/* Modified temporary file, no exit. */
#define	FR_TMPFILE	0x100		/* If file has no name. */
#define	FR_UNLOCKED	0x200		/* File couldn't be locked. */
	u_int16_t flags;
};

#define	TEMPORARY_FILE_STRING	"/tmp"	/* Default temporary file name. */

struct _gs {
	CIRCLEQ_HEAD(_dqh, _scr) dq;	/* Displayed screens. */
	CIRCLEQ_HEAD(_hqh, _scr) hq;	/* Hidden screens. */

					/* File references. */
	CIRCLEQ_HEAD(_frefh, _fref) frefq;

	char	*progname;		/* Programe name. */

	mode_t	 origmode;		/* Original terminal mode. */
	struct termios
		 original_termios;	/* Original terminal values. */

#define	INDX_ALRM	0		/* Offsets in the array. */
#define	INDX_HUP	1
#define	INDX_INT	2
#define	INDX_TERM	3
#define	INDX_WINCH	4
#define	INDX_MAX	5
	struct sigaction
		 oact[INDX_MAX];	/* Original signal information. */

	void	*cl_private;		/* Curses support private area. */
	void	*xaw_private;		/* XAW support private area. */

	DB	*msg;			/* Messages DB. */
	MSGH	 msgq;			/* User message list. */
	CHAR_T	 noprint;		/* Cached, unprintable character. */

	char	*tmp_bp;		/* Temporary buffer. */
	size_t	 tmp_blen;		/* Temporary buffer size. */

	char	*icommand;		/* Initial command. */

	char	*postcmd;		/* Ex posted command. */
	size_t	 postcmd_len;		/* Ex posted command length. */

	sigset_t blockset;		/* Signal mask. */

#ifdef DEBUG
	FILE	*tracefp;		/* Trace file pointer. */
#endif

	CHAR_T	 *i_ch;			/* Array of input characters. */
	u_int8_t *i_chf;		/* Array of character flags (CH_*). */
	size_t	  i_cnt;		/* Count of characters. */
	size_t	  i_nelem;		/* Number of array elements. */
	size_t    i_next;		/* Offset of next array entry. */

	CB	*dcbp;			/* Default cut buffer pointer. */
	CB	 dcb_store;		/* Default cut buffer storage. */
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

/* Flags. */
#define	G_ABBREV	0x0001		/* If have abbreviations. */
#define	G_BELLSCHED	0x0002		/* Bell scheduled. */
#define	G_RECOVER_SET	0x0004		/* Recover system initialized. */
#define	G_SETMODE	0x0008		/* Tty mode changed. */
#define	G_SIGALRM	0x0010		/* SIGALRM arrived. */
#define	G_SIGINT	0x0020		/* SIGINT arrived. */
#define	G_SIGWINCH	0x0040		/* SIGWINCH arrived. */
#define	G_SNAPSHOT	0x0080		/* Always snapshot files. */
#define	G_STDIN_TTY	0x0100		/* Standard input is a tty. */
#define	G_TERMIOS_SET	0x0200		/* Termios structure is valid. */
#define	G_TMP_INUSE	0x0400		/* Temporary buffer in use. */
	u_int16_t flags;
};

extern GS *__global_list;		/* List of screens. */

/*
 * Signals/timers have no structure or include files, so it's all here.
 *
 * Block all signals that are being handled.  Used to keep the underlying DB
 * system calls from being interrupted and not restarted, as it could cause
 * consistency problems.  Also used when vi forks child processes, to avoid
 * a signal arriving after the fork and before the exec, causing both parent
 * and child to attempt recovery processing.
 */
#define	INTERRUPTED(sp)							\
	(F_ISSET((sp), S_INTERRUPTED) || F_ISSET((sp)->gp, G_SIGINT))
#define	CLR_INTERRUPT(sp) {						\
	F_CLR((sp), S_INTERRUPTED);					\
	F_CLR((sp)->gp, G_SIGINT);					\
}
#define	SIGBLOCK(gp)							\
	(void)sigprocmask(SIG_BLOCK, &(gp)->blockset, NULL);
#define	SIGUNBLOCK(gp)							\
	(void)sigprocmask(SIG_UNBLOCK, &(gp)->blockset, NULL);

void	 busy_off __P((SCR *));
int	 busy_on __P((SCR *, char const *));
void	 sig_end __P((GS *));
int	 sig_init __P((SCR *));
void	 sig_restore __P((SCR *));
