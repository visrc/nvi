/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: cl.h,v 10.6 1995/11/06 19:31:49 bostic Exp $ (Berkeley) $Date: 1995/11/06 19:31:49 $
 */

typedef struct _cl_private {
	CHAR_T	 ibuf[256];	/* Input keys. */

	int	 eof_count;	/* EOF count. */

	struct termios orig;	/* Original terminal values. */
	struct termios ex_enter;/* Terminal values to enter ex. */
	struct termios vi_enter;/* Terminal values to enter vi. */

	char	*el;		/* Clear to EOL terminal string. */
	char	*cup;		/* Cursor movement terminal string. */
	char	*cuu1;		/* Cursor up terminal string. */
	char	*rmso, *smso;	/* Inverse video terminal strings. */

	int	 killersig;	/* Killer signal. */
#define	INDX_HUP	0
#define	INDX_INT	1
#define	INDX_TERM	2
#define	INDX_WINCH	3
#define	INDX_MAX	4	/* Original signal information. */
	struct sigaction oact[INDX_MAX];

#define	CL_LLINE_IV	0x0001	/* Last line is in inverse video. */
#define	CL_SCR_EX	0x0002	/* Ex initialized and running. */
#define	CL_SCR_EX_INIT	0x0004	/* Vi initialized. */
#define	CL_SCR_VI	0x0008	/* Vi running. */
#define	CL_SCR_VI_INIT	0x0010	/* Vi initialized. */
#define	CL_SIGHUP	0x0020	/* SIGHUP arrived. */
#define	CL_SIGINT	0x0040	/* SIGINT arrived. */
#define	CL_SIGTERM	0x0080	/* SIGTERM arrived. */
#define	CL_SIGWINCH	0x0100	/* SIGWINCH arrived. */
	u_int16_t flags;
} CL_PRIVATE;

#define	CLP(sp)		((CL_PRIVATE *)((sp)->gp->cl_private))
#define	GCLP(gp)	((CL_PRIVATE *)gp->cl_private)

/* Return possibilities from the keyboard read routine. */
typedef enum { INP_OK=0, INP_EOF, INP_ERR, INP_INTR, INP_TIMEOUT } input_t;

/* The screen line relative to a specific window. */
#define	RLNO(sp, lno)	(sp)->woff + (lno)

/* Some functions can be safely ignored until the screen is running. */
#define	VI_INIT_IGNORE(sp)						\
	if (F_ISSET(sp, S_VI) && !F_ISSET(CLP(sp), CL_SCR_VI))		\
		return (0);
#define	EX_INIT_IGNORE(sp)						\
	if (F_ISSET(sp, S_EX) && !F_ISSET(CLP(sp), CL_SCR_EX))		\
		return (0);

#include "cl_extern.h"
