/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 5.28 1993/04/13 16:18:31 bostic Exp $ (Berkeley) $Date: 1993/04/13 16:18:31 $
 */

/*
 * There are minimum values that vi has to have to display a screen.  These
 * are about the MINIMUM that are possible, and changing them is not a good
 * idea.  In particular, while I intend to get the minimum rows down to 2
 * for the curses screen version of the vi screen, (1 for the line, 1 for
 * the error messages) changing the minimum columns is a lot trickier.  For
 * example, you have to have enough columns to display the line number, not
 * to mention guaranteeing that the tabstop and shiftwidth values are smaller.
 * It's a lot simpler to have a fixed value and not worry about it.
 */
#define	MINIMUM_SCREEN_ROWS	 4		/* XXX Should be 2. */
#define	MINIMUM_SCREEN_COLS	20

enum confirmation { YES, NO, QUIT };	/* Confirmation routine interface. */
					/* Line operations. */
enum operation { LINE_APPEND, LINE_DELETE, LINE_INSERT, LINE_RESET };
					/* Standard continue message. */
#define	CONTMSG	"Enter return to continue: "

/*
 * Structure for building argc/argv vector of ex arguments.
 */
typedef struct _args {
	char	*bp;			/* Buffer. */
	size_t	 len;			/* Buffer length. */

#define	A_ALLOCATED	0x01		/* If allocated space. */
	u_char	 flags;
} ARGS;

/*
 * Structure for mapping lines to the screen.  An SMAP is an array of
 * structures, one per screen line, holding a physical line and screen
 * offset into the line.  For example, the pair 2:1 would be the first
 * screen of line 2, and 2:2 would be the second.  If doing left-right
 * scrolling, all of the offsets will be the same, i.e. for the second
 * screen, 1:2, 2:2, 3:2, etc.  If doing the standard, but unbelievably
 * stupid, vi scrolling, it will be staggered, i.e. 1:1, 1:2, 1:3, 2:1,
 * 3:1, etc.
 *
 * The SMAP is always as large as the physical screen, so that when split
 * screens close, there is room to add in the newly available lines.
 */
					/* Map positions. */
enum position { P_BOTTOM, P_FILL, P_MIDDLE, P_TOP };

typedef struct _smap {
	recno_t lno;			/* 1-N: Physical file line number. */
	size_t off;			/* 1-N: Screen offset in the line. */
} SMAP;

/*
 * scr --
 *	The screen structure.  Most of the traditional ex/vi options and
 *	values follow the screen, and therefore are kept here.  For those
 *	of you that didn't follow that sentence, read "dumping ground".
 *	For example, things like tags and mapped character sequences are
 *	stored here.  Each new screen that is added to the editor will
 *	almost certainly have to keep its own stuff in here as well.
 */
struct _exf;
typedef struct _scr {
/* INITIALIZED AT SCREEN CREATE. */
	struct _scr	*next, *prev;	/* Linked list of screens. */

					/* Underlying file information. */
	struct _exf	*ep;		/* Screen's current file. */
	struct _exf	*enext;		/* Next file to edit. */
	struct _exf	*eprev;		/* Last file edited. */

					/* Split screen information. */
	struct _scr	*child;		/* Child screen. */
	struct _scr	*parent;	/* Parent screen. */
	struct _scr	*snext;		/* Next screen to display. */

					/* Physical screen information. */
	struct _smap	*h_smap;	/* Head of screen/row map. */
	struct _smap	*t_smap;	/* Tail of screen/row map. */
	recno_t	 lno;			/* 1-N:     cursor file line. */
	recno_t	 olno;			/* 1-N: old cursor file line. */
	size_t	 cno;			/* 0-N:     file cursor column. */
	size_t	 ocno;			/* 0-N: old file cursor column. */
	size_t	 rows;			/* 1-N: number of rows per screen. */
	size_t	 cols;			/* 1-N: number of columns per screen. */
	size_t	 t_rows;		/* 1-N: text rows per screen. */
	size_t	 w_rows;		/* 1-N: number of rows per window. */
	size_t	 s_off;			/* 0-N: offset into window. */
	size_t	 sc_row;		/* 0-N: logical screen cursor row. */
	size_t	 sc_col;		/* 0-N: logical screen cursor column. */

	struct _msg	*msgp;		/* User message list. */

	recno_t	 rptlines;		/* Ex/vi: lines changed by last op. */
	char	*rptlabel;		/* How modified. */

	size_t	 rcm;			/* Vi: 0-N: Column suck. */
#define	RCM_FNB		0x01		/* Column suck: first non-blank. */
#define	RCM_LAST	0x02		/* Column suck: last. */
	u_int	 rcmflags;

	struct _args	*args;		/* Ex/vi: argument buffers. */
	int	 argscnt;		/* Argument count. */
	char   **argv;			/* Arguments. */
	glob_t	 g;			/* Glob array. */
	
					/* Ex/vi: interface between ex/vi. */
	FILE	*stdfp;			/* Ex output file pointer. */
	size_t	 exlinecount;		/* Ex/vi overwrite count. */
	size_t	 extotalcount;		/* Ex/vi overwrite count. */
	size_t	 exlcontinue;		/* Ex/vi line continue value. */
#ifdef FWOPEN_NOT_AVAILABLE
	int	 trapped_fd;		/* Ex/vi trapped file descriptor. */
#endif

	u_int	 nkeybuf;		/* # of keys in the input buffer. */
	char	*mappedkey;		/* Mapped key return. */
	u_int	 nextkey;		/* Index of next key in keybuf. */
	char	 keybuf[256];		/* Key buffer. */

	fd_set	 rdfd;			/* Ex/vi: read fd select mask. */

	struct _hdr	 bhdr;		/* Ex/vi: line input. */
	struct _hdr	 txthdr;	/* Vi: text input TEXT header. */

	char	*ibp;			/* Ex: line input buffer. */
	size_t	 ibp_len;		/* Line input buffer length. */

	struct _excmdlist *lastcmd;	/* Ex: last command. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	struct _gs	*gp;		/* Pointer to global area. */

	char	*rep;			/* Vi: input replay buffer. */
	size_t	 rep_len;		/* Vi: input replay buffer length. */

	char	*VB;			/* Visual bell termcap string. */

	char	*lastbcomm;		/* Ex/vi: last bang command. */

	u_char	 inc_lastch;		/* Vi: Last increment character. */
	long	 inc_lastval;		/* Vi: Last increment value. */

	struct _cb cuts[UCHAR_MAX + 2];	/* Ex/vi: cut buffers. */

	struct _tag	*thead;		/* Ex/vi: tag stack. */
	struct _tagf   **tfhead;	/* List of tag files. */
	char	*tlast;			/* Last tag. */

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

	struct _chname *cname;		/* Display names of characters. */
	u_char	 special[UCHAR_MAX];	/* Special character array. */

					/* Ex/vi: mapped chars, abbrevs. */
	struct _hdr	 seqhdr;	/* Linked list of all sequences. */
	struct _seq	*seq[UCHAR_MAX];/* Linked character sequences. */

					/* Ex/vi: executed buffers. */
	char	*atkey_buf;		/* At key buffer. */
	char	*atkey_cur;		/* At key current pointer. */
	int	 atkey_len;		/* Remaining keys in at buffer. */
					/* At key stack. */
	u_char	 atkey_stack[UCHAR_MAX + 1];
	int	 exat_recurse;		/* Ex at recursion count. */
	int	 exat_lbuf;		/* Ex at last buffer. */
					/* Ex at key stack. */
	u_char	 exat_stack[UCHAR_MAX + 1];

	OPTION	 opts[O_OPTIONCOUNT];	/* Ex/vi: options. */

/*
 * SCREEN SUPPORT ROUTINES.
 * This is the set of routines that have to be written to add a screen.
 */
	void	 (*bell) __P((struct _scr *));
	int	 (*change) __P((struct _scr *,
		     struct _exf *, recno_t, enum operation));
	enum confirmation
		 (*confirm) __P((struct _scr *,
		     struct _exf *, struct _mark *, struct _mark *));
	int	 (*down) __P((struct _scr *,
		     struct _exf *, struct _mark *, recno_t, int));
	int	 (*end) __P((struct _scr *));
	int	 (*exwrite) __P((void *, const char *, int));
	int	 (*fill) __P((struct _scr *,
		     struct _exf *, recno_t, enum position));
	int	 (*gb) __P((struct _scr *, struct _exf *, struct _hdr *,
		     int, u_int));
	int	 (*position) __P((struct _scr *,
		     struct _exf *, recno_t *, u_long, enum position));
	int	 (*refresh) __P((struct _scr *, struct _exf *));
	size_t	 (*relative) __P((struct _scr *, struct _exf *, recno_t));
	int	 (*split) __P((struct _scr *, struct _exf *));
	int	 (*up) __P((struct _scr *,
		     struct _exf *, struct _mark *, recno_t, int));
	int	 (*vex) __P((struct _scr *, struct _exf *,
		     struct _mark *, struct _mark *, struct _mark *));

/* FLAGS. */
#define	S_EXIT		0x0000001	/* Exiting (forced). */
#define	S_EXIT_FORCE	0x0000002	/* Exiting (not forced). */
#define	S_MODE_EX	0x0000004	/* In ex mode. */
#define	S_MODE_VI	0x0000008	/* In vi mode. */
#define	S_FSWITCH	0x0000010	/* Switch files (not forced). */
#define	S_FSWITCH_FORCE	0x0000020	/* Switch files (forced). */
#define	S_SSWITCH	0x0000040	/* Switch screens. */
#define	__S_SPARE	0x0000080	/* Unused. */
#define	S_FILE_CHANGED			/* File change mask. */ \
	(S_EXIT | S_EXIT_FORCE | S_FSWITCH | S_FSWITCH_FORCE | S_SSWITCH)

#define	S_ABBREV	0x0000100	/* If have abbreviations. */
#define	S_AUTOPRINT	0x0000200	/* Autoprint flag. */
#define	S_BELLSCHED	0x0000400	/* Bell scheduled. */
#define	S_CHARDELETED	0x0000800	/* Character deleted. */
#define	S_CUR_INVALID	0x0001000	/* Cursor position is wrong. */
#define	S_IN_GLOBAL	0x0002000	/* Doing a global command. */
#define	S_INPUT		0x0004000	/* Doing text input. */
#define	S_ISFROMTTY	0x0008000	/* Reading from a tty. */
#define	S_MSGREENTER	0x0010000	/* If msg routine reentered. */
#define	S_RE_SET	0x0020000	/* The file's RE has been set. */
#define	S_REDRAW	0x0040000	/* Redraw the screen. */
#define	S_REFORMAT	0x0080000	/* Reformat the lines. */
#define	S_REFRESH	0x0100000	/* Refresh the screen. */
#define	S_RESIZE	0x0200000	/* Resize the screen. */
#define	S_UPDATE_MODE	0x0400000	/* Don't repaint modeline. */
#define	S_UPDATE_SCREEN	0x0800000	/* Don't repaint screen. */

#define	S_SCREEN_RETAIN			/* Retain over screen create. */ \
	(S_MODE_EX | S_MODE_VI | S_ISFROMTTY)

	u_int flags;
} SCR;

/* Public interfaces to the screens. */
int	scr_end __P((struct _scr *));
int	scr_init __P((struct _scr *, struct _scr *));
int	sex_init __P((struct _scr *, struct _exf *));
int	svi_init __P((struct _scr *, struct _exf *));
