/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 5.3 1992/10/01 17:30:39 bostic Exp $ (Berkeley) $Date: 1992/10/01 17:30:39 $
 */

#define	BOTLINE		(curf->top + LINES - 2)
#define	HALFSCREEN	((LINES - 1) / 2)
#define	SCREENSIZE	(LINES - 1)

/* 
 * The interface to the screen is defined by the following functions:
 *
 *	scr_init
 *		Set up curses, initialize the screen.
 *	scr_end
 *		End the screen.
 *
 *	scr_cchange:
 *		Change the screen as necessary for a cursor movement and
 *		move to the position.
 *	scr_ldelete:
 *		Delete a line from the screen.
 *	scr_linsert:
 *		Insert a line into the screen.
 *	scr_lchange
 *		Change the screen as necessary for a line change.
 *	scr_modeline
 *		Refresh the screen info line.
 *	scr_ref:
 *		Repaint the entire screen.
 */
void	scr_cchange __P((void));
void	scr_end __P((void));
int	scr_init __P((void));
void	scr_lchange __P((u_long, char *, size_t));
void	scr_ldelete __P((recno_t));
void	scr_linsert __P((recno_t, char *, size_t));
void	scr_modeline __P((void));
void	scr_ref __P((void));
