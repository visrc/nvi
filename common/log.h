/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: log.h,v 5.5 1993/05/10 11:14:17 bostic Exp $ (Berkeley) $Date: 1993/05/10 11:14:17 $
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

int	log_backward __P((struct _scr *, struct _exf *, struct _mark *));
int	log_cursor __P((struct _scr *, struct _exf *));
int	log_end __P((struct _scr *, struct _exf *));
int	log_forward __P((struct _scr *, struct _exf *, struct _mark *));
int	log_init __P((struct _scr *, struct _exf *));
int	log_line __P((struct _scr *, struct _exf *, recno_t, u_int));
int	log_mark __P((struct _scr *, struct _exf *, u_int, struct _mark *));
int	log_setline __P((struct _scr *, struct _exf *));
