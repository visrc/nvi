/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 5.12 1993/01/24 18:37:30 bostic Exp $ (Berkeley) $Date: 1993/01/24 18:37:30 $
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

/*
 * Macros for the O_NUMBER format and length, plus macro for the number
 * of columns on a screen.
 */
#define	O_NUMBER_FMT	"%7lu "
#define	O_NUMBER_LENGTH	8
#define	SCREEN_COLS(ep) \
	((ISSET(O_NUMBER) ? (ep)->cols - O_NUMBER_LENGTH : (ep)->cols))

/* Move, and fail if it doesn't work. */
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
 * Flags to the screen change routine.  The LINE_LOGICAL flag is OR'd into
 * the other flags if the operation is only logical.   The problem this is
 * meant to solve is that if the operation is logical, not physical, the top
 * line value may change, i.e. curf->top is changed as part of the update.
 * LINE_LOGICAL is not meaningful for the LINE_RESET flag.
 */
#define	LINE_LOGICAL	0x001		/* Logical, not physical. */
#define	LINE_APPEND	0x010		/* Append a line. */
#define	LINE_DELETE	0x020		/* Delete a line. */
#define	LINE_INSERT	0x040		/* Insert a line. */
#define	LINE_RESET	0x080		/* Reset a line. */

int	scr_end __P((EXF *));
int	scr_init __P((EXF *));
int	scr_modeline __P((EXF *, int));
