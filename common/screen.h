/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: screen.h,v 5.20 1993/02/28 14:01:42 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:01:42 $
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
	recno_t lno;		/* 1-N: Physical (file) line number. */
	size_t off;		/* 1-N: Screen (logical) offset in the line. */
} SMAP;

				/* Reference the screen structure. */
#define	SCRP(ep)	((SCR *)((ep)->scrp))

/* Structure to represent a screen. */
typedef struct scr {
	recno_t lno;		/* 1-N:     Cursor screen line. */
	recno_t olno;		/* 1-N: Old cursor screen line. */
	size_t cno;		/* 0-N:     Physical cursor column. */
	size_t ocno;		/* 0-N: Old physical cursor column. */
	size_t scno;		/* 0-N: Logical screen cursor column. */

	size_t lines;		/* Physical number of screen lines. */
	size_t cols;		/* Physical number of screen cols. */

	SMAP *h_smap;		/* Head of map of lines to the screen. */
	SMAP *t_smap;		/* Tail of map of lines to the screen. */

	size_t rcm;		/* 0-N: Column suck. */
#define	RCM_FNB		0x01	/* Column suck: first non-blank. */
#define	RCM_LAST	0x02	/* Column suck: last. */
	u_int rcmflags;

#define	S_CHARDELETED	0x01	/* Character deleted. */
#define	S_REDRAW	0x02	/* Redraw the screen. */
#define	S_REFORMAT	0x04	/* Reformat the lines. */
#define	S_REFRESH	0x08	/* Refresh the screen. */
#define	S_RESIZE	0x10	/* Resize the screen. */

#define	SF_SET(ep, f)	(SCRP(ep))->flags |= (f)
#define	SF_CLR(ep, f)	(SCRP(ep))->flags &= ~(f)
#define	SF_ISSET(sp, f)	((SCRP(ep))->flags & (f))
	u_int flags;
} SCR;

/*
 * The screen structure is a void * in the EXF structure to make the
 * include files simpler.
 */
#define	SCRLNO(ep)	(SCRP(ep)->lno)		/* Current line number. */
#define	SCRCNO(ep)	(SCRP(ep)->cno)		/* Current column number. */
#define	SCRCOL(ep)	(SCRP(ep)->cols)	/* Current column count. */

#define	SCREENSIZE(ep)	(SCRP(ep)->lines - 1)	/* Last screen line. */
#define	TEXTSIZE(ep)	(SCRP(ep)->lines - 2)	/* Last text line. */

/*
 * Macros for the O_NUMBER format and length, plus macro for the number
 * of columns on a screen.
 */
#define	O_NUMBER_FMT	"%7lu "
#define	O_NUMBER_LENGTH	8
#define	SCREEN_COLS(ep) \
	((ISSET(O_NUMBER) ? SCRCOL(ep) - O_NUMBER_LENGTH : SCRCOL(ep)))

/* Move, and fail if it doesn't work. */
#define	MOVE(ep, lno, cno) {						\
	if (move(lno, cno) == ERR) {					\
		ep->msg(ep, M_ERROR, "Error: %s/%d: move(%u, %u).",	\
		    tail(__FILE__), __LINE__, lno, cno);		\
		return (1);						\
	}								\
}

/* General screen functions. */
int	scr_begin __P((EXF *));
void	scr_def __P((SCR *));
int	scr_end __P((EXF *));
size_t	scr_lrelative __P((EXF *, recno_t, size_t));
int	scr_modeline __P((EXF *, int));
int	scr_refresh __P((EXF *, size_t));
size_t	scr_relative __P((EXF *, recno_t));
int	scr_sm_bot __P((EXF *, recno_t *, u_long));
int	scr_sm_down __P((EXF *, MARK *, recno_t, int));
int	scr_sm_mid __P((EXF *, recno_t *));
int	scr_sm_top __P((EXF *, recno_t *, u_long));
int	scr_sm_up __P((EXF *, MARK *, recno_t, int));
int	scr_update __P((EXF *));

enum operation { LINE_APPEND, LINE_DELETE, LINE_INSERT, LINE_RESET };
int	scr_change __P((EXF *, recno_t, enum operation));
enum position { P_TOP, P_FILL, P_MIDDLE, P_BOTTOM };
int	scr_sm_fill __P((EXF *, recno_t, enum position));

/* The private vi screen functions. */
#ifdef SMAP_PRIVATE
size_t	scr_screens __P((EXF *, recno_t, size_t *));
int	scr_line __P((EXF *, SMAP *, u_char *, size_t, size_t *, size_t *));
int	scr_sm_1down __P((EXF *));
int	scr_sm_1up __P((EXF *));
int	scr_sm_delete __P((EXF *, recno_t));
int	scr_sm_insert __P((EXF *, recno_t));
int	scr_sm_next __P((EXF *, SMAP *, SMAP *));
recno_t	scr_sm_nlines __P((EXF *, SMAP *, recno_t, size_t));
int	scr_sm_prev __P((EXF *, SMAP *, SMAP *));
#endif

#ifdef DEBUG
void	scr_sm_dmap __P((EXF *, char *));
#endif
