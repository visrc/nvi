/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 5.24 1993/03/28 19:04:44 bostic Exp $ (Berkeley) $Date: 1993/03/28 19:04:44 $
 */

/*
 * There are minimum values that vi has to have to display a screen.  These
 * are about the MINIMUM that you can have.  Changing these is not a good
 * idea.  In particular, while I intend to get the minimum rows down to 2
 * for the curses screen version of the vi screen, (1 for the line, 1 for
 * the error messages) changing the minimum columns is a lot trickier.  For
 * example, you have to have enough columns to display the line number, not
 * to mention the tabstop and shiftwidth values.  It's a lot simpler to have
 * a fixed value and not worry about it.
 */
#define	MINIMUM_SCREEN_ROWS	 4		/* XXX Should be 2. */
#define	MINIMUM_SCREEN_COLS	20

enum confirmation { YES, NO, QUIT };	/* Confirmation routine interface. */
					/* Line operations. */
enum operation { LINE_APPEND, LINE_DELETE, LINE_INSERT, LINE_RESET };
					/* Standard continue message. */
#define	CONTMSG	"Enter return to continue: "

/*
 * Structure for mapping lines to the screen.  SMAP is an array of
 * structures, one per screen line, holding a physical line and screen
 * offset into the line.  For example, the pair 2:1 would be the first
 * screen of line 2, and 2:2 would be the second.  If doing left-right
 * scrolling, all of the offsets will be the same, i.e. for the second
 * screen, 1:2, 2:2, 3:2, etc.  If doing the standard, but unbelievably
 * stupid, vi scrolling, it will be staggered, i.e. 1:1, 1:2, 1:3, 2:1,
 * 3:1, etc.
 *
 * It might be better to make SMAP a linked list of structures, instead
 * of an array.  This wins if scrolling the top or bottom of the screen,
 * because you can move the head and tail pointers instead of copying the
 * array.  This loses when you're deleting three lines out of the middle of
 * the map, though, because you have to increment through the structures
 * instead of doing a memmove.
 */

/* Map positions. */
enum position { P_TOP, P_FILL, P_MIDDLE, P_BOTTOM };

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
	struct _scr	*next, *prev;	/* Linked list of screens. */

	struct _gs	*gp;		/* Pointer to global area. */

	/* Underlying file information. */
	struct _exf	*enext;		/* Next file to edit. */
	struct _exf	*eprev;		/* Last file edited. */

	/* Vi curses screen information. */
	struct _smap	*h_smap;	/* Head of screen/line map. */
	struct _smap	*t_smap;	/* Tail screen/line map. */
	size_t	 scno;			/* 0-N: Logical screen cursor column. */
	size_t	 lines;			/* Physical number of screen lines. */
	size_t	 cols;			/* Physical number of screen cols. */

	/* Characters displayed. */
	u_char	*clen;			/* Character lengths. */
	char   **cname;			/* Character names. */

	/* Messages. */
	struct _msg	*msgp;		/* Linked list of messages. */
	char	*VB;			/* Visual bell termcap string. */

	/* Ex/vi: report of lines changed. */
	recno_t	 rptlines;		/* Count of lines modified. */
	char	*rptlabel;		/* How lines modified. */

	/* Vi: column suck. */
	size_t	 rcm;			/* 0-N: Column suck. */
#define	RCM_FNB		0x01		/* Column suck: first non-blank. */
#define	RCM_LAST	0x02		/* Column suck: last. */
	u_int	 rcmflags;

	/*
	 * Ex/vi: Input character handling.
	 *
	 * The routines that fill a buffer from the terminal share these
	 * three data structures.  They are a buffer to hold the return
	 * value, a quote buffer so we know which characters are quoted, and
	 * a widths buffer.  The last is used internally to hold the widths
	 * of each character.  Any new routines will need to support these
	 * too.
	 */
	char	*gb_cb;			/* Input character buffer. */
	char	*gb_qb;			/* Input character quote buffer. */
	u_char	*gb_wb;			/* Input character widths buffer. */
	size_t	 gb_len;		/* Input character buffer lengths. */
	int	 nkeybuf;		/* # of keys in the input buffer. */
	int	 nextkey;		/* Index of next key in keybuf. */
	u_char	*mapoutput;		/* Mapped key return. */
	u_char	 special[UCHAR_MAX];	/* Special characters. */
	u_char	 keybuf[256];		/* Key buffer. */

	/* Ex/vi: tags. */
	struct _tag	*thead;		/* Tag stack. */
	struct _tagf   **tfhead;	/* List of tag files. */
	char	*tlast;			/* Last tag. */

	/* Ex/vi: mapped characters, abbreviations. */
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

	/* Vi: increment command. */
	u_char	 inc_lastch;		/* Last increment character. */
	long	 inc_lastval;		/* Last increment value. */

	/* Ex/vi: bang command. */
	u_char	*lastbcomm;		/* Last bang command. */

	/* Ex/vi: search information. */
	enum direction searchdir;	/* File search direction. */
	enum cdirection csearchdir;	/* Character search direction. */
	regex_t	 sre;			/* Last search RE. */
	u_char	 lastckey;		/* Last search character. */

	/* Ex: output file pointer. */
	FILE	*stdfp;			/* Ex output file pointer. */

	/* Ex/vi: interface between ex/vi. */
	size_t	 exlinecount;		/* Ex/vi overwrite count. */
	size_t	 extotalcount;		/* Ex/vi overwrite count. */
	size_t	 exlcontinue;		/* Ex/vi line continue value. */

	/*
	 * Screen support routines.  This is the set of routines that
	 * have to be replaced to add a new screen to the editor.
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
	int	 (*gb) __P((struct _scr *, int, u_char **, size_t *, u_int));
	int	 (*position) __P((struct _scr *,
		     struct _exf *, recno_t *, u_long, enum position));
	int	 (*refresh) __P((struct _scr *, struct _exf *));
	size_t	 (*relative) __P((struct _scr *, struct _exf *, recno_t));
	recno_t	 (*textlines) __P((struct _scr *));
	int	 (*up) __P((struct _scr *,
		     struct _exf *, struct _mark *, recno_t, int));
	int	 (*vex) __P((struct _scr *, struct _exf *,
		     struct _mark *, struct _mark *, struct _mark *));

#define	S_EXIT		0x0000001	/* Exiting (forced). */
#define	S_EXIT_FORCE	0x0000002	/* Exiting (not forced). */
#define	S_MODE_EX	0x0000004	/* In ex mode. */
#define	S_MODE_VI	0x0000008	/* In vi mode. */
#define	S_SWITCH	0x0000010	/* Switch files (forced). */
#define	S_SWITCH_FORCE	0x0000020	/* Switch files (not forced). */
					/* File change mask. */
#define	S_FILE_CHANGED	(S_EXIT | S_EXIT_FORCE | S_SWITCH | S_SWITCH_FORCE)

#define	S_ABBREV	0x0000100	/* If have abbreviations. */
#define	S_AUTOPRINT	0x0000200	/* Autoprint flag. */
#define	S_BELLSCHED	0x0000400	/* Bell scheduled. */
#define	S_CHARDELETED	0x0000800	/* Character deleted. */
#define	S_CUR_INVALID	0x0001000	/* Cursor position is wrong. */
#define	S_IN_GLOBAL	0x0002000	/* Doing a global command. */
#define	S_INPUT		0x0004000	/* Doing text input. */
#define	S_MSGREENTER	0x0008000	/* If msg routine reentered. */
#define	S_RE_SET	0x0010000	/* The file's RE has been set. */
#define	S_REDRAW	0x0020000	/* Redraw the screen. */
#define	S_REFORMAT	0x0040000	/* Reformat the lines. */
#define	S_REFRESH	0x0080000	/* Refresh the screen. */
#define	S_RESIZE	0x0100000	/* Resize the screen. */
#define	S_SCHED_UPDATE	0x0200000	/* Erase modeline after keystroke. */
#define	S_SCREENWAIT	0x0400000	/* Hold messages for awhile. */
	u_int flags;
} SCR;

/* Public interfaces to the screens. */
int	sex_init __P((struct _scr *, struct _exf *));
int	svi_init __P((struct _scr *, struct _exf *));
