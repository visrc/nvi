/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 5.8 1992/10/26 17:44:50 bostic Exp $ (Berkeley) $Date: 1992/10/26 17:44:50 $
 */

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
 *
 * We use a single curses window for vi.  The model would be simpler with
 * two windows (one for the text, and one for the modeline) because scrolling
 * the text window down would work correctly, then, not affecting the mode
 * line.  As it is we have to play games to make it look right.  The reasons
 * we don't use two windows are three-fold.  First, the ex code doesn't want
 * to deal with curses.  With two windows vi would have a different method
 * of putting out ex messages than ex would.  As it is now, we just switch
 * between methods when we cross into the ex code and ex can simply use
 * printf and friends without regard for vi.  Second, it would be difficult
 * for curses to optimize the movement, i.e.  detect that the down scroll
 * isn't going to change the modeline, set the scrolling region on the
 * terminal and only scroll the first part of the text window.  Third, even
 * if curses did detect it, the set-scrolling-region terminal commands can't
 * be used by curses because it's indeterminate where the cursor ends up
 * after they are sent.
 */

extern int needexerase;

#define	BOTLINE(ep, l)	((l) + (ep)->lines - 2)
#define	COLSIZE(ep)	(ISSET(O_NUMBER) ? (ep)->cols - 9 : (ep)->cols - 1)
#define	HALFSCREEN(ep)	(((ep)->lines - 1) / 2)
#define	SCREENSIZE(ep)	((ep)->lines - 1)

#define	MOVE(lno, cno) {						\
	if (move(lno, cno) == ERR) {					\
		bell();							\
		msg("Error: %s/%d: move(%u, %u).",			\
		    tail(__FILE__), __LINE__, lno, cno);		\
		return (1);						\
	}								\
}

int	scr_cchange __P((EXF *));
int	scr_end __P((EXF *));
int	scr_init __P((EXF *));
int	scr_modeline __P((EXF *, int));
int	scr_ref __P((EXF *));
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
int	scr_update __P((EXF *, recno_t, u_char *, size_t, u_int));
