/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: log.h,v 9.2 1995/01/11 15:58:09 bostic Exp $ (Berkeley) $Date: 1995/01/11 15:58:09 $
 */

#define	LOG_NOTYPE		0
#define	LOG_CURSOR_INIT		1
#define	LOG_CURSOR_END		2
#define	LOG_LINE_APPEND		3
#define	LOG_LINE_DELETE		4
#define	LOG_LINE_INSERT		5
#define	LOG_LINE_RESET_F	6
#define	LOG_LINE_RESET_B	7
#define	LOG_MARK		8

int	log_backward __P((SCR *, MARK *));
int	log_cursor __P((SCR *));
int	log_end __P((SCR *, EXF *));
int	log_forward __P((SCR *, MARK *));
int	log_init __P((SCR *, EXF *));
int	log_line __P((SCR *, recno_t, u_int));
int	log_mark __P((SCR *, LMARK *));
int	log_setline __P((SCR *));
