/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: mark.h,v 5.5 1993/02/16 20:16:22 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:16:22 $
 */

/*
 * The MARK structure defines a position in the file.  Because of the different
 * interfaces used by the db(3) package, curses, and users, the line number is
 * 1 based, while the column number is 0 based.  Additionally, it is known that
 * the out-of-band line number is less than any legal line number.  The line
 * number is of type recno_t, as that's the underlying type of the database.
 * The column number is of type size_t so that we can malloc a line.
 */
typedef struct {
#define	OOBLNO		0			/* Out-of-band line number. */
	recno_t lno;				/* Line number. */
	size_t cno;				/* Column number. */
} MARK;

/* Set absolute movement mark. */
#define	SETABSMARK(ep, mp)	mark_set(ep, '\'', mp)
