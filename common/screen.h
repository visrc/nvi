/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 5.10 1992/12/25 16:24:03 bostic Exp $ (Berkeley) $Date: 1992/12/25 16:24:03 $
 */

/*
 * The basic screen macros.
 *
 * BOTLINE:
 *	The line number at the bottom of the screen, not counting the mode
 *	line, or, if `l' == 0, the number of text lines on the screen.
 * SCREENSIZE:
 *	The number of lines in the screen.
 * HALFSCREEN:
 *	Half the number of lines in the screen.
 */
#define	BOTLINE(ep, l)	((l) + (ep)->lines - 2)
#define	SCREENSIZE(ep)	((ep)->lines - 1)
#define	HALFSCREEN(ep)	(SCREENSIZE(ep) / 2)

#define	MOVE(lno, cno) {						\
	if (move(lno, cno) == ERR) {					\
		bell();							\
		msg("Error: %s/%d: move(%u, %u).",			\
		    tail(__FILE__), __LINE__, lno, cno);		\
		return (1);						\
	}								\
}

#define	CONTMSG	"Enter return to continue: "

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
 *	scr_relative
 *		Return the physical column that would display closest to
 *		a specified character position.
 *	scr_update
 *		Inform the screen a line has been modified.
 */
int	scr_cchange __P((EXF *));
int	scr_end __P((EXF *));
int	scr_init __P((EXF *));
int	scr_modeline __P((EXF *, int));
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
