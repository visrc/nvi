/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 9.19 1995/02/02 15:34:16 bostic Exp $ (Berkeley) $Date: 1995/02/02 15:34:16 $
 */

/*
 * There are minimum values that vi has to have to display a screen.  The row
 * minimum is fixed at 1 (the svi code can share a line between the text line
 * and the colon command/message line).  Column calculation is a lot trickier.
 * For example, you have to have enough columns to display the line number,
 * not to mention guaranteeing that tabstop and shiftwidth values are smaller
 * than the current column value.  It's simpler to have a fixed value and not
 * worry about it.
 *
 * XXX
 * MINIMUM_SCREEN_COLS is almost certainly wrong.
 */
#define	MINIMUM_SCREEN_ROWS	 1
#define	MINIMUM_SCREEN_COLS	20

typedef enum {				/* Line operations. */
    LINE_APPEND, LINE_DELETE, LINE_INSERT, LINE_RESET } lnop_t;

/*
 * Structure for holding file references.  Each SCR structure contains a
 * linked list of these.  The structure contains the name of the file,
 * along with the information that follows the name.
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
#define	FR_FNONBLANK	0x008		/* Move to the first non-<blank>. */
#define	FR_NAMECHANGE	0x010		/* If the name changed. */
#define	FR_NEWFILE	0x020		/* File doesn't really exist yet. */
#define	FR_RDONLY	0x040		/* File is read-only. */
#define	FR_RECOVER	0x080		/* File is being recovered. */
#define	FR_TMPEXIT	0x100		/* Modified temporary file, no exit. */
#define	FR_TMPFILE	0x200		/* If file has no name. */
#define	FR_UNLOCKED	0x400		/* File couldn't be locked. */
	u_int16_t flags;
};

#define	TEMPORARY_FILE_STRING	"/tmp"	/* Default temporary file name. */

/*
 * SCR --
 *	The screen structure.  To the extent possible, all screen information
 *	is stored in the various private areas.  The only information here
 *	is used by global routines or is shared by too many screens.
 */
struct _scr {
/* INITIALIZED AT SCREEN CREATE. */
	CIRCLEQ_ENTRY(_scr) q;		/* Screens. */

	int	 refcnt;		/* Reference count. */

	GS	*gp;			/* Pointer to global area. */

	SCR	*nextdisp;		/* Next display screen. */

	EXF	*ep;			/* Screen's current EXF structure. */

	MSGH	 msgq;			/* Message list. */
					/* FREF list. */
	CIRCLEQ_HEAD(_frefh, _fref) frefq;
	FREF	*frp;			/* FREF being edited. */

	char	**argv;			/* NULL terminated file name array. */
	char	**cargv;		/* Current file name. */

	u_long	 ccnt;			/* Command count. */
	u_long	 q_ccnt;		/* Quit or ZZ command count. */

					/* Screen's: */
	size_t	 rows;			/* 1-N: number of rows. */
	size_t	 cols;			/* 1-N: number of columns. */
	size_t	 woff;			/* 0-N: row offset in screen. */
	size_t	 t_rows;		/* 1-N: cur number of text rows. */
	size_t	 t_maxrows;		/* 1-N: max number of text rows. */
	size_t	 t_minrows;		/* 1-N: min number of text rows. */

					/* Cursor's: */
	recno_t	 lno;			/* 1-N: file line. */
	size_t	 cno;			/* 0-N: file character in line. */

	size_t	 rcm;			/* Vi: 0-N: Most attractive column. */

#define	L_ADDED		0		/* Added lines. */
#define	L_CHANGED	1		/* Changed lines. */
#define	L_DELETED	2		/* Deleted lines. */
#define	L_JOINED	3		/* Joined lines. */
#define	L_MOVED		4		/* Moved lines. */
#define	L_SHIFT		5		/* Shift lines. */
#define	L_YANKED	6		/* Yanked lines. */
	recno_t	 rptlchange;		/* Ex/vi: last L_CHANGED lno. */
	recno_t	 rptlines[L_YANKED + 1];/* Ex/vi: lines changed by last op. */

	FILE	*stdfp;			/* Ex output file pointer. */
	FILE	*sv_stdfp;		/* Saved copy. */

	char	*if_name;		/* Ex input file name, for messages. */
	recno_t	 if_lno;		/* Ex input file line, for messages. */

	fd_set	 rdfd;			/* Ex/vi: read fd select mask. */

	TEXTH	 __tiq;			/* Ex/vi: text input queue. */
	TEXTH	 *tiqp;			/* Ex/vi: text input queue reference. */

	SCRIPT	*script;		/* Vi: script mode information .*/

	recno_t	 defscroll;		/* Vi: ^D, ^U scroll information. */

	struct timeval	 busy_tod;	/* ITIMER_REAL: busy time-of-day. */
	char const	*busy_msg;	/* ITIMER_REAL: busy message. */

					/* Display character. */
	CHAR_T	 cname[MAX_CHARACTER_COLUMNS + 1];
	size_t	 clen;			/* Length of display character. */

#define	MAX_MODE_NAME	12
	char	*showmode;		/* Mode. */

	void	*ex_private;		/* Ex private area. */
	void	*sex_private;		/* Ex screen private area. */
	void	*vi_private;		/* Vi private area. */
	void	*svi_private;		/* Vi screen private area. */
	void	*cl_private;		/* Curses support private area. */
	void	*xaw_private;		/* XAW support private area. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	char	*alt_name;		/* Ex/vi: alternate file name. */

					/* Ex/vi: search/substitute info. */
	enum direction	searchdir;	/* File search direction. */
	regex_t	 subre;			/* Last subst. RE (compiled form). */
	regex_t	 sre;			/* Last search RE (compiled form). */
	char	*re;			/* Last search RE (uncompiled form). */
	size_t	 re_len;		/* Last search RE length. */
	char	*repl;			/* Substitute replacement. */
	size_t	 repl_len;		/* Substitute replacement length.*/
	size_t	*newl;			/* Newline offset array. */
	size_t	 newl_len;		/* Newline array size. */
	size_t	 newl_cnt;		/* Newlines in replacement. */
	u_int8_t c_suffix;		/* Edcompatible 'c' suffix value. */
	u_int8_t g_suffix;		/* Edcompatible 'g' suffix value. */

	u_int32_t
		 saved_vi_mode;		/* Saved S_VI_* flags. */

	OPTION	 opts[O_OPTIONCOUNT];	/* Options. */

/*
 * The following are editor routines that are called by "generic" functions
 * in ex or in the common code, and which are handled differently by ex and
 * vi, or by different vi screen types.  All the routines except e_ssize are
 * necessary because ex and vi handle the problem differently.  E_ssize is
 * here because we have to know (and in the curses case set the environment)
 * the screen size before we initialize the screen functions.  (SCR *) MUST
 * be the first argument to these routines.
 */
#define	SCR_ROUTINES_N	9
					/* Beep/bell/flash the terminal. */
	int	(*e_bell) __P((SCR *));
					/* Display a busy message. */
	int	(*e_busy) __P((SCR *, char const *));
					/* File line changed callback. */
	int	(*e_change) __P((SCR *, recno_t, lnop_t));
					/* Clear to the end of the screen. */
	int	(*e_clrtoeos) __P((SCR *));
					/* Confirm an action with the user. */
	conf_t	(*e_confirm) __P((SCR *, MARK *, MARK *));
	int     (*e_fmap)		/* Set a function key. */
	    __P((SCR *, enum seqtype, CHAR_T *, size_t, CHAR_T *, size_t));
					/* Refresh the screen. */
	int	(*e_refresh) __P((SCR *));
					/* Set the screen size. */
	int	(*e_ssize) __P((SCR *, int));
					/* Suspend the editor. */
	int	(*e_suspend) __P((SCR *));

					/* Saved copies. */
	void   (*sv_func[SCR_ROUTINES_N])();

/*
 * Screen flags.
 *
 * Editor screens.
 */
#define	S_EX		0x0000001	/* Ex screen. */
#define	S_VI_CURSES	0x0000002	/* Vi: curses screen. */
#define	S_VI_XAW	0x0000004	/* Vi: Athena widgets screen. */
#define	S_VI		(S_VI_CURSES | S_VI_XAW)
#define	S_SCREENS	(S_EX | S_VI)

/*
 * Public screen formatting flags.  Each flag implies the flags that
 * follow it.
 *
 * S_SCR_RESIZE
 *	The screen size has changed.  Requires that the current screen
 *	real estate be reallocated, and...
 * S_SCR_REFORMAT
 *	The expected presentation of the lines on the screen have changed,
 *	requiring that the intended screen lines be recalculated, and ...
 * S_SCR_REDRAW
 *	The screen doesn't correctly represent the file; repaint it.  Note,
 *	setting S_SCR_REDRAW in the current window causes *all* windows to
 *	be repainted.
 *
 * There are five additional flags:
 * S_SCR_CENTER
 *	If the current line isn't already on the screen, center it.
 * S_SCR_EXWROTE
 *	The ex part of the editor wrote to the screen.
 * S_SCR_REFRESH
 *	The screen has unknown contents.
 * S_SCR_TOP
 *	If the current line isn't already on the screen, put it at the top.
 * S_SCR_UMODE
 *	Don't update the mode line until the user enters a character.
 */
#define	S_SCR_RESIZE	0x0000008	/* Resize (reformat, refresh). */
#define	S_SCR_REFORMAT	0x0000010	/* Reformat (refresh). */
#define	S_SCR_REDRAW	0x0000020	/* Refresh. */

#define	S_SCR_CENTER	0x0000040	/* Center the line if not visible. */
#define	S_SCR_EXWROTE	0x0000080	/* Ex wrote the screen. */
#define	S_SCR_REFRESH	0x0000100	/* Repaint. */
#define	S_SCR_TOP	0x0000200	/* Top the line if not visible. */
#define	S_SCR_UMODE	0x0000400	/* Don't repaint modeline. */

/* Screen/file changes that require exit/reenter of the editor. */
#define	S_EXIT		0x0000800	/* Exiting (not forced). */
#define	S_EXIT_FORCE	0x0001000	/* Exiting (forced). */
#define	S_SSWITCH	0x0002000	/* Switch screens. */
#define	S_MAJOR_CHANGE	(S_EXIT | S_EXIT_FORCE | S_SSWITCH | S_SCR_RESIZE)

#define	S_ARGNOFREE	0x0004000	/* Argument list wasn't allocated. */
#define	S_ARGRECOVER	0x0008000	/* Argument list is recovery files. */
#define	S_BELLSCHED	0x0010000	/* Bell scheduled. */
#define	S_CONTINUE	0x0020000	/* Need to ask the user to continue. */
#define	S_EXSILENT	0x0040000	/* Ex batch script. */
#define	S_GLOBAL	0x0080000	/* Ex global: in the command. */
#define	S_GLOBAL_ABORT	0x0100000	/* Ex global: file/screen changed. */
#define	S_INPUT		0x0200000	/* Doing text input. */
#define	S_INTERRUPTED	0x0400000	/* If have been interrupted. */
#define	S_RE_RECOMPILE	0x0800000	/* The search RE needs recompiling. */
#define	S_RE_SEARCH	0x1000000	/* The search RE has been set. */
#define	S_RE_SUBST	0x2000000	/* The substitute RE has been set. */
#define	S_SCRIPT	0x4000000	/* Window is a shell script. */
	u_int32_t flags;
};

/* Generic routines to create/end a screen. */
int	screen_end __P((SCR *));
int	screen_init __P((SCR *, SCR **));
void	screen_fcopy __P((SCR *, int));
