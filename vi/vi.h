/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: vi.h,v 5.26 1992/12/20 15:54:28 bostic Exp $ (Berkeley) $Date: 1992/12/20 15:54:28 $
 */

#include "exf.h"

/* Structure passed around to functions implementing vi commands. */
typedef struct {
				/* ZERO OUT. */
	int buffer;		/* Buffer. */
	int character;		/* Character. */
	u_long count;		/* Count. */
	u_long count2;		/* Second count (only used by z). */
	int key;		/* Command key. */
	struct _vikeys *kp;	/* VIKEYS structure. */
	size_t klen;		/* Keyword length. */

#define	VC_C1SET	0x001	/* Count 1 set. */
#define	VC_C1RESET	0x002	/* Reset the C1SET flag for dot commands. */
#define	VC_C2SET	0x004	/* Count 2 set. */
#define	VC_LMODE	0x008	/* Motion is line oriented. */
#define	VC_ISDOT	0x010	/* Command was the dot command. */

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
 */
#define	VC_C		0x020	/* The 'c' command. */
#define	VC_D		0x040	/* The 'd' command. */
#define	VC_COMMASK	0x060	/* Mask for special flags. */
	u_int flags;
				/* DO NOT ZERO OUT. */
	char *keyword;		/* Keyword. */
	size_t kbuflen;		/* Keyword buffer length. */
} VICMDARG;

#define	vpstartzero	buffer
#define	vpendzero	keyword

/* Vi command structure. */
typedef struct _vikeys {	/* Underlying function. */
	int (*func) __P((VICMDARG *, MARK *, MARK *, MARK *));

/* XXX Check to see if these are all needed. */
#define	V_ABS		0x00001	/* Absolute movement, sets the '' mark. */
#define	V_CHAR		0x00002	/* Character (required, trailing). */
#define	V_CNT		0x00004	/* Count (optional, leading). */
#define	V_DOT		0x00008	/* Successful command sets dot command. */
#define	V_KEYNUM	0x00010	/* Cursor referenced number. */
#define	V_KEYW		0x00020	/* Cursor referenced word. */
#define	V_INPUT		0x00040	/* Input command. */
#define	V_LMODE		0x00080	/* Motion is line oriented. */
#define	V_MOTION	0x00100	/* Motion (required, trailing). */
#define	V_MOVE		0x00200	/* Command defines movement. */
#define	V_OBUF		0x00400	/* Buffer (optional, leading). */
#define	V_RBUF		0x00800	/* Buffer (required, trailing). */
#define	V_RCM		0x01000	/* Use relative cursor movment (RCM). */
#define	V_RCM_SET	0x02000	/* Set RCM absolutely. */
#define	V_RCM_SETFNB	0x04000	/* Set RCM to first non-blank character. */
#define	V_RCM_SETLAST	0x08000	/* Set RCM to last character. */
	u_long flags;
	char *usage;		/* Usage line. */
} VIKEYS;

#define	MAXVIKEY	126	/* List of vi commands. */
extern VIKEYS vikeys[MAXVIKEY + 1];

/* Definition of a "word". */
#define	inword(ch)	(isalnum(ch) || (ch) == '_')

void	status __P((EXF *, recno_t));
int	v_msgflush __P((EXF *));

int	v_again __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_at __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_bottom __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_Change __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_change __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_chF __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_chf __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_chrepeat  __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_chrrepeat __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_chT __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_cht __P((VICMDARG *, MARK *, MARK *, MARK *));
enum confirmation v_confirm __P((EXF *, MARK *, MARK *));
void	v_comment __P((EXF *));
int	v_Delete __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_delete __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_dollar __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_down __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_end __P((EXF *));
void	v_eof __P((MARK *));
void	v_eol __P((MARK *));
int	v_errlist __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_ex __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_exwrite __P((void *, const char *, int));
int	v_exit __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_filter __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_first __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_home __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_hpagedown __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_hpageup __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_iA __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_ia __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_iI __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_ii __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_increment __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_init __P((EXF *));
int	v_iO __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_io __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_join __P((VICMDARG *, MARK *, MARK *, MARK *));
void	v_leaveex __P((void));
int	v_left __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_lgoto __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_linedown __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_lineup __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_mark __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_markbt __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_marksq __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_match __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_middle __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_nbdown __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_nbup __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_ncol __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_pagedown __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_pageup __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_paragraphb __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_paragraphf __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_Put __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_put __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_quit __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_redraw __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_Replace __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_replace __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_right __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_searchb __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_searchf __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_searchN __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_searchn __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_searchw __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_sectionb __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_sectionf __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_sentenceb __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_sentencef __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_shiftl __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_shiftr __P((VICMDARG *, MARK *, MARK *, MARK *));
void	v_sof __P((MARK *));
void	v_startex __P((void));
int	v_status __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_stop __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_subst __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_switch __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_tagpop __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_tagpush __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_ulcase __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_Undo __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_undo __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_up __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_wordB __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_wordb __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_wordE __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_worde __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_wordW __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_wordw __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_Xchar __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_xchar __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_Yank __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_yank __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_z __P((VICMDARG *, MARK *, MARK *, MARK *));
int	v_zero __P((VICMDARG *, MARK *, MARK *, MARK *));
