/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 5.4 1992/10/10 13:53:03 bostic Exp $ (Berkeley) $Date: 1992/10/10 13:53:03 $
 */

#define	BOTLINE		(curf->top + LINES - 2)
#define	HALFSCREEN	((LINES - 1) / 2)
#define	SCREENSIZE	(LINES - 1)

/* 
 * The interface to the screen is defined by the following functions:
 *
 *	scr_cchange
 *		Change the screen for a cursor movement.
 *	scr_end
 *		Close down curses, end the screen.
 *	scr_init
 *		Set up curses, initialize the screen.
 *	scr_modeline
 *		Display the modeline (ruler, mode).
 *	scr_ref
 *		Repaint the screen from scratch.
 *	scr_relative
 *		Return the physical column that would display closest to
 *		a specified character position.
 *	scr_update
 *		Inform the screen a line has been modified.
 */
void	scr_cchange __P((EXF *));
void	scr_end __P((void));
int	scr_init __P((void));
void	scr_modeline __P((EXF *));
void	scr_ref __P((void));
size_t	scr_relative __P((EXF *, recno_t));
void	scr_update __P((EXF *, recno_t, u_char *, size_t, enum upmode));
