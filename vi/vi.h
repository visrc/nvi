/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: vi.h,v 5.11 1992/05/22 09:57:41 bostic Exp $ (Berkeley) $Date: 1992/05/22 09:57:41 $
 */

#include "exf.h"
#include "mark.h"

/* Structure passed around to functions implementing vi commands. */
typedef struct {
				/* ZERO OUT. */
	int buffer;		/* Buffer. */
	int character;		/* Character. */
	u_long count;		/* Count. */
	u_long count2;		/* Second count (only used by z). */
	int key;		/* Command key. */
	MARK motion;		/* Associated motion. */
	struct _vikeys *kp;	/* VIKEYS structure. */
	size_t klen;		/* Keyword length. */

#define	VC_C1SET	0x001	/* Count 1 set. */
#define	VC_C2SET	0x002	/* Count 2 set. */
#define	VC_LMODE	0x004	/* Motion is line oriented. */
#define	VC_ISDOT	0x008	/* Command was the dot command. */
#define	VC_ISMOTION	0x010	/* Decoding a motion. */
	u_int flags;
				/* DO NOT ZERO OUT. */
	char *keyword;		/* Keyword. */
	size_t kbuflen;		/* Keyword buffer length. */
} VICMDARG;

#define	vpstartzero	buffer
#define	vpendzero	keyword

/* Vi command structure. */
typedef struct _vikeys {	/* Underlying function. */
	int (*func) __P((VICMDARG *, MARK *, MARK *));

/* XXX Check to see if these are all needed. */
#define	V_ABS		0x0001	/* Absolute movement, sets the '' mark. */
#define	V_CHAR		0x0002	/* Character (required, trailing). */
#define	V_CNT		0x0004	/* Count (optional, leading). */
#define	V_DOT		0x0008	/* Successful command sets dot command. */
#define	V_KEYNUM	0x0010	/* Cursor referenced number. */
#define	V_KEYW		0x0020	/* Cursor referenced word. */
#define	V_LMODE		0x0040	/* Motion is line oriented. */
#define	V_MOTION	0x0080	/* Motion (required, trailing). */
#define	V_MOVE		0x0100	/* Command defines movement. */
#define	V_OBUF		0x0200	/* Buffer (optional, leading). */
#define	V_RBUF		0x0400	/* Buffer (required, trailing). */
#define	V_START		0x0800	/* Command implies SOL movement. */
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

int	v_again __P((VICMDARG *, MARK *, MARK *));
int	v_at __P((VICMDARG *, MARK *, MARK *));
int	v_bottom __P((VICMDARG *, MARK *, MARK *));
int	v_bsearch __P((VICMDARG *, MARK *, MARK *));
int	v_bsentence __P((VICMDARG *, MARK *, MARK *));
int	v_bword __P((VICMDARG *, MARK *, MARK *));
int	v_delete __P((VICMDARG *, MARK *, MARK *));
int	v_down __P((VICMDARG *, MARK *, MARK *));
void	v_eof __P((MARK *));
int	v_eol __P((VICMDARG *, MARK *, MARK *));
int	v_errlist __P((VICMDARG *, MARK *, MARK *));
int	v_ex __P((VICMDARG *, MARK *, MARK *));
int	v_Fch __P((VICMDARG *, MARK *, MARK *));
int	v_fch __P((VICMDARG *, MARK *, MARK *));
int	v_filter __P((VICMDARG *, MARK *, MARK *));
int	v_first __P((VICMDARG *, MARK *, MARK *));
int	v_fsearch __P((VICMDARG *, MARK *, MARK *));
int	v_fsentence __P((VICMDARG *, MARK *, MARK *));
int	v_home __P((VICMDARG *, MARK *, MARK *));
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
int	v_lgoto __P((VICMDARG *, MARK *, MARK *));
int	v_linedown __P((VICMDARG *, MARK *, MARK *));
int	v_lineup __P((VICMDARG *, MARK *, MARK *));
int	v_mark __P((VICMDARG *, MARK *, MARK *));
int	v_markbt __P((VICMDARG *, MARK *, MARK *));
int	v_marksq __P((VICMDARG *, MARK *, MARK *));
int	v_match __P((VICMDARG *, MARK *, MARK *));
int	v_middle __P((VICMDARG *, MARK *, MARK *));
int	v_nbdown __P((VICMDARG *, MARK *, MARK *));
int	v_nbup __P((VICMDARG *, MARK *, MARK *));
int	v_ncol __P((VICMDARG *, MARK *, MARK *));
int	v_nonblank __P((MARK *));
int	v_Nsearch __P((VICMDARG *, MARK *, MARK *));
int	v_nsearch __P((VICMDARG *, MARK *, MARK *));
int	v_pagedown __P((VICMDARG *, MARK *, MARK *));
int	v_pageup __P((VICMDARG *, MARK *, MARK *));
int	v_Put __P((VICMDARG *, MARK *, MARK *));
int	v_put __P((VICMDARG *, MARK *, MARK *));
int	v_redraw __P((VICMDARG *, MARK *, MARK *));
int	v_repeatch  __P((VICMDARG *, MARK *, MARK *));
int	v_right __P((VICMDARG *, MARK *, MARK *));
int	v_rrepeatch __P((VICMDARG *, MARK *, MARK *));
int	v_shiftl __P((VICMDARG *, MARK *, MARK *));
int	v_shiftr __P((VICMDARG *, MARK *, MARK *));
void	v_sof __P((MARK *));
void	v_startex __P((void));
int	v_status __P((VICMDARG *, MARK *, MARK *));
int	v_stop __P((VICMDARG *, MARK *, MARK *));
int	v_switch __P((VICMDARG *, MARK *, MARK *));
int	v_tag __P((VICMDARG *, MARK *, MARK *));
int	v_Tch __P((VICMDARG *, MARK *, MARK *));
int	v_tch __P((VICMDARG *, MARK *, MARK *));
int	v_ulcase __P((VICMDARG *, MARK *, MARK *));
int	v_up __P((VICMDARG *, MARK *, MARK *));
int	v_wordB __P((VICMDARG *, MARK *, MARK *));
int	v_wordb __P((VICMDARG *, MARK *, MARK *));
int	v_wordE __P((VICMDARG *, MARK *, MARK *));
int	v_worde __P((VICMDARG *, MARK *, MARK *));
int	v_wordW __P((VICMDARG *, MARK *, MARK *));
int	v_wordw __P((VICMDARG *, MARK *, MARK *));
int	v_wsearch __P((VICMDARG *, MARK *, MARK *));
int	v_Xchar __P((VICMDARG *, MARK *, MARK *));
int	v_xchar __P((VICMDARG *, MARK *, MARK *));
int	v_yank __P((VICMDARG *, MARK *, MARK *));
int	v_zero __P((VICMDARG *, MARK *, MARK *));

#ifndef VIROUTINE
MARK	*adjmove __P((MARK *, MARK *, int));
MARK	*input __P((MARK *, MARK *, int, int));
MARK	*m_paragraph __P((MARK *, long, int));
MARK	*m_tch __P((MARK *, long, int));
MARK	*m_z __P((MARK *, long, int));
MARK	*v_change __P((MARK *, MARK *));
MARK	*v_insert __P((MARK *, long, int));
MARK	*v_join __P((MARK *, long));
MARK	*v_overtype __P((MARK *));
MARK	*v_quit __P((void));
MARK	*v_replace __P((MARK *, long, int));
MARK	*v_selcut __P((MARK *, long, int));
MARK	*v_start __P((MARK *, long, int));
MARK	*v_subst __P((MARK *, long));
MARK	*v_undo __P((MARK *));
MARK	*v_undoline __P((MARK *));
void	 v_undosave __P((MARK *));
MARK	*v_xit __P((MARK *, long, int));
#else
int adjmove();
int input();
int m_paragraph();
int m_tch();
int m_z();
int v_change();
int v_insert();
int v_join();
int v_overtype();
int v_quit();
int v_replace();
int v_selcut();
int v_start();
int v_subst();
int v_undo();
int v_undoline();
void	 v_undosave();
int v_xit();
#endif
