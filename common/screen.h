/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 5.16 1993/02/19 11:17:42 bostic Exp $ (Berkeley) $Date: 1993/02/19 11:17:42 $
 */

#define	CONTMSG	"Enter return to continue: "

#define	SCREENSIZE(ep)	((ep)->lines - 1)	/* Last screen line. */
#define	TEXTSIZE(ep)	((ep)->lines - 2)	/* Last text line. */

/*
 * Macros for the O_NUMBER format and length, plus macro for the number
 * of columns on a screen.
 */
#define	O_NUMBER_FMT	"%7lu "
#define	O_NUMBER_LENGTH	8
#define	SCREEN_COLS(ep) \
	((ISSET(O_NUMBER) ? (ep)->cols - O_NUMBER_LENGTH : (ep)->cols))

/* Move, and fail if it doesn't work. */
#define	MOVE(ep, lno, cno) {						\
	if (move(lno, cno) == ERR) {					\
		msg(ep, M_ERROR, "Error: %s/%d: move(%u, %u).",		\
		    tail(__FILE__), __LINE__, lno, cno);		\
		return (1);						\
	}								\
}

/*
 * Flags to the screen change routine.  The LINE_LOGICAL flag is OR'd into
 * the other flags if the operation is only logical.   The problem this is
 * meant to solve is that if the operation is logical, not physical, the top
 * line value may change, i.e. EXF->top is changed as part of the update.
 * LINE_LOGICAL is not meaningful for the LINE_RESET flag.
 */
#define	LINE_LOGICAL	0x0001		/* Logical, not physical. */
#define	LINE_APPEND	0x0010		/* Append a line. */
#define	LINE_DELETE	0x0020		/* Delete a line. */
#define	LINE_INSERT	0x0040		/* Insert a line. */
#define	LINE_RESET	0x0080		/* Reset a line. */

/*
 * Structure for mapping lines to the screen.  SMAP is an array of structures,
 * one per screen line, holding a physical line and screen offset into the
 * line.  For example, the pair 2:1 would be the first screen of line 2, and
 * 2:2 would be the second.  If doing left-right scrolling, all of the offsets
 * will be the same, i.e. for the second screen, 1:2, 2:2, 3:2, etc.  If doing
 * the standard, but unbelievably stupid, vi scrolling, it will be staggered,
 * i.e. 1:1, 1:2, 1:3, 2:1, 3:1, etc.
 *
 * It might be reasonable to make SMAP a linked list of structures, instead of
 * an array.  This wins if the top or bottom of the line is scrolling, because
 * you can just move the head and tail pointers instead of copying the array.
 * This loses when you're deleting three lines out of the middle of the map,
 * though, because you have to increment through the structures instead of just
 * doing a memmove.  Not particularly clear which choice is better.
 */
typedef struct smap {
	recno_t lno;			/* File line number. */
	size_t off;			/* Screen line offset in the line. */
} SMAP;

#define	HMAP	((SMAP *)ep->h_smap)	/* Head of the map. */
#define	TMAP	((SMAP *)ep->t_smap)	/* Tail of the map. */

/* General screen functions. */
int	scr_begin __P((EXF *));
int	scr_change __P((EXF *, recno_t, u_int));
int	scr_end __P((EXF *));
int	scr_line __P((EXF *, SMAP *, u_char *, size_t, size_t *, size_t *));
int	scr_modeline __P((EXF *, int));
int	scr_refresh __P((EXF *, size_t));
size_t	scr_relative __P((EXF *, recno_t));
size_t	scr_screens __P((EXF *, recno_t, size_t *));
int	scr_update __P((EXF *));

/* SMAP manipulation functions. */
int	scr_sm_bot __P((EXF *, recno_t *, u_long));
int	scr_sm_delete __P((EXF *, recno_t, int));
int	scr_sm_1down __P((EXF *));
int	scr_sm_init __P((EXF *));
int	scr_sm_insert __P((EXF *, recno_t, int));
int	scr_sm_mid __P((EXF *, recno_t *));
int	scr_sm_next __P((EXF *, SMAP *, SMAP *));
recno_t	scr_sm_nlines __P((EXF *, SMAP *, recno_t, size_t));
int	scr_sm_prev __P((EXF *, SMAP *, SMAP *));
int	scr_sm_top __P((EXF *, recno_t *, u_long));
int	scr_sm_1up __P((EXF *));

enum position { P_TOP, P_MIDDLE, P_BOTTOM };
int	scr_sm_fill __P((EXF *, recno_t, enum position));

#ifdef DEBUG
void	scr_sm_dmap __P((EXF *, char *));
#endif
