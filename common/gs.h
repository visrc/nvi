/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: gs.h,v 10.7 1995/07/04 12:43:12 bostic Exp $ (Berkeley) $Date: 1995/07/04 12:43:12 $
 */

#define	TEMPORARY_FILE_STRING	"/tmp"	/* Default temporary file name. */

/*
 * File reference structure (FREF).  The structure contains the name of the
 * file, along with the information that follows the name.
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

/* Action argument to scr_exadjust(). */
typedef enum { EX_TERM_CE, EX_TERM_SCROLL } exadj_t;

/* Screen attribute argument to scr_attr(). */
typedef enum { SA_INVERSE } scr_attr_t;
 
/*
 * GS:
 *
 * Structure that describes global state of the running program.
 */
struct _gs {
	CIRCLEQ_HEAD(_dqh, _scr) dq;	/* Displayed screens. */
	CIRCLEQ_HEAD(_hqh, _scr) hq;	/* Hidden screens. */

					/* File references. */
	CIRCLEQ_HEAD(_frefh, _fref) frefq;

	char	*progname;		/* Programe name. */

	mode_t	 origmode;		/* Original terminal mode. */
	struct termios
		 original_termios;	/* Original terminal values. */

	void	*cl_private;		/* Curses support private area. */
	void	*xaw_private;		/* XAW support private area. */

	DB	*msg;			/* Message catalog DB. */
	MSGH	 msgq;			/* User message list. */
#define	DEFAULT_NOPRINT	'\1'		/* Emergency non-printable character. */
	CHAR_T	 noprint;		/* Cached, unprintable character. */

	char	*tmp_bp;		/* Temporary buffer. */
	size_t	 tmp_blen;		/* Temporary buffer size. */

	/*
	 * Ex command structures (EXCMD) and ex state.
	 *
	 * Defined here because ex commands and command state exist outside
	 * of any particular screen or file.  Also, vi commands use this to
	 * execute ex commands directly.  See the files ex/ex.c and vi/v_ex.c
	 * for details.
	 */
	LIST_HEAD(_excmdh, _excmd) ecq;	/* Ex command linked list. */
	EXCMD	 excmd;			/* Default ex command structure. */
	s_ex_t	 cm_state;		/* Ex current state. */
	s_ex_t	 cm_next;		/* Ex next state. */
	/*
	 * __TK__ load this into an excmd structure?
	 */
	char	*icommand;		/* Ex initial, command-line command. */

/*
 * XXX
 * Block signals if there are asynchronous events.  Used to keep DB system calls
 * from being interrupted and not restarted, as that will result in consistency
 * problems.  This should be handled by DB.
 */
#define	SIGBLOCK(gp)							\
	if (F_ISSET(gp, G_SIGBLOCK))					\
		(void)sigprocmask(SIG_BLOCK, &gp->blockset, NULL);
#define	SIGUNBLOCK(gp)							\
	if (F_ISSET(gp, G_SIGBLOCK))					\
		(void)sigprocmask(SIG_UNBLOCK, &gp->blockset, NULL);
	sigset_t blockset;		/* Signal mask. */

#ifdef DEBUG
	FILE	*tracefp;		/* Trace file pointer. */
#endif

	EVENT	*i_event;		/* Array of input events. */
	size_t	 i_nelem;		/* Number of array elements. */
	size_t	 i_cnt;			/* Count of events. */
	size_t	 i_next;		/* Offset of next event. */

#define	EC_MAPCOMMAND	0x01		/* Apply the command map. */
#define	EC_MAPINPUT	0x02		/* Apply the input map. */
#define	EC_MAPNODIGIT	0x04		/* Return to a digit. */
#define	EC_QUOTED	0x08		/* Try to quote next character */
	u_int8_t ec_flags;		/* Current event character mappings. */

	CB	*dcbp;			/* Default cut buffer pointer. */
	CB	 dcb_store;		/* Default cut buffer storage. */
	LIST_HEAD(_cuth, _cb) cutq;	/* Linked list of cut buffers. */

#define	MAX_BIT_SEQ	128		/* Max + 1 fast check character. */
	LIST_HEAD(_seqh, _seq) seqq;	/* Linked list of maps, abbrevs. */
	bitstr_t bit_decl(seqb, MAX_BIT_SEQ);

#define	MAX_FAST_KEY	254		/* Max fast check character.*/
#define	KEY_LEN(sp, ch)							\
	((ch) <= MAX_FAST_KEY ?						\
	    sp->gp->cname[ch].len : __v_key_len(sp, ch))
#define	KEY_NAME(sp, ch)						\
	((ch) <= MAX_FAST_KEY ?						\
	    sp->gp->cname[ch].name : __v_key_name(sp, ch))
	struct {
		CHAR_T	 name[MAX_CHARACTER_COLUMNS + 1];
		u_int8_t len;
	} cname[MAX_FAST_KEY + 1];	/* Fast lookup table. */

#define	KEY_VAL(sp, ch)							\
	((ch) <= MAX_FAST_KEY ? sp->gp->special_key[ch] :		\
	    (ch) > sp->gp->max_special ? 0 : __v_key_val(sp, ch))
	CHAR_T	 max_special;		/* Max special character. */
	u_char				/* Fast lookup table. */
	    special_key[MAX_FAST_KEY + 1];

/* Flags. */
#define	G_ABBREV	0x001		/* If have abbreviations. */
#define	G_BELLSCHED	0x002		/* Bell scheduled. */
#define	G_RECOVER_SET	0x004		/* Recover system initialized. */
#define	G_SETMODE	0x008		/* Tty mode changed. */
#define	G_SIGBLOCK	0x010		/* Need to block signals. */
#define	G_SNAPSHOT	0x020		/* Always snapshot files. */
#define	G_STDIN_TTY	0x040		/* Standard input is a tty. */
#define	G_TERMIOS_SET	0x080		/* Termios structure is valid. */
#define	G_TMP_INUSE	0x100		/* Temporary buffer in use. */
	u_int16_t flags;

	/* Screen interface functions. */
					/* Add a string to the screen. */
	int	(*scr_addstr) __P((SCR *, const char *, size_t));
					/* Toggle a screen attribute. */
	int	(*scr_attr) __P((SCR *, scr_attr_t, int));
					/* Beep/bell/flash the terminal. */
	int	(*scr_bell) __P((SCR *));
					/* Display a busy message. */
	int	(*scr_busy) __P((SCR *, char const *, int));
					/* Enter tty canonical mode. */
	int	(*scr_canon) __P((SCR *, int));
					/* Clear to the end of the line. */
	int	(*scr_clrtoeol) __P((SCR *));
					/* Return the cursor location. */
	int	(*scr_cursor) __P((SCR *, size_t *, size_t *));
					/* Delete a line. */
	int	(*scr_deleteln) __P((SCR *));
					/* Discard a screen. */
	int	(*scr_discard) __P((SCR *, SCR **));
					/* Ex: screen adjustment routine. */
	int	(*scr_ex_adjust) __P((SCR *, exadj_t));
	int	(*scr_fmap)		/* Set a function key. */
	    __P((SCR *, seq_t, CHAR_T *, size_t, CHAR_T *, size_t));
					/* Get a keyboard event. */
	int	(*scr_getkey) __P((SCR *, CHAR_T *));
					/* Insert a line. */
	int	(*scr_insertln) __P((SCR *));
					/* Return if interrupted. */
	int	(*scr_interrupt) __P((SCR *));
					/* Move the cursor. */
	int	(*scr_move) __P((SCR *, size_t, size_t));
					/* Message or ex output. */
	int	(*scr_msg) __P((SCR *, mtype_t, const char *, ...));
					/* Refresh the screen. */
	int	(*scr_refresh) __P((SCR *, int));
					/* Resize two screens. */
	int	(*scr_resize) __P((SCR *, long, long, SCR *, long, long));
					/* Split the screen. */
	int	(*scr_split) __P((SCR *, SCR *, int));
					/* Suspend the editor. */
	int	(*scr_suspend) __P((SCR *));
};

/* Init argument to v_init(). */
typedef enum { INIT_OK, INIT_DONE, INIT_ERR, INIT_USAGE } init_t;
