/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 5.6 1992/10/20 18:24:28 bostic Exp $ (Berkeley) $Date: 1992/10/20 18:24:28 $
 */

#define	BOTLINE		(curf->top + LINES - 2)
#define	HALFSCREEN	((LINES - 1) / 2)
#define	SCREENSIZE	(LINES - 1)
#define	COLSIZE		(ISSET(O_NUMBER) ? COLS - 9 : COLS - 1)

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
void	scr_modeline __P((EXF *, int));
void	scr_ref __P((void));
size_t	scr_relative __P((EXF *, recno_t));

/*
 * Flags to scr_update.  The LINE_LOGICAL flag is OR'd into the other flags
 * if the operation is only logical.   The problem this is meant to solve is
 * that if the operation is logical, not physical, the top line value may
 * change, i.e. curf->top is changed as part of the update.  LINE_LOGICAL is
 * not meaningful for the LINE_RESET flag.
 */
#define	LINE_LOGICAL	0x001		/* Logical, not physical. */
#define	LINE_APPEND	0x010		/* Append a line. */
#define	LINE_DELETE	0x020		/* Delete a line. */
#define	LINE_INSERT	0x040		/* Insert a line. */
#define	LINE_RESET	0x080		/* Reset a line. */
void	scr_update __P((EXF *, recno_t, u_char *, size_t, u_int));
