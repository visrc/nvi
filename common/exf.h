/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: exf.h,v 5.40 1993/03/25 14:59:04 bostic Exp $ (Berkeley) $Date: 1993/03/25 14:59:04 $
 */

#ifndef _DATA_STRUCTURES_H_
#define	_DATA_STRUCTURES_H_

/*
 * There are three basic data structures in this editor.  The first is a
 * single global structure (GS) which contains information common to all
 * files and screens.  It's really pretty tiny, and functions more as a
 * single place to hang things than anything else.
 *
 * The second and third structures are the file structures (EXF) and the
 * screen structures (SCR).  They contain information theoretically unique
 * to a screen or file, respectively.  The GS structure contains linked
 * lists of EXF and SCR structures.  The structures can also be classed
 * by persistence.  The GS structure never goes away and the SCR structure
 * persists over instances of files.
 *
 * In general, we pass an SCR structure and an EXF structure around to
 * editor functions.  The SCR structure is necessary for any routine that
 * wishes to talk to the screen, the EXF structure is necessary for any
 * routine that wants to modify the file.  The relationship between an
 * SCR structure and its underlying EXF structure is not fixed, and there
 * is (deliberately) no way to translate from one to the other.
 *
 * The naming is consistent across the program.  (Macros even depend on it,
 * so don't change it!)  The global structure is "gp", the screen structure
 * is "sp", and the file structure is "ep".
 */

/* The HEADER structure is a special structure heading doubly linked lists. */
typedef struct {
	void *next, *prev;
} HEADER;

/* Initialize the doubly linked list header. */
#define	HEADER_INIT(hdr, n, p, cast) {					\
	(hdr).n = (hdr).p = (cast *)&(hdr);				\
}

/* Insert before node in a doubly linked list. */
#define INSERT_HEAD(ins, node, n, p, cast) {				\
        (ins)->n = (node)->n;						\
        (ins)->p = (cast *)(node);					\
        (node)->n->p = (ins);						\
        (node)->n = (ins);						\
}

/* Insert at the tail of a doubly linked list. */
#define INSERT_TAIL(ins, node, n, p, cast) {				\
	(node)->p->n = (ins);						\
	(ins)->p = (node)->p;						\
	(node)->p = (ins);						\
	(ins)->n = (cast *)(node);					\
}

/* Delete node from doubly linked list. */
#define	DELETE(node, n, p) {						\
	(node)->p->n = (node)->n;					\
	(node)->n->p = (node)->p;					\
}

/* Macros to set/clear/test flags. */
#define	F_SET(p, f)	(p)->flags |= (f)
#define	F_CLR(p, f)	(p)->flags &= ~(f)
#define	F_ISSET(p, f)	((p)->flags & (f))

struct _scr;
typedef struct _gs {
	HEADER	 exfhdr;		/* Linked list of EXF structures. */
	HEADER	 scrhdr;		/* Linked list of SCR structures. */

	struct _scr	*snext;		/* Next screen to display. */
	struct _scr	*sprev;		/* Last screen displayed. */

	mode_t	 origmode;		/* Original terminal mode. */
	struct termios
		 original_termios;	/* Original terminal values. */

	char	*tmp_bp;		/* Temporary buffer. */
	size_t	 tmp_blen;		/* Size of temporary buffer. */

#ifdef DEBUG
	FILE	*tracefp;		/* Trace file pointer. */
#endif

#define	G_SETMODE	0x01		/* Tty mode changed. */
#define	G_TMP_INUSE	0x02		/* Temporary buffer in use. */
	u_int	 flags;
} GS;

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
typedef struct smap {
	recno_t lno;			/* 1-N: File line number. */
	size_t off;			/* 1-N: Screen offset in the line. */
} SMAP;

/*
 * Map and abbreviation structures.
 *
 * The map structure is an UCHAR_MAX size array of SEQ pointers which are
 * NULL or valid depending if the offset key begins any map or abbreviation
 * sequences.  If the pointer is valid, it references a doubly linked list
 * of SEQ pointers, threaded through the snext and sprev pointers.  This is
 * based on the belief that most normal characters won't start sequences so
 * lookup will be fast on non-mapped characters.  Only a single pointer is
 * maintained to keep the overhead of the map array fairly small, so the first
 * element of the linked list has a NULL sprev pointer and the last element
 * of the list has a NULL snext pointer.  The structures in this list are
 * ordered by length, shortest to longest.  This is so that short matches 
 * will happen before long matches when the list is searched.
 *
 * In addition, each SEQ structure is on another doubly linked list of SEQ
 * pointers, threaded through the next and prev pointers.  This is a list
 * of all of the sequences.  This list is used by the routines that display
 * all of the sequences to the screen or write them to a file.
 */
					/* Sequence type. */
enum seqtype { SEQ_ABBREV, SEQ_COMMAND, SEQ_INPUT };

typedef struct _seq {
	struct _seq *next, *prev;	/* Linked list of all sequences. */
	struct _seq *snext, *sprev;	/* Linked list of ch sequences. */
	u_char *name;			/* Name of the sequence, if any. */
	u_char *input;			/* Input key sequence. */
	u_char *output;			/* Output key sequence. */
	int ilen;			/* Input key sequence length. */
	enum seqtype stype;		/* Sequence type. */

#define	S_USERDEF	0x01		/* If sequence user defined. */
	u_char flags;
} SEQ;

/*
 * scr --
 *	The screen structure.  Most of the traditional ex/vi options and
 *	values follow the screen, and therefore are kept here.  For those
 *	of you that didn't follow that sentence, read "dumping ground".
 *	For example, things like tags and mapped character sequences are
 *	stored here.
 */
struct _exf;
typedef struct _scr {
	struct _scr *next, *prev;	/* Linked list of screens. */

	GS	*gp;			/* Pointer to global area. */

	/* Underlying file information. */
	struct _exf	*enext;		/* Next file to edit. */
	struct _exf	*eprev;		/* Last file edited. */

	/* Screen information. */
	SMAP	*h_smap;		/* Head of screen/line map. */
	SMAP	*t_smap;		/* Tail screen/line map. */
	size_t	 scno;			/* 0-N: Logical screen cursor column. */
	size_t	 lines;			/* Physical number of screen lines. */
	size_t	 cols;			/* Physical number of screen cols. */

	/* Characters displayed. */
	u_char	*clen;			/* Character lengths. */
	char   **cname;			/* Character names. */

	/* Messages. */
	MSG	*msgp;			/* Linked list of messages. */
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
	TAG	*thead;			/* Tag stack. */
	TAGF   **tfhead;		/* List of tag files. */
	char	*tlast;			/* Last tag. */

	/* Ex/vi: mapped characters, abbreviations. */
	HEADER	 seqhdr;		/* Linked list of all sequences. */
	SEQ	*seq[UCHAR_MAX];	/* Linked character sequences. */

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

	/* Ex/vi: support routines. */
	enum confirmation		/* Confirm an action, yes or no. */
		 (*confirm) __P((struct _scr *, struct _exf *, MARK *, MARK *));
					/* End a screen session. */
	int	 (*end) __P((struct _scr *));

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
#define	S_MSGREENTER	0x0004000	/* If msg routine reentered. */
#define	S_MSGWAIT	0x0008000	/* Hold messages for awhile. */
#define	S_NEEDMERASE	0x0010000	/* Erase modeline after keystroke. */
#define	S_RE_SET	0x0020000	/* The file's RE has been set. */
#define	S_REDRAW	0x0040000	/* Redraw the screen. */
#define	S_REFORMAT	0x0080000	/* Reformat the lines. */
#define	S_REFRESH	0x0100000	/* Refresh the screen. */
#define	S_RESIZE	0x0200000	/* Resize the screen. */
	u_int flags;
} SCR;

/*
 * exf --
 *	The file structure.
 */
typedef struct _exf {
	struct _exf *next, *prev;	/* Linked list of files. */

	char	*name;			/* File name. */
	char	*tname;			/* Temporary file name. */
	size_t	 nlen;			/* File name length. */

	recno_t	 lno;			/* 1-N:     Cursor screen line. */
	recno_t	 olno;			/* 1-N: Old cursor screen line. */
	size_t	 cno;			/* 0-N:     Physical cursor column. */
	size_t	 ocno;			/* 0-N: Old physical cursor column. */

					/* Underlying database state. */
	DB	*db;			/* File db structure. */
	u_char	*c_lp;			/* Cached line. */
	size_t	 c_len;			/* Cached line length. */
	recno_t	 c_lno;			/* Cached line number. */
	recno_t	 c_nlines;		/* Cached lines in the file. */

	DB	*log;			/* Log db structure. */
	u_char	*l_lp;			/* Log buffer. */
	size_t	 l_len;			/* Log buffer length. */
	u_char	 l_ltype;		/* Log type. */
	recno_t	 l_srecno;		/* Saved record number. */
	enum udirection lundo;		/* Last undo direction. */

	MARK	 getc_m;		/* Getc mark. */
	u_char	*getc_bp;		/* Getc buffer. */
	size_t	 getc_blen;		/* Getc buffer length. */

	MARK	 marks[UCHAR_MAX + 1];	/* File marks. */

#define	F_IGNORE	0x0002		/* File to be ignored. */
#define	F_MODIFIED	0x0008		/* File has been modified. */
#define	F_NAMECHANGED	0x0010		/* File name was changed. */
#define	F_NEWSESSION	0x0020		/* If a new edit session. */
#define	F_NOLOG		0x0040		/* Logging turned off. */
#define	F_NONAME	0x0080		/* File has no name. */
#define	F_RDONLY	0x0100		/* File is read-only. */
#define	F_UNDO		0x0400		/* No change since last undo. */

#define	F_RETAINMASK	(F_IGNORE)	/* Retained over edit sessions. */
	u_int	 flags;
} EXF;

#define	AUTOWRITE(sp, ep) {						\
	if (F_ISSET(ep, F_MODIFIED) && ISSET(O_AUTOWRITE) &&		\
	    file_sync(sp, ep, 0))					\
		return (1);						\
}

#define	GETLINE_ERR(sp, lno) {						\
	msgq(sp, M_ERROR,						\
	    "Error: %s/%d: unable to retrieve line %u.",		\
	    tail(__FILE__), __LINE__, lno);				\
}

#define	MODIFY_CHECK(sp, ep, force) {					\
	if (F_ISSET(ep, F_MODIFIED))					\
		if (ISSET(O_AUTOWRITE)) {				\
			if (file_sync((sp), (ep), (force)))		\
				return (1);				\
		} else if (!(force)) {					\
			msgq(sp, M_ERROR,				\
	"Modified since last write; write or use ! to override.");	\
			return (1);					\
		}							\
}

#define	MODIFY_WARN(sp, ep) {						\
	if (F_ISSET(ep, F_MODIFIED) && ISSET(O_WARN))			\
		msgq(sp, M_ERROR, "Modified since last write.");	\
}
#endif /* !_DATA_STRUCTURES_H_ */
