/*-
 * Copyright (c) 1996
 *	Rob Zimmermann.  All rights reserved.
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	"$Id: m_motif.h,v 8.6 1996/12/11 20:58:24 bostic Exp $ (Berkeley) $Date: 1996/12/11 20:58:24 $";
 */

#if XtSpecificationRelease == 4
#define	ArgcType	Cardinal *
#else
#define	ArgcType	int *
#endif

/* Describes a single 'screen' implemented in X widgetry. */
typedef	struct {
    Widget	parent,		/* the pane */
		area,		/* text goes here */
		form,		/* holds text and scrollbar */
		scroll;		/* not connected yet */
    Region	clip;
    int		color;
    int		rows,
		cols;
    int		ch_width,
		ch_height,
		ch_descent;
    int		curx, cury;
    char	*characters;
    char	*flags;
    Boolean	init;
} xvi_screen;

/*
 * Color support
 */
#define	COLOR_INVALID	0xff	/* force color change */

/*
 * These are color indices.  When vi passes color info, we can do 2..0x3f
 * in the 8 bits I've allocated.
 */
#define	COLOR_STANDARD	0x00	/* standard video */
#define	COLOR_INVERSE	0x01	/* reverse video */

/* These are flag bits, they override the above colors. */
#define	COLOR_CARET	0x80	/* draw the caret */
#define	COLOR_SELECT	0x40	/* draw the selection */

#define	ToRowCol( scr, lin, r, c )	\
	    r = (lin) / scr->cols;	\
	    c = ((lin) - r * (scr->cols)) % scr->cols;
#define	Linear( scr, y, x )	\
	    ( (y) * scr->cols + (x) )
#define	CharAt( scr, y, x )	\
	    ( scr->characters + Linear( scr, y, x ) )
#define	FlagAt( scr, y, x )	\
	    ( scr->flags + Linear( scr, y, x ) )

#define	XPOS( scr, x )	\
	scr->ch_width * (x)
#define	YTOP( scr, y )	\
	scr->ch_height * (y)
#define	YPOS( scr, y )	\
	YTOP( scr, ((y)+1) ) - scr->ch_descent

#define	ROW( scr, y )	\
	( (y) / scr->ch_height )

#define	COLUMN( scr, x )	\
	( (x) / scr->ch_width )

/* Internal use: */
extern GC	   __vi_copy_gc;
extern void	 (*__vi_exitp) __P((void));
extern xvi_screen *__vi_screen;
