/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: vi.h,v 5.7 1992/05/07 12:49:57 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:49:57 $
 */

#define	C_C_K_REP1	(CURSOR_CNT_KEY | 0x10)
#define C_C_K_CUT	(CURSOR_CNT_KEY | 0x20)
#define C_C_K_MARK	(CURSOR_CNT_KEY | 0x30)
#define C_C_K_CHAR	(CURSOR_CNT_KEY | 0x40)
#define	KEYMODE(args) (keymodes[(args) >> 4])

/* Vi command structure. */
typedef struct {
	MARK *(*func)();	/* Underlying function. */

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

MARK	*adjmove __P((MARK *, MARK *, int));
MARK	*input __P((MARK *, MARK *, int, int));
MARK	*m_Fch __P((MARK *, long, int));
MARK	*m_Tch __P((MARK *, long, int));
MARK	*m__ch __P((MARK *, long, int));
MARK	*m_bsentence __P((MARK *, long));
MARK	*m_bword __P((MARK *, long, int));
MARK	*m_eword __P((MARK *, long, int));
MARK	*m_fch __P((MARK *, long, int));
MARK	*m_front __P((MARK *, long));
MARK	*m_fsentence __P((MARK *, long));
MARK	*m_fword __P((MARK *, long, int, int));
MARK	*m_left __P((MARK *, long));
MARK	*m_match __P((MARK *, long));
MARK	*m_paragraph __P((MARK *, long, int));
MARK	*m_rear __P((MARK *, long));
MARK	*m_right __P((MARK *, long));
MARK	*m_row __P((MARK *, long, int));
MARK	*m_scroll __P((MARK *, long, int));
MARK	*m_tch __P((MARK *, long, int));
MARK	*m_tocol __P((MARK *, long, int));
MARK	*m_tomark __P((MARK *, long, int));
MARK	*m_updnto __P((MARK *, long, int));
MARK	*m_z __P((MARK *, long, int));
MARK	*v_Nsearch __P((MARK *));
MARK	*v_again __P((MARK *, MARK *));
MARK	*v_at __P((MARK *, long, int));
MARK	*v_change __P((MARK *, MARK *));
MARK	*v_delete __P((MARK *, MARK *));
MARK	*v_errlist __P((MARK *));
MARK	*v_ex __P((void));
MARK	*v_filter __P((MARK *, MARK *));
MARK	*v_increment __P((char *, MARK *, long));
MARK	*v_insert __P((MARK *, long, int));
MARK	*v_join __P((MARK *, long));
void	 v_leaveex __P((void));
MARK	*v_mark __P((MARK *, long, int));
MARK	*v_nsearch __P((MARK *));
MARK	*v_overtype __P((MARK *));
MARK	*v_paste __P((MARK *, long, int));
MARK	*v_quit __P((void));
MARK	*v_redraw __P((void));
MARK	*v_replace __P((MARK *, long, int));
MARK	*v_selcut __P((MARK *, long, int));
MARK	*v_shiftl __P((MARK *, MARK *));
MARK	*v_shiftr __P((MARK *, MARK *));
MARK	*v_start __P((MARK *, long, int));
void	 v_startex __P((void));
MARK	*v_status __P((void));
MARK	*v_subst __P((MARK *, long));
MARK	*v_switch __P((void));
MARK	*v_tag __P((char *, MARK *, long));
MARK	*v_ulcase __P((MARK *, long));
MARK	*v_undo __P((MARK *));
MARK	*v_undoline __P((MARK *));
void	 v_undosave __P((MARK *));
MARK	*v_wsearch __P((char *, MARK *, int));
MARK	*v_xchar __P((MARK *, long, int));
MARK	*v_xit __P((MARK *, long, int));
MARK	*v_yank __P((MARK *, MARK *));
