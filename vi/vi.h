/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: vi.h,v 5.39 1993/04/17 12:07:10 bostic Exp $ (Berkeley) $Date: 1993/04/17 12:07:10 $
 */

/* Structure passed around to functions implementing vi commands. */
typedef struct _vicmdarg {
				/* ZERO OUT. */
	int buffer;		/* Buffer. */
	int character;		/* Character. */
	u_long count;		/* Count. */
	u_long count2;		/* Second count (only used by z). */
	int key;		/* Command key. */
				/* VIKEYS structure. */
	struct _vikeys const *kp;
	size_t klen;		/* Keyword length. */

/*
 * Historic vi allowed "dl" when the cursor was on the last column, deleting
 * the last character, and similarly allowed "dw" when the cursor was on the
 * last column of the file.  It didn't allow "dh" when the cursor was on
 * column 1, although these cases are not strictly analogous.  The point is
 * that some movements would succeed if they were associated with a motion
 * command, and fail otherwise.  This is part of the off-by-1 schizophrenia
 * that plagued vi.  Other examples are that "dfb" deleted everything up to
 * and including the next 'b' character, but "d/b" only deleted everything
 * up to the next 'b' character.  While this implementation regularizes the
 * interface to the extent possible, there are many special cases that can't
 * be fixed.  This is implemented by setting special flags per command so that
 * the motion routines know what's really going on.
 *
 * Note, the VC_C flags and VC_D flags are set per command, and therefore
 * must have values not used the set of flags used by the VIKEYS structure
 * below.
 */
#define	VC_C		0x001	/* The 'c' command. */
#define	VC_D		0x002	/* The 'd' command. */
#define	VC_COMMASK	0x003	/* Mask for special flags VC_C and VC_D. */

#define	VC_C1SET	0x004	/* Count 1 set. */
#define	VC_C1RESET	0x008	/* Reset the C1SET flag for dot commands. */
#define	VC_C2SET	0x010	/* Count 2 set. */
#define	VC_LMODE	0x020	/* Motion is line oriented. */
#define	VC_ISDOT	0x040	/* Command was the dot command. */

	u_int flags;
				/* DO NOT ZERO OUT. */
	char *keyword;		/* Keyword. */
	size_t kbuflen;		/* Keyword buffer length. */
} VICMDARG;

#define	vpstartzero	buffer
#define	vpendzero	keyword

/* Vi command structure. */
typedef struct _vikeys {	/* Underlying function. */
	int (*func) __P((SCR *, EXF *,
	    VICMDARG *, MARK *, MARK *, MARK *));

/* XXX Check to see if these are all needed. */
#define	V_DONTUSE1	0x00001	/* VC_C */
#define	V_DONTUSE2	0x00002	/* VC_D */
#define	V_ABS		0x00004	/* Absolute movement, sets the '' mark. */
#define	V_CHAR		0x00008	/* Character (required, trailing). */
#define	V_CNT		0x00010	/* Count (optional, leading). */
#define	V_DOT		0x00020	/* Successful command sets dot command. */
#define	V_KEYNUM	0x00040	/* Cursor referenced number. */
#define	V_KEYW		0x00080	/* Cursor referenced word. */
#define	V_LMODE		0x00100	/* Motion is line oriented. */
#define	V_MOTION	0x00200	/* Motion (required, trailing). */
#define	V_MOVE		0x00400	/* Command defines movement. */
#define	V_OBUF		0x00800	/* Buffer (optional, leading). */
#define	V_RBUF		0x01000	/* Buffer (required, trailing). */
#define	V_RCM		0x02000	/* Use relative cursor movment (RCM). */
#define	V_RCM_SET	0x04000	/* Set RCM absolutely. */
#define	V_RCM_SETFNB	0x08000	/* Set RCM to first non-blank character. */
#define	V_RCM_SETLAST	0x10000	/* Set RCM to last character. */
	u_long flags;
	char *usage;		/* Usage line. */
} VIKEYS;

#define	MAXVIKEY	126	/* List of vi commands. */
extern VIKEYS const vikeys[MAXVIKEY + 1];

/* Definition of a "word". */
#define	inword(ch)	(isalnum(ch) || (ch) == '_')

#define	EMPTYLINE	-1

/* Vi function prototypes. */
int	getc_init __P((SCR *, EXF *, MARK *, int *));
int	getc_next __P((SCR *, EXF *, enum direction, int *));
void	getc_set __P((SCR *, EXF *, MARK *));
int	txt_auto __P((SCR *, EXF *, recno_t, TEXT *));
int	v_comment __P((SCR *, EXF *));
int	v_end __P((SCR *));
void	v_eof __P((SCR *, EXF *, MARK *));
void	v_eol __P((SCR *, EXF *, MARK *));
int	v_exwrite __P((void *, const char *, int));
int	v_init __P((SCR *, EXF *));
int	v_msgflush __P((SCR *));
int	v_ntext __P((SCR *, EXF *, HDR *,
	    MARK *, char *, size_t, MARK *, int, recno_t, u_int));
void	v_sof __P((SCR *, MARK *));

#define	VIPROTO(type, name)						\
	type	name __P((SCR *, EXF *,	VICMDARG *, MARK *, MARK *, MARK *))

VIPROTO(int, v_again);
VIPROTO(int, v_at);
VIPROTO(int, v_bottom);
VIPROTO(int, v_Change);
VIPROTO(int, v_change);
VIPROTO(int, v_chF);
VIPROTO(int, v_chf);
VIPROTO(int, v_chrepeat);
VIPROTO(int, v_chrrepeat);
VIPROTO(int, v_chT);
VIPROTO(int, v_cht);
VIPROTO(int, v_Delete);
VIPROTO(int, v_delete);
VIPROTO(int, v_dollar);
VIPROTO(int, v_down);
VIPROTO(int, v_errlist);
VIPROTO(int, v_ex);
VIPROTO(int, v_exit);
VIPROTO(int, v_exmode);
VIPROTO(int, v_filter);
VIPROTO(int, v_first);
VIPROTO(int, v_home);
VIPROTO(int, v_hpagedown);
VIPROTO(int, v_hpageup);
VIPROTO(int, v_iA);
VIPROTO(int, v_ia);
VIPROTO(int, v_iI);
VIPROTO(int, v_ii);
VIPROTO(int, v_increment);
VIPROTO(int, v_iO);
VIPROTO(int, v_io);
VIPROTO(int, v_join);
VIPROTO(int, v_left);
VIPROTO(int, v_lgoto);
VIPROTO(int, v_linedown);
VIPROTO(int, v_lineup);
VIPROTO(int, v_mark);
VIPROTO(int, v_markbt);
VIPROTO(int, v_marksq);
VIPROTO(int, v_match);
VIPROTO(int, v_middle);
VIPROTO(int, v_ncol);
VIPROTO(int, v_pagedown);
VIPROTO(int, v_pageup);
VIPROTO(int, v_paragraphb);
VIPROTO(int, v_paragraphf);
VIPROTO(int, v_Put);
VIPROTO(int, v_put);
VIPROTO(int, v_redraw);
VIPROTO(int, v_Replace);
VIPROTO(int, v_replace);
VIPROTO(int, v_right);
VIPROTO(int, v_searchb);
VIPROTO(int, v_searchf);
VIPROTO(int, v_searchN);
VIPROTO(int, v_searchn);
VIPROTO(int, v_searchw);
VIPROTO(int, v_sectionb);
VIPROTO(int, v_sectionf);
VIPROTO(int, v_sentenceb);
VIPROTO(int, v_sentencef);
VIPROTO(int, v_shiftl);
VIPROTO(int, v_shiftr);
VIPROTO(int, v_status);
VIPROTO(int, v_stop);
VIPROTO(int, v_subst);
VIPROTO(int, v_switch);
VIPROTO(int, v_tagpop);
VIPROTO(int, v_tagpush);
VIPROTO(int, v_ulcase);
VIPROTO(int, v_Undo);
VIPROTO(int, v_undo);
VIPROTO(int, v_up);
VIPROTO(int, v_window);
VIPROTO(int, v_wordB);
VIPROTO(int, v_wordb);
VIPROTO(int, v_wordE);
VIPROTO(int, v_worde);
VIPROTO(int, v_wordW);
VIPROTO(int, v_wordw);
VIPROTO(int, v_Xchar);
VIPROTO(int, v_xchar);
VIPROTO(int, v_Yank);
VIPROTO(int, v_yank);
VIPROTO(int, v_z);
VIPROTO(int, v_zero);
