/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: mark.h,v 8.4 1993/11/18 08:17:09 bostic Exp $ (Berkeley) $Date: 1993/11/18 08:17:09 $
 */

/*
 * The MARK structure defines a position in the file.  Because of the different
 * interfaces used by the db(3) package, curses, and users, the line number is
 * 1 based, while the column number is 0 based.  Additionally, it is known that
 * the out-of-band line number is less than any legal line number.  The line
 * number is of type recno_t, as that's the underlying type of the database.
 * The column number is of type size_t, guaranteeing that we can malloc a line.
 */
struct _mark {
	LIST_ENTRY(_mark) q;		/* Linked list of marks. */
#define	OOBLNO		0		/* Out-of-band line number. */
	recno_t	lno;			/* Line number. */
	size_t	cno;			/* Column number. */
	CHAR_T	name;			/* Mark name. */

#define	MARK_DELETED	0x01		/* Mark was deleted. */
#define	MARK_USERSET	0x02		/* User set this mark. */
	u_char	flags;
};

#define	ABSMARK1	'\''		/* Absolute mark name. */
#define	ABSMARK2	'`'		/* Absolute mark name. */

/* Mark routines. */
void	 mark_delete __P((SCR *, EXF *, recno_t));
int	 mark_end __P((SCR *, EXF *));
MARK	*mark_get __P((SCR *, EXF *, ARG_CHAR_T));
int	 mark_init __P((SCR *, EXF *));
void	 mark_insert __P((SCR *, EXF *, recno_t));
int	 mark_set __P((SCR *, EXF *, ARG_CHAR_T, MARK *, int));
