/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: vi.h,v 8.25 1994/03/04 10:05:37 bostic Exp $ (Berkeley) $Date: 1994/03/04 10:05:37 $
 */

typedef struct _vikeys VIKEYS;

/* Structure passed around to functions implementing vi commands. */
typedef struct _vicmdarg {
#define	vp_startzero	buffer		/* START ZERO OUT. */
	CHAR_T	key;			/* Command key. */
	CHAR_T	buffer;			/* Buffer. */
	CHAR_T	character;		/* Character. */
	u_long	count;			/* Count. */
	u_long	count2;			/* Second count (only used by z). */
	VIKEYS const *kp;		/* VIKEYS key. */
	size_t	klen;			/* Keyword length. */

	/*
	 * Historic vi allowed "dl" when the cursor was on the last column,
	 * deleting the last character, and similarly allowed "dw" when
	 * the cursor was on the last column of the file.  It didn't allow
	 * "dh" when the cursor was on column 1, although these cases are
	 * not strictly analogous.  The point is that some movements would
	 * succeed if they were associated with a motion command, and fail
	 * otherwise.  This is part of the off-by-1 schizophrenia that
	 * plagued vi.  Other examples are that "dfb" deleted everything
	 * up to and including the next 'b' character, while "d/b" deleted
	 * everything up to the next 'b' character.  While this implementation
	 * regularizes the interface to the extent possible, there are many
	 * special cases that can't be fixed.  The special cases are handled
	 * by setting flags per command so that the underlying motion routines
	 * know what's really going on.
	 *
	 * Note: the VC_* flags are also set in the vikeys array, therefore
	 * they must have values not used by the VIKEYS structure flags.
	 */
#define	VC_C		0x0000001	/* The 'c' command. */
#define	VC_D		0x0000002	/* The 'd' command. */
#define	VC_S		0x0000004	/* The '>' command. */
#define	VC_Y		0x0000008	/* The 'y' command. */
#define	VC_COMMASK	0x000000f	/* Mask for special flags. */
#define	ISMOTION(vp)	F_ISSET(vp, VC_COMMASK)

	/*
	 * In addition, there are flags which may be set either in the command
	 * structure or by the motion component of the command.
	 *
	 * Note: the VM_* flags are also set in the vikeys array, therefore
	 * they must have values not used by the VIKEYS structure flags.
	 *
	 * Note: the VM_RCM_* flags are single usage, i.e. if you set one,
	 * you have to clear the others.
	 */
#define	VM_LMODE	0x0000010	/* Motion is line oriented. */
#define	VM_RCM		0x0000020	/* Use relative cursor movment (RCM). */
#define	VM_RCM_SET	0x0000040	/* RCM: set to current position. */
#define	VM_RCM_SETFNB	0x0000080	/* RCM: set to first non-blank (FNB). */
#define	VM_RCM_SETLAST	0x0000100	/* RCM: set to last character. */
#define	VM_RCM_SETLFNB	0x0000200	/* RCM: set to FNB if cursor moved. */
#define	VM_RCM_SETNNB	0x0000400	/* RCM: set to next non-blank. */
#define	VM_RCM_MASK	0x00007e0	/* Mask for RCM flags. */

	/* Flags for the underlying command. */
#define	VC_BUFFER	0x0000800	/* Buffer set. */
#define	VC_C1SET	0x0001000	/* Count 1 set. */
#define	VC_C1RESET	0x0002000	/* Reset C1SET flag for dot commands. */
#define	VC_C2SET	0x0004000	/* Count 2 set. */
#define	VC_ISDOT	0x0008000	/* Command was the dot command. */
	u_int	 flags;

#define	vp_endzero	keyword		/* END ZERO OUT. */
	char	*keyword;		/* Keyword. */
	size_t	 kbuflen;		/* Keyword buffer length. */
	/*
	 * There are four cursor locations that we worry about: the initial
	 * cursor position, the start of the range, the end of the range,
	 * and the final cursor position.  The initial cursor position and
	 * the start of the range are both m_start, and are always the same.
	 * All locations are initialized to the starting cursor position by
	 * the main vi routines, and the underlying functions depend on this.
	 *
	 * Commands that can be motion components set the end of the range
	 * cursor position, m_stop.  All commands must set the ending cursor
	 * position, m_final.  The reason that m_stop isn't the same as m_final
	 * is that there are situations where the final position of the cursor
	 * is outside of the cut/delete range (e.g. 'd[[' from the first column
	 * of a line).  The final cursor position often varies based on the
	 * direction of the movement, as well as the command.  The only special
	 * case that the delete code handles is that it will make adjustments
	 * if the final cursor position is deleted.
	 *
	 * The reason for all of this is that the historic vi semantics were
	 * defined command-by-command.  Every function has to roll its own
	 * starting and stopping positions, and adjust if it's being used as a
	 * motion component.  The general rules are as follows:
	 *	1: If not a motion component, the final cursor is at the end
	 *	   of the range.
	 *	2: If moving backward in the file:
	 *	 a:	VC_D moves to the end of the range.
	 *	 b:	If the line hasn't changed, VC_Y doesn't move, else it
	 *		moves to the end of the range.
	 *	3: If moving forward in the file, VC_D and VC_Y stay at the
	 *	   start of the range.
	 *
	 * Usually, if moving backward in the file and it's a motion component,
	 * the starting cursor is decremented by a single character (or, in a
	 * few cases, to the end of the previous line) so that the starting
	 * cursor character isn't cut or deleted.  No cursor adjustment is
	 * needed for moving forward, because the cut/delete routines handle
	 * m_stop inclusively, i.e. the last character in the range is cut or
	 * deleted.  This makes cutting to the EOF/EOL reasonable.
	 *
	 * We ignore VC_C and VC_S everywhere, because the text input routines
	 * always set the cursor to the last character inserted.
	 */
	MARK	 m_start;		/* mark: initial cursor, range start. */
	MARK	 m_stop;		/* mark: range end. */
	MARK	 m_final;		/* mark: final cursor position. */
} VICMDARG;

/* Vi command structure. */
struct _vikeys {			/* Underlying function. */
	int	 (*func) __P((SCR *, EXF *, VICMDARG *));
#define	V_ALREADY_USED	0x00007ff	/* VC_*, VM_* */
#define	V_ABS		0x0000800	/* Absolute movement, set '' mark. */
#define	V_CHAR		0x0001000	/* Character (required, trailing). */
#define	V_CNT		0x0002000	/* Count (optional, leading). */
#define	V_DOT		0x0004000	/* On success, sets dot command. */
#define	V_KEYNUM	0x0008000	/* Cursor referenced number. */
#define	V_KEYW		0x0010000	/* Cursor referenced word. */
#define	V_MOTION	0x0020000	/* Motion (required, trailing). */
#define	V_MOVE		0x0040000	/* Command defines movement. */
#define	V_OBUF		0x0080000	/* Buffer (optional, leading). */
#define	V_RBUF		0x0100000	/* Buffer (required, trailing). */
	u_long	 flags;
	char	*usage;			/* Usage line. */
	char	*help;			/* Help line. */
};
#define	MAXVIKEY	126		/* List of vi commands. */
extern VIKEYS const vikeys[MAXVIKEY + 1];

/* Definition of a vi "word". */
#define	inword(ch)	(isalnum(ch) || (ch) == '_')

/* Character stream structure, prototypes. */
typedef struct _vcs {
	recno_t	 cs_lno;		/* Line. */
	size_t	 cs_cno;		/* Column. */
	char	*cs_bp;			/* Buffer. */
	size_t	 cs_len;		/* Length. */
	int	 cs_ch;			/* Character. */
#define	CS_EMP	1			/* Empty line. */
#define	CS_EOF	2			/* End-of-file. */
#define	CS_EOL	3			/* End-of-line. */
#define	CS_SOF	4			/* Start-of-file. */
	int	 cs_flags;		/* Return flags. */
} VCS;

int	cs_bblank __P((SCR *, EXF *, VCS *));
int	cs_fblank __P((SCR *, EXF *, VCS *));
int	cs_fspace __P((SCR *, EXF *, VCS *));
int	cs_init __P((SCR *, EXF *, VCS *));
int	cs_next __P((SCR *, EXF *, VCS *));
int	cs_prev __P((SCR *, EXF *, VCS *));

					/* Character search information. */
enum cdirection	{ CNOTSET, FSEARCH, fSEARCH, TSEARCH, tSEARCH };

/* Vi private, per-screen memory. */
typedef struct _vi_private {
	VICMDARG sdot;			/* Saved dot, motion command. */
	VICMDARG sdotmotion;

	CHAR_T	 rlast;			/* Last 'r' command character. */

	char	*rep;			/* Input replay buffer. */
	size_t	 rep_len;		/* Input replay buffer length. */
	size_t	 rep_cnt;		/* Input replay buffer characters. */

	CHAR_T	 inc_lastch;		/* Last increment character. */
	long	 inc_lastval;		/* Last increment value. */

	char	*paragraph;		/* Paragraph search list. */
	size_t	 paragraph_len;		/* Paragraph search list length. */

	u_long	 u_ccnt;		/* Undo command count. */

	CHAR_T	 lastckey;		/* Last search character. */
	enum cdirection	csearchdir;	/* Character search direction. */
} VI_PRIVATE;

#define	VIP(sp)	((VI_PRIVATE *)((sp)->vi_private))

/* Vi function prototypes. */
int	txt_auto __P((SCR *, EXF *, recno_t, TEXT *, size_t, TEXT *));
int	v_buildparagraph __P((SCR *));
int	v_end __P((SCR *));
void	v_eof __P((SCR *, EXF *, MARK *));
void	v_eol __P((SCR *, EXF *, MARK *));
int	v_exwrite __P((void *, const char *, int));
int	v_init __P((SCR *, EXF *));
int	v_isempty __P((char *, size_t));
int	v_msgflush __P((SCR *));
void	v_nomove __P((SCR *));
int	v_ntext __P((SCR *, EXF *, TEXTH *, MARK *,
	    const char *, const size_t, MARK *, int, recno_t, u_int));
int	v_optchange __P((SCR *, int));
int	v_screen_copy __P((SCR *, SCR *));
int	v_screen_end __P((SCR *));
void	v_sof __P((SCR *, MARK *));
void	v_sol __P((SCR *));
int	vi __P((SCR *, EXF *));

#define	VIPROTO(name)	int name __P((SCR *, EXF *, VICMDARG *))
VIPROTO(v_again);
VIPROTO(v_at);
VIPROTO(v_bmark);
VIPROTO(v_bottom);
VIPROTO(v_cfirst);
VIPROTO(v_Change);
VIPROTO(v_change);
VIPROTO(v_chF);
VIPROTO(v_chf);
VIPROTO(v_chrepeat);
VIPROTO(v_chrrepeat);
VIPROTO(v_chT);
VIPROTO(v_cht);
VIPROTO(v_cr);
VIPROTO(v_Delete);
VIPROTO(v_delete);
VIPROTO(v_dollar);
VIPROTO(v_down);
VIPROTO(v_ex);
VIPROTO(v_exmode);
VIPROTO(v_filter);
VIPROTO(v_first);
VIPROTO(v_fmark);
VIPROTO(v_home);
VIPROTO(v_hpagedown);
VIPROTO(v_hpageup);
VIPROTO(v_iA);
VIPROTO(v_ia);
VIPROTO(v_iI);
VIPROTO(v_ii);
VIPROTO(v_increment);
VIPROTO(v_iO);
VIPROTO(v_io);
VIPROTO(v_join);
VIPROTO(v_left);
VIPROTO(v_lgoto);
VIPROTO(v_linedown);
VIPROTO(v_lineup);
VIPROTO(v_mark);
VIPROTO(v_match);
VIPROTO(v_middle);
VIPROTO(v_ncol);
VIPROTO(v_pagedown);
VIPROTO(v_pageup);
VIPROTO(v_paragraphb);
VIPROTO(v_paragraphf);
VIPROTO(v_Put);
VIPROTO(v_put);
VIPROTO(v_redraw);
VIPROTO(v_Replace);
VIPROTO(v_replace);
VIPROTO(v_right);
VIPROTO(v_screen);
VIPROTO(v_searchb);
VIPROTO(v_searchf);
VIPROTO(v_searchN);
VIPROTO(v_searchn);
VIPROTO(v_searchw);
VIPROTO(v_sectionb);
VIPROTO(v_sectionf);
VIPROTO(v_sentenceb);
VIPROTO(v_sentencef);
VIPROTO(v_shiftl);
VIPROTO(v_shiftr);
VIPROTO(v_status);
VIPROTO(v_stop);
VIPROTO(v_Subst);
VIPROTO(v_subst);
VIPROTO(v_switch);
VIPROTO(v_tagpop);
VIPROTO(v_tagpush);
VIPROTO(v_ulcase);
VIPROTO(v_Undo);
VIPROTO(v_undo);
VIPROTO(v_up);
VIPROTO(v_wordB);
VIPROTO(v_wordb);
VIPROTO(v_wordE);
VIPROTO(v_worde);
VIPROTO(v_wordW);
VIPROTO(v_wordw);
VIPROTO(v_Xchar);
VIPROTO(v_xchar);
VIPROTO(v_Yank);
VIPROTO(v_yank);
VIPROTO(v_z);
VIPROTO(v_zero);
VIPROTO(v_zexit);
