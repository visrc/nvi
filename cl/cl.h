/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: cl.h,v 10.4 1995/07/06 11:53:37 bostic Exp $ (Berkeley) $Date: 1995/07/06 11:53:37 $
 */

typedef struct _cl_private {
	CHAR_T	 ibuf[256];	/* Input keys. */
	int	 icnt;		/* Input key count. */

	int	 eof_count;	/* EOF count. */

				/* Busy state. */
	enum { BUSY_OFF=0, BUSY_ON, BUSY_SILENT } busy_state;
	int	 busy_ch;	/* Busy character. */
	struct timeval
		 busy_tv;	/* Busy timer. */
	size_t	 busy_fx;	/* Busy character x coordinate. */
	size_t	 busy_y, busy_x;/* Busy saved screen coordinates. */

	mtype_t	 mtype;		/* Last displayed message type. */
	size_t	 linecount;	/* 1-N: Output overwrite count. */
	size_t	 lcontinue;	/* 1-N: Output line continue value. */
	size_t	 totalcount;	/* 1-N: Output overwrite count. */

	CHAR_T	*lline;		/* Last line retrieved or copied. */
	size_t	 lline_len;	/* Length of last line. */
	size_t	 lline_blen;	/* Length of last line buffer. */

	struct termios exterm;	/* Terminal values ex restores. */
	char	*el;		/* Clear to EOL terminal string. */
	char	*cup;		/* Cursor movement terminal string. */
	char	*cuu1;		/* Cursor up terminal string. */
	char	*rmso, *smso;	/* Inverse video terminal strings. */

#define	INDX_CONT	0	/* Signal array offsets. */
#define	INDX_HUP	1
#define	INDX_INT	2
#define	INDX_TERM	3
#define	INDX_WINCH	4
#define	INDX_MAX	5	/* Original signal information. */
	struct sigaction oact[INDX_MAX];

#define	CL_DIVIDER	0x00001	/* Divider line was displayed. */
#define	CL_EX_WROTE	0x00002	/* Ex wrote output in canonical mode. */
#define	CL_INIT_EX	0x00004	/* Ex curses/terminal initialized. */
#define	CL_INIT_VI	0x00008	/* Vi curses/terminal initialized. */
#define	CL_LLINE	0x00010	/* If cursor on the screen's last line. */
#define	CL_LLINE_IV	0x00020	/* If the last line is inverse video. */
#define	CL_PRIVWRITE	0x00040	/* Local screen call, don't ignore. */
#define	CL_REPAINT	0x00080	/* Deleteln/insertln ignored, repaint. */
#define	CL_SIGCONT	0x00100	/* SIGCONT arrived. */
#define	CL_SIGHUP	0x00200	/* SIGHUP arrived. */
#define	CL_SIGINT	0x00400	/* SIGINT arrived. */
#define	CL_SIGTERM	0x00800	/* SIGTERM arrived. */
#define	CL_SIGWINCH	0x01000	/* SIGWINCH arrived. */
	u_int16_t flags;
} CL_PRIVATE;

extern GS *__global_list;	/* GLOBAL: List of screens. */

#define	CLP(sp)		((CL_PRIVATE *)((sp)->gp->cl_private))

/* Return possibilities from the keyboard read routine. */
typedef enum { INP_OK=0, INP_EOF, INP_ERR, INP_INTR, INP_TIMEOUT } input_t;

/* The screen line relative to a specific window. */
#define	RLNO(sp, lno)	(sp)->woff + (lno)

/*
 * Ex/vi routines can run before the screen really exists, which means that
 * we have to be careful not to step on anything.
 */
#define	VI_INIT_IGNORE(sp)						\
	if (F_ISSET(sp, S_VI) && !F_ISSET(CLP(sp), CL_INIT_VI))		\
		return (0);
/* A similar check when running in ex mode. */
#define	EX_INIT_IGNORE(sp)						\
	if (F_ISSET(sp, S_EX) && !F_ISSET(CLP(sp), CL_INIT_EX))		\
		return (0);
/* Some functions are noop's in ex mode. */
#define	EX_NOOP(sp)							\
	if (F_ISSET(sp, S_EX))						\
		return (0);
/* Some functions should never be called in vi mode. */
#define	VI_ABORT(sp)							\
	if (F_ISSET(sp, S_VI))						\
		abort();
/* Some functions should never be called in ex mode. */
#define	EX_ABORT(sp)							\
	if (F_ISSET(sp, S_EX))						\
		abort();

#define	SCROLL_QUIT	0x01		/* User can enter 'q' to interrupt. */
#define	SCROLL_WAIT	0x02		/* User must enter continuation char. */

#include "cl_extern.h"
