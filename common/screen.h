/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 5.22 1993/03/25 15:01:00 bostic Exp $ (Berkeley) $Date: 1993/03/25 15:01:00 $
 */

/*
 * There are various minimum values that vi has to have to display a
 * screen.  These are about the MINIMUM that you can have.  Changing
 * these is not a good idea.
 */
#define	MINIMUM_SCREEN_ROWS	 4		/* XXX Should be 2. */
#define	MINIMUM_SCREEN_COLS	20

/* Standard continue message. */
#define	CONTMSG	"Enter return to continue: "

#define	SCREENSIZE(sp)	((sp)->lines - 1)	/* Last screen line. */
#define	TEXTSIZE(sp)	((sp)->lines - 2)	/* Last text line. */

/*
 * Macros for the O_NUMBER format and length, plus macro for the number
 * of columns on a screen.
 */
#define	O_NUMBER_FMT	"%7lu "
#define	O_NUMBER_LENGTH	8
#define	SCREEN_COLS(sp) \
	((ISSET(O_NUMBER) ? (sp)->cols - O_NUMBER_LENGTH : (sp)->cols))

/* Move, and fail if it doesn't work. */
#define	MOVE(sp, lno, cno) {						\
	if (move(lno, cno) == ERR) {					\
		msgq(sp, M_ERROR, "Error: %s/%d: move(%u, %u).",	\
		    tail(__FILE__), __LINE__, lno, cno);		\
		return (1);						\
	}								\
}
