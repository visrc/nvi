/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 8.37 1993/10/27 13:35:53 bostic Exp $ (Berkeley) $Date: 1993/10/27 13:35:53 $
 */

/*
 * There are minimum values that vi has to have to display a screen.  The
 * row minimum is fixed at 1 line for the text, and 1 line for any error
 * messages.  The column calculation is a lot trickier.  For example, you
 * have to have enough columns to display the line number, not to mention
 * guaranteeing that tabstop and shiftwidth values are smaller than the
 * current column value.  It's a lot simpler to have a fixed value and not
 * worry about it.
 *
 * XXX
 * MINIMUM_SCREEN_COLS is probably wrong.
 */
#define	MINIMUM_SCREEN_ROWS	 2
#define	MINIMUM_SCREEN_COLS	20
					/* Line operations. */
enum operation { LINE_APPEND, LINE_DELETE, LINE_INSERT, LINE_RESET };
					/* Standard continue message. */
enum position { P_BOTTOM, P_FILL, P_MIDDLE, P_TOP };

/*
 * Structure for holding file references.  This structure contains the
 * "name" of a file, including the state of the name and if it's backed
 * by a temporary file.  Each SCR structure contains a linked list of
 * these (the user's argument list) as well as pointers for the current,
 * previous and next files.
 *
 * Note that the read-only bit follows the file name, not the file itself.
 *
 * XXX
 * The mtime field should be a struct timespec, but time_t is more portable.
 */
typedef struct _fref {
	struct queue_entry q;		/* Linked list of file references. */
	char	*tname;			/* Temporary file name. */
	char	*fname;			/* File name. */
	size_t	 nlen;			/* File name length. */
	recno_t	 lno;			/* 1-N: file cursor line. */
	size_t	 cno;			/* 0-N: file cursor column. */
	time_t	 mtime;			/* Last modification time. */

#define	FR_CURSORSET	0x001		/* If lno/cno valid. */
#define	FR_EDITED	0x002		/* If the file was ever edited. */
#define	FR_FREE_TNAME	0x004		/* Free the tname field. */
#define	FR_IGNORE	0x008		/* File isn't part of argument list. */
#define	FR_NAMECHANGED	0x010		/* File name was changed. */
#define	FR_NEWFILE	0x020		/* File doesn't really exist yet. */
#define	FR_NONAME	0x040		/* File has no name. */
#define	FR_RDONLY	0x080		/* File is read-only. */
#define	FR_UNLINK_TFILE	0x100		/* Unlink the temporary file. */
	u_int	 flags;
} FREF;

/*
 * scr --
 *	The screen structure.
 *
 * Most of the traditional ex/vi options and values follow the screen, and
 * are therefore kept here.  For those of you that didn't follow that sentence,
 * read "dumping ground".  For example, things like tags and mapped character
 * sequences are stored here.  Each new screen that is added to the editor will
 * almost certainly have to keep its own stuff in here as well.
 */
typedef struct _scr {
/* INITIALIZED AT SCREEN CREATE. */
	struct _scr	*next, *prev;	/* Linked list of screens. */

	struct _scr	*child;		/* split screen: child screen. */
	struct _scr	*parent;	/* split screen: parent screen. */
	struct _scr	*snext;		/* split screen: next display screen. */

	struct _exf	*ep;		/* Screen's current file. */

					/* File name oriented state. */
	struct queue_entry frefq;	/* Linked list of FREF structures. */
	FREF	*frp;			/* Current FREF. */
	FREF	*p_frp;			/* Previous FREF. */

	void	*svi_private;		/* Vi curses screen information. */
	void	*xaw_private;		/* Vi XAW screen information. */

	recno_t	 lno;			/* 1-N:     cursor file line. */
	recno_t	 olno;			/* 1-N: old cursor file line. */
	size_t	 cno;			/* 0-N:     file cursor column. */
	size_t	 ocno;			/* 0-N: old file cursor column. */
	size_t	 sc_row;		/* 0-N: logical screen cursor row. */
	size_t	 sc_col;		/* 0-N: logical screen cursor column. */

	size_t	 rows;			/* 1-N:      rows per screen. */
	size_t	 cols;			/* 1-N:   columns per screen. */
	size_t	 t_rows;		/* 1-N:     text rows per screen. */
	size_t	 t_maxrows;		/* 1-N: max text rows per screen. */
	size_t	 t_minrows;		/* 1-N: min text rows per screen. */
	size_t	 w_rows;		/* 1-N:      rows per window. */
	size_t	 s_off;			/* 0-N: row offset in window. */

	size_t	 rcm;			/* Vi: 0-N: Column suck. */
#define	RCM_FNB		0x01		/* Column suck: first non-blank. */
#define	RCM_LAST	0x02		/* Column suck: last. */
	u_int	 rcmflags;

#define	L_ADDED		0		/* Added lines. */
#define	L_CHANGED	1		/* Changed lines. */
#define	L_COPIED	2		/* Copied lines. */
#define	L_DELETED	3		/* Deleted lines. */
#define	L_JOINED	4		/* Joined lines. */
#define	L_MOVED		5		/* Moved lines. */
#define	L_PUT		6		/* Put lines. */
#define	L_LSHIFT	7		/* Left shift lines. */
#define	L_RSHIFT	8		/* Right shift lines. */
#define	L_YANKED	9		/* Yanked lines. */
	recno_t	 rptlines[L_YANKED + 1];/* Ex/vi: lines changed by last op. */

	struct _msg	*msgp;		/* User message list. */

	u_long	ccnt;			/* Command count. */
	u_long	q_ccnt;			/* Quit command count. */

	void	*args;			/* Ex argument buffers. */
	char   **argv;			/* Arguments. */
	char	*ex_argv[3];		/* Special purpose 2 slots. */
	int	 argscnt;		/* Argument count. */
	
					/* Ex/vi: interface between ex/vi. */
	FILE	*stdfp;			/* Ex output file pointer. */
	size_t	 exlinecount;		/* Ex/vi overwrite count. */
	size_t	 extotalcount;		/* Ex/vi overwrite count. */
	size_t	 exlcontinue;		/* Ex/vi line continue value. */

					/* FWOPEN_NOT_AVAILABLE */
	int	 trapped_fd;		/* Ex/vi trapped file descriptor. */

	char	 at_lbuf;		/* Last at buffer executed. */

	fd_set	 rdfd;			/* Ex/vi: read fd select mask. */

	HDR	 bhdr;			/* Ex/vi: line input. */
	HDR	 txthdr;		/* Vi: text input TEXT header. */

	char	*ibp;			/* Ex: line input buffer. */
	size_t	 ibp_len;		/* Line input buffer length. */

					/* Ex: last command. */
	struct _excmdlist const *lastcmd;

	int	 sh_in[2];		/* Vi: script pipe. */
	int	 sh_out[2];		/* Vi: script pipe. */
	pid_t	 sh_pid;		/* Vi: shell pid. */
	char	*sh_prompt;		/* Vi: script prompt. */
	size_t	 sh_prompt_len;		/* Vi: script prompt length. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	struct _gs	*gp;		/* Pointer to global area. */

	char	*rep;			/* Vi: input replay buffer. */
	size_t	 rep_len;		/* Vi: input replay buffer length. */

	char	*lastbcomm;		/* Ex/vi: last bang command. */

	char	*alt_fname;		/* Ex/vi: alternate file name. */

	u_char	 inc_lastch;		/* Vi: Last increment character. */
	long	 inc_lastval;		/*     Last increment value. */

	void	*sdot;			/* Vi: saved dot, motion command. */
	void	*sdotmotion;

	char	 rlast;			/* Vi: saved 'r' command character. */

	char	*paragraph;		/* Vi: paragraph search list. */

	struct queue_entry tagq;	/* Ex/vi: tag stack. */
	struct _tagf   **tfhead;	/* Ex/vi: list of tag files. */
	char	*tlast;			/* Ex/vi: saved last tag. */

					/* Ex/vi: search/substitute info. */
	enum direction	searchdir;	/* File search direction. */
	regex_t	 sre;			/* Last search RE. */
	enum cdirection	csearchdir;	/* Character search direction. */
	u_char	 lastckey;		/* Last search character. */
	regmatch_t     *match;		/* Substitute match array. */
	size_t	 matchsize;		/* Substitute match array size. */
	char	*repl;			/* Substitute replacement. */
	size_t	 repl_len;		/* Substitute replacement length.*/
	size_t	*newl;			/* Newline offset array. */
	size_t	 newl_len;		/* Newline array size. */
	size_t	 newl_cnt;		/* Newlines in replacement. */

	CHNAME	*cname;			/* Display names of characters. */
	u_char	 special[UCHAR_MAX];	/* Special character array. */

					/* Ex/vi: mapped chars, abbrevs. */
	HDR	 seqhdr;		/* Linked list of all sequences. */
	HDR	 seq[UCHAR_MAX];	/* Linked character sequences. */

	char const *time_msg;		/* ITIMER_REAL message. */
	struct itimerval time_value;	/* ITIMER_REAL saved value. */
	sig_ret_t time_handler;		/* ITIMER_REAL saved handler. */

	OPTION	 opts[O_OPTIONCOUNT];	/* Ex/vi: options. */

/*
 * SCREEN SUPPORT ROUTINES.
 * This is the set of routines that have to be written to add a screen.
 */
	void	 (*s_bell) __P((struct _scr *));
	int	 (*s_busy) __P((struct _scr *, char const *));
	int	 (*s_change) __P((struct _scr *,
		     struct _exf *, recno_t, int *, enum operation));
	size_t	 (*s_chposition) __P((struct _scr *,
		     struct _exf *, recno_t, size_t));
	enum confirm
		 (*s_confirm) __P((struct _scr *,
		     struct _exf *, struct _mark *, struct _mark *));
	int	 (*s_down) __P((struct _scr *,
		     struct _exf *, struct _mark *, recno_t, int));
	int	 (*s_ex_cmd) __P((struct _scr *, struct _exf *,
		     struct _excmdarg *, struct _mark *));
	int	 (*s_ex_run) __P((struct _scr *, struct _exf *,
		     struct _mark *));
	int	 (*s_ex_write) __P((void *, const char *, int));
	int	 (*s_fill) __P((struct _scr *,
		     struct _exf *, recno_t, enum position));
	enum input
		 (*s_get) __P((struct _scr *,
		     struct _exf *, HDR *, int, u_int));
	enum input
		 (*s_key_read) __P((struct _scr *, int *, int));
	int	 (*s_key_wait) __P((struct _scr *));
	int	 (*s_optchange) __P((struct _scr *, int));
	int	 (*s_position) __P((struct _scr *,
		     struct _exf *, MARK *, u_long, enum position));
	int	 (*s_refresh) __P((struct _scr *, struct _exf *));
	size_t	 (*s_relative) __P((struct _scr *, struct _exf *, recno_t));
	int	 (*s_split) __P((struct _scr *, char *[]));
	int	 (*s_suspend) __P((struct _scr *));
	int	 (*s_up) __P((struct _scr *,
		     struct _exf *, struct _mark *, recno_t, int));

/* Editor modes. */
#define	S_MODE_EX	0x0000001	/* Ex mode. */
#define	S_MODE_VI	0x0000002	/* Vi mode. */

/* Major screen/file changes. */
#define	S_EXIT		0x0000004	/* Exiting (not forced). */
#define	S_EXIT_FORCE	0x0000008	/* Exiting (forced). */
#define	S_FSWITCH	0x0000010	/* Switch files. */
#define	S_SSWITCH	0x0000020	/* Switch screens. */
#define	S_MAJOR_CHANGE			/* Screen or file changes. */	\
	(S_EXIT | S_EXIT_FORCE | S_FSWITCH | S_SSWITCH)

#define	S_ABBREV	0x0000040	/* If have abbreviations. */
#define	S_AUTOPRINT	0x0000080	/* Autoprint flag. */
#define	S_BELLSCHED	0x0000100	/* Bell scheduled. */
#define	S_GLOBAL	0x0000200	/* Doing a global command. */
#define	S_INPUT		0x0000400	/* Doing text input. */
#define	S_INTERRUPTED	0x0000800	/* If have been interrupted. */
#define	S_INTERRUPTIBLE	0x0001000	/* If can be interrupted. */
#define	S_MSGREENTER	0x0002000	/* If msg routine reentered. */
#define	S_REDRAW	0x0004000	/* Redraw the screen. */
#define	S_REFORMAT	0x0008000	/* Reformat the screen. */
#define	S_REFRESH	0x0010000	/* Refresh the screen. */
#define	S_RESIZE	0x0020000	/* Resize the screen. */
#define	S_RE_SET	0x0040000	/* The file's RE has been set. */
#define	S_SCRIPT	0x0080000	/* Window is a shell script. */
#define	S_TIMER_SET	0x0100000	/* If a busy timer is running. */
#define	S_TERMSIGNAL	0x0200000	/* Termination signal received. */
#define	S_UPDATE_MODE	0x0400000	/* Don't repaint modeline. */

#define	S_SCREEN_RETAIN			/* Retain at screen create. */	\
	(S_MODE_EX | S_MODE_VI | S_RE_SET)

	u_int flags;
} SCR;

/* Public interfaces to the screens. */
int	screen_end __P((struct _scr *));
int	screen_init __P((struct _scr *, struct _scr *));
int	sex __P((struct _scr *, struct _exf *, struct _scr **));
int	svi __P((struct _scr *, struct _exf *, struct _scr **));
int	xaw __P((struct _scr *, struct _exf *, struct _scr **));
