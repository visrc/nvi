/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: mark.h,v 5.8 1993/05/08 16:06:56 bostic Exp $ (Berkeley) $Date: 1993/05/08 16:06:56 $
 */

/*
 * The MARK structure defines a position in the file.  Because of the different
 * interfaces used by the db(3) package, curses, and users, the line number is
 * 1 based, while the column number is 0 based.  Additionally, it is known that
 * the out-of-band line number is less than any legal line number.  The line
 * number is of type recno_t, as that's the underlying type of the database.
 * The column number is of type size_t so that we can malloc a line.
 */
typedef struct _mark {
#define	OOBLNO		0			/* Out-of-band line number. */
	recno_t lno;				/* Line number. */
	size_t cno;				/* Column number. */
} MARK;

/* Mark routines. */
void	mark_delete __P((struct _scr *,
	    struct _exf *, struct _mark *, struct _mark *, int));
struct _mark *
	mark_get __P((struct _scr *, struct _exf *, int));
int	mark_init __P((struct _scr *, struct _exf *));
void	mark_insert __P((struct _scr *,
	    struct _exf *, struct _mark *, struct _mark *));
int	mark_set __P((struct _scr *, struct _exf *, int, struct _mark *));
