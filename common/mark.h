/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: mark.h,v 8.2 1993/09/27 16:21:51 bostic Exp $ (Berkeley) $Date: 1993/09/27 16:21:51 $
 */

/*
 * The MARK structure defines a position in the file.  Because of the different
 * interfaces used by the db(3) package, curses, and users, the line number is
 * 1 based, while the column number is 0 based.  Additionally, it is known that
 * the out-of-band line number is less than any legal line number.  The line
 * number is of type recno_t, as that's the underlying type of the database.
 * The column number is of type size_t, guaranteeing that we can malloc a line.
 */
typedef struct _mark {
	struct _mark	*next, *prev;		/* Linked list of marks. */
#define	OOBLNO		0			/* Out-of-band line number. */
	recno_t	lno;				/* Line number. */
	size_t	cno;				/* Column number. */
	CHAR_T	name;				/* Mark name. */

#define	MARK_DELETED	0x01			/* Mark was deleted. */
#define	MARK_USERSET	0x02			/* User set this mark. */
	u_char	flags;
} MARK;

#define	ABSMARK1	'\''			/* Absolute mark name. */
#define	ABSMARK2	'`'			/* Absolute mark name. */

/* Mark routines. */
void	 mark_delete __P((struct _scr *, struct _exf *, recno_t));
MARK	*mark_get __P((struct _scr *, struct _exf *, ARG_CHAR_T));
int	 mark_init __P((struct _scr *, struct _exf *));
void	 mark_insert __P((struct _scr *, struct _exf *, recno_t));
int	 mark_set __P((struct _scr *, struct _exf *, ARG_CHAR_T, MARK *, int));
