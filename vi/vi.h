/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: vi.h,v 5.4 1992/04/22 08:10:04 bostic Exp $ (Berkeley) $Date: 1992/04/22 08:10:04 $
 */

#define	C_C_K_REP1	(CURSOR_CNT_KEY | 0x10)
#define C_C_K_CUT	(CURSOR_CNT_KEY | 0x20)
#define C_C_K_MARK	(CURSOR_CNT_KEY | 0x30)
#define C_C_K_CHAR	(CURSOR_CNT_KEY | 0x40)
#define	KEYMODE(args) (keymodes[(args) >> 4])

/* Vi command structure. */
typedef struct {
	MARK (*func)();		/* Underlying function. */

#define CURSOR		1
#define CURSOR_CNT_KEY	2
#define CURSOR_MOVED	3
#define CURSOR_EOL	4
#define ZERO		5
#define DIGIT		6
#define CURSOR_TEXT	7
#define KEYWORD		8
#define ARGSMASK	0x0f
	u_short	args;		/* Description of function arguments. */

#define	MVMT		0x001	/* Movement command. */
#define FRNT		0x002	/* After move, go to column 1. */
#define INCL		0x004	/* Include last char when used with c/d/y. */
#define LNMD		0x008	/* Use line mode of c/d/y. */
#define NCOL		0x010	/* Can't change the column number. */
#define NREL		0x020	/* Non-relative -- set the default mark. */
#define PTMV		0x040	/* Part of a movement command. */
#define SDOT		0x080	/* Set the "dot" info, for the "." command. */
#define VIZ		0x100	/* Command can be used with 'v'. */
	u_short flags;
} VIKEYS;
extern VIKEYS vikeys[];		/* List of vi commands. */

/* This macro is used to set the default value of cnt for an operation */
#define SETDEFCNT(val) { \
	if (cnt < 1) \
		cnt = (val); \
}

MARK	adjmove();		/* a helper fn, used by move fns */
MARK	m_Fch();		/* F */
MARK	m_Nsrch();		/* N */
MARK	m_Tch();		/* T */
MARK	m__ch();		/* ; , */
MARK	m_bsentence();		/* ( */
MARK	m_bsrch();		/* ?regexp */
MARK	m_bword();		/* b */
MARK	m_eword();		/* e */
MARK	m_fch();		/* f */
MARK	m_front();		/* ^ */
MARK	m_fsentence();		/* ) */
MARK	m_fsrch();		/* /regexp */
MARK	m_fword();		/* w */
MARK	m_left();		/* l */
MARK	m_match();		/* % */
MARK	m_nsrch();		/* n */
MARK	m_paragraph();		/* { } [[ ]] */
MARK	m_rear();		/* $ */
MARK	m_right();		/* h */
MARK	m_row();		/* H L M */
MARK	m_scroll();		/* ^B ^F ^E ^Y ^U ^D */
MARK	m_tch();		/* t */
MARK	m_tocol();		/* | */
MARK	m_tomark();		/* 'm */
MARK	m_updnto();		/* k j G */
MARK	m_wsrch();		/* ^A */
MARK	m_z();			/* z */
MARK	v_again __P((MARK, MARK));
MARK	v_at __P((MARK, long, int));
MARK	v_change __P((MARK, MARK));
MARK	v_delete __P((MARK, MARK));
MARK	v_errlist __P((MARK));
MARK	v_ex __P((MARK, char *));
MARK	v_filter __P((MARK, MARK));
MARK	v_increment __P((char *, MARK, long));
MARK	v_insert __P((MARK, long, int));
MARK	v_join __P((MARK, long));
MARK	v_mark __P((MARK, long, int));
MARK	v_overtype __P((MARK));
MARK	v_paste __P((MARK, long, int));
MARK	v_quit __P((void));
MARK	v_redraw __P((void));
MARK	v_replace __P((MARK, long, int));
MARK	v_selcut __P((MARK, long, int));
MARK	v_shiftl __P((MARK, MARK));
MARK	v_shiftr __P((MARK, MARK));
MARK	v_start __P((MARK, long, int));
MARK	v_status __P((void));
MARK	v_subst __P((MARK, long));
MARK	v_switch __P((void));
MARK	v_tag __P((char *, MARK, long));
MARK	v_ulcase __P((MARK, long));
MARK	v_undo __P((MARK));
MARK	v_undoline __P((MARK));
MARK	v_xchar __P((MARK, long, int));
MARK	v_xit __P((MARK, long, int));
MARK	v_yank __P((MARK, MARK));
