/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: exf.h,v 5.4 1992/05/15 10:58:45 bostic Exp $ (Berkeley) $Date: 1992/05/15 10:58:45 $
 */

#ifndef _EXF_H_
#define	_EXF_H_

#include <db.h>

/*
 * XXX
 * Old flags...
 */
#define NEWFILE		0x0001	/* the file was just created */
#define READONLY	0x0002	/* the file is read-only */
#define HADNUL		0x0004	/* the file contained NUL characters */
#define MODIFIED	0x0008	/* the file has been modified */
#define NONAME		0x0010	/* no name is known for the current text */
#define ADDEDNL		0x0020	/* newlines were added to the file */
#define HADBS		0x0040	/* backspace chars were lost from the file */

typedef struct exf {
	struct exf *next, *prev;		/* Linked list of files. */
	char *name;				/* File name. */
	size_t nlen;				/* File name length. */
	DB *db;					/* Underlying db structure. */

#ifdef INTENDED_FOR_THE_FUTURE
	WINDOW *win;				/* Window structure. */
	u_long lines;				/* Lines in window. */
#endif
	recno_t lno;				/* Cursor line number. */
	size_t cno;				/* Cursor column number. */
	recno_t ntop;				/* New first window line. */
	recno_t otop;				/* Current first window line. */

#define	F_MODIFIED	0x001			/* File has been modified. */
#define	F_NAMECHANGED	0x002			/* File name changed. */
#define	F_NONAME	0x004			/* File has no name. */
#define	F_RDONLY	0x008			/* File is read-only. */
	u_int flags;
} EXF;

extern EXF *curf;				/* Current file. */

typedef struct {
	EXF *next, *prev;
} EXFLIST;

/* Remove from the file chain. */
#define	rmexf(p) { \
        (p)->prev->next = (p)->next; \
        (p)->next->prev = (p)->prev; \
}

/* Insert into the file chain after p. */
#define insexf(p, hp) { \
        (p)->next = (hp)->next; \
        (p)->prev = (EXF *)(hp); \
        (hp)->next->prev = (p); \
        (hp)->next = (p); \
}

/* Insert into the file chain before p. */
#define instailexf(p, hp) { \
	(hp)->prev->next = (p); \
	(p)->prev = (hp)->prev; \
	(hp)->prev = (p); \
	(p)->next = (EXF *)(hp); \
}

/*
 * The MARK structure defines a position in the file.  Because of the
 * different interfaces used by the db(3) package, curses, and users,
 * the line number is 1 based, while the column number is 0 based.  The
 * line number is of type recno_t, because that's the underlying type
 * of the database.  The column number is of type size_t so that we can
 * malloc a line.
 */
typedef struct {
#define	OOBLNO		0			/* Out-of-band line number. */
	recno_t lno;				/* Line number. */
	size_t cno;				/* Column number. */
} MARK;

extern MARK cursor;				/* Cursor MARK. */

int	 file_default __P((void));
EXF	*file_first __P((void));
char	*file_gline __P((EXF *, recno_t, size_t *));
void	 file_init __P((void));
int	 file_ins __P((EXF *, char *));
int	 file_iscurrent __P((char *));
u_long	 file_lline __P((EXF *));
EXF	*file_next __P((EXF *));
EXF	*file_prev __P((EXF *));
int	 file_set __P((int, char *[]));
int	 file_sline __P((EXF *, recno_t, char *, size_t));
int	 file_start __P((EXF *));
int	 file_stop __P((EXF *, int));
int	 file_sync __P((EXF *, int));
#endif /* !_EXF_H_ */
