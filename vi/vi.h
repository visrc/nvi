/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: vi.h,v 5.8 1992/05/15 11:11:26 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:11:26 $
 */

#include "exf.h"

/* Structure passed around to functions implementing vi commands. */
typedef struct {
	int buffer;		/* Buffer. */
	int character;		/* Character. */
	u_long count;		/* Count. */
	u_long count2;		/* Second count (only used by z). */
	int key;		/* Command key. */
	MARK motion;		/* Associated motion. */
	struct _vikeys *kp;	/* VIKEYS structure. */
	char *keyword;		/* Keyword. */
	u_long kcstart;		/* Beginning of keyword. */
	u_long kcstop;		/* End of keyword. */
	size_t klen;		/* Keyword length. */

#define	VC_C1SET	0x001	/* Count 1 set. */
#define	VC_C2SET	0x002	/* Count 2 set. */
	u_int flags;
} VICMDARG;

/* Vi command structure. */
typedef struct _vikeys {	/* Underlying function. */
	int (*func) __P((VICMDARG *, MARK *, MARK *));

/* XXX Check to see if these are all needed. */
#define	V_ABS		0x0001	/* Absolute movement, sets the '' mark. */
#define	V_CHAR		0x0002	/* Character (required, trailing). */
#define	V_CNT		0x0004	/* Count (optional, leading). */
#define	V_DOT		0x0008	/* Successful command sets dot command. */
#define	V_KEYW		0x0010	/* Keyword. */
#define	V_MOTION	0x0020	/* Motion (required, trailing). */
#define	V_MOVE		0x0040	/* Command defines movement. */
#define	V_OBUF		0x0080	/* Buffer (optional, leading). */
#define	V_RBUF		0x0100	/* Buffer (required, trailing). */
#define	V_START		0x0200	/* Command implies SOL movement. */
	u_int flags;
	char *usage;		/* Usage line. */
} VIKEYS;

#define	MAXVIKEY	126	/* List of vi commands. */
extern VIKEYS vikeys[MAXVIKEY + 1];

/* This macro is used to set the default count value for an operation */
#define SETDEFCNT(val) {						\
	if (cnt < 1)							\
		cnt = (val);						\
}

/* Get current line. */
#define	GETLINE(p, lno, len) {						\
	u_long __lno = (lno);						\
	p = file_gline(curf, __lno, &(len));				\
}

/* Get current line; an error if fail. */
#define	EGETLINE(p, lno, len) {						\
	u_long __lno = (lno);						\
	p = file_gline(curf, __lno, &(len));				\
	if (p == NULL) {						\
		bell();							\
		msg("Unable to retrieve line %lu.", lno);		\
		return (1);						\
	}								\
}

int	v_again __P((VICMDARG *, MARK *, MARK *));
int	v_at __P((VICMDARG *, MARK *, MARK *));
int	v_bsearch __P((VICMDARG *, MARK *, MARK *));
int	v_bsentence __P((VICMDARG *, MARK *, MARK *));
int	v_bword __P((VICMDARG *, MARK *, MARK *));
int	v_down __P((VICMDARG *, MARK *, MARK *));
void	v_eof __P((MARK *));
int	v_eol __P((VICMDARG *, MARK *, MARK *));
int	v_errlist __P((VICMDARG *, MARK *, MARK *));
int	v_ex __P((VICMDARG *, MARK *, MARK *));
int	v_fch __P((VICMDARG *, MARK *, MARK *));
int	v_fch __P((VICMDARG *, MARK *, MARK *));
int	v_filter __P((VICMDARG *, MARK *, MARK *));
int	v_fsearch __P((VICMDARG *, MARK *, MARK *));
int	v_fsentence __P((VICMDARG *, MARK *, MARK *));
int	v_hpagedown __P((VICMDARG *, MARK *, MARK *));
int	v_hpageup __P((VICMDARG *, MARK *, MARK *));
int	v_iA __P((VICMDARG *, MARK *, MARK *));
int	v_ia __P((VICMDARG *, MARK *, MARK *));
int	v_iI __P((VICMDARG *, MARK *, MARK *));
int	v_ii __P((VICMDARG *, MARK *, MARK *));
int	v_increment __P((VICMDARG *, MARK *, MARK *));
int	v_iO __P((VICMDARG *, MARK *, MARK *));
int	v_io __P((VICMDARG *, MARK *, MARK *));
void	v_leaveex __P((void));
int	v_left __P((VICMDARG *, MARK *, MARK *));
int	v_linedown __P((VICMDARG *, MARK *, MARK *));
int	v_lineup __P((VICMDARG *, MARK *, MARK *));
int	v_mark __P((VICMDARG *, MARK *, MARK *));
int	v_markbt __P((VICMDARG *, MARK *, MARK *));
int	v_marksq __P((VICMDARG *, MARK *, MARK *));
int	v_match __P((VICMDARG *, MARK *, MARK *));
int	v_nbdown __P((VICMDARG *, MARK *, MARK *));
int	v_nbup __P((VICMDARG *, MARK *, MARK *));
int	v_nonblank __P((MARK *));
int	v_Nsearch __P((VICMDARG *, MARK *, MARK *));
int	v_nsearch __P((VICMDARG *, MARK *, MARK *));
int	v_pagedown __P((VICMDARG *, MARK *, MARK *));
int	v_pageup __P((VICMDARG *, MARK *, MARK *));
int	v_rch __P((VICMDARG *, MARK *, MARK *));
int	v_redraw __P((VICMDARG *, MARK *, MARK *));
int	v_repeatch  __P((VICMDARG *, MARK *, MARK *));
int	v_right __P((VICMDARG *, MARK *, MARK *));
int	v_rrepeatch __P((VICMDARG *, MARK *, MARK *));
int	v_shiftl __P((VICMDARG *, MARK *, MARK *));
int	v_shiftr __P((VICMDARG *, MARK *, MARK *));
void	v_sof __P((MARK *));
void	v_startex __P((void));
int	v_status __P((VICMDARG *, MARK *, MARK *));
int	v_switch __P((VICMDARG *, MARK *, MARK *));
int	v_tag __P((VICMDARG *, MARK *, MARK *));
int	v_tfch __P((VICMDARG *, MARK *, MARK *));
int	v_trch __P((VICMDARG *, MARK *, MARK *));
int	v_up __P((VICMDARG *, MARK *, MARK *));
int	v_wordB __P((VICMDARG *, MARK *, MARK *));
int	v_wordb __P((VICMDARG *, MARK *, MARK *));
int	v_wordE __P((VICMDARG *, MARK *, MARK *));
int	v_worde __P((VICMDARG *, MARK *, MARK *));
int	v_wordW __P((VICMDARG *, MARK *, MARK *));
int	v_wordw __P((VICMDARG *, MARK *, MARK *));
int	v_wsearch __P((VICMDARG *, MARK *, MARK *));
int	v_zero __P((VICMDARG *, MARK *, MARK *));
int	v_ulcase __P((VICMDARG *, MARK *, MARK *));

#ifndef VIROUTINE
MARK	*adjmove __P((MARK *, MARK *, int));
MARK	*input __P((MARK *, MARK *, int, int));
MARK	*m_front __P((MARK *, long));
MARK	*m_paragraph __P((MARK *, long, int));
MARK	*m_row __P((MARK *, long, int));
MARK	*m_scroll __P((MARK *, long, int));
MARK	*m_tch __P((MARK *, long, int));
MARK	*m_tocol __P((MARK *, long, int));
MARK	*m_updnto __P((MARK *, long, int));
MARK	*m_z __P((MARK *, long, int));
MARK	*v_change __P((MARK *, MARK *));
MARK	*v_delete __P((MARK *, MARK *));
MARK	*v_insert __P((MARK *, long, int));
MARK	*v_join __P((MARK *, long));
MARK	*v_overtype __P((MARK *));
MARK	*v_paste __P((MARK *, long, int));
MARK	*v_quit __P((void));
MARK	*v_replace __P((MARK *, long, int));
MARK	*v_selcut __P((MARK *, long, int));
MARK	*v_start __P((MARK *, long, int));
MARK	*v_subst __P((MARK *, long));
MARK	*v_undo __P((MARK *));
MARK	*v_undoline __P((MARK *));
void	 v_undosave __P((MARK *));
MARK	*v_xchar __P((MARK *, long, int));
MARK	*v_xit __P((MARK *, long, int));
MARK	*v_yank __P((MARK *, MARK *));
#else
int adjmove();
int input();
int m_front();
int m_paragraph();
int m_row();
int m_scroll();
int m_tch();
int m_tocol();
int m_updnto();
int m_z();
int v_change();
int v_delete();
int v_insert();
int v_join();
int v_overtype();
int v_paste();
int v_quit();
int v_replace();
int v_selcut();
int v_start();
int v_subst();
int v_undo();
int v_undoline();
void	 v_undosave();
int v_xchar();
int v_xit();
int v_yank();
#endif
