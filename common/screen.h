/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 5.1 1992/06/08 09:27:55 bostic Exp $ (Berkeley) $Date: 1992/06/08 09:27:55 $
 */

#define	BOTLINE	(curf->top + LINES - 2)

/* 
 * The interface to the screen is defined by six functions:
 *
 *	scr_init
 *		Set up curses, initialize the screen.
 *	scr_end
 *		End the screen.
 *
 *	scr_cchange:
 *		Change the screen as necessary for a cursor movement and
 *		move to the position.
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
void	scr_modeline __P((void));
void	scr_ref __P((void));
