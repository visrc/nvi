/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: exf.h,v 5.8 1992/10/01 17:30:05 bostic Exp $ (Berkeley) $Date: 1992/10/01 17:30:05 $
 */

#ifndef _EXF_H_
#define	_EXF_H_

#include <db.h>

typedef struct exf {
	struct exf *next, *prev;		/* Linked list of files. */

						/* Screen state. */
	recno_t lno;				/* Cursor line number. */
	recno_t top;				/* Current first window line. */
	size_t cno;				/* Cursor column number. */
	size_t lcno;				/* Left column number. */
	u_short uwindow;			/* Lines to move window up. */
	u_short dwindow;			/* Lines to move window down. */

	char *name;				/* File name. */
	size_t nlen;				/* File name length. */
	DB *db;					/* Underlying db structure. */

#define	F_CREATED	0x001			/* File was created. */
#define	F_MODIFIED	0x002			/* File was modified. */
#define	F_NAMECHANGED	0x004			/* File name changed. */
#define	F_NONAME	0x008			/* File has no name. */
#define	F_RDONLY	0x010			/* File is read-only. */
#define	F_WRITTEN	0x020			/* File has been written. */
	u_int flags;
} EXF;

extern EXF *curf;				/* Current file. */

typedef struct {
	EXF *next, *prev;
} EXFLIST;

/* Remove from the file chain. */
#define	rmexf(p) {							\
        (p)->prev->next = (p)->next;					\
        (p)->next->prev = (p)->prev;					\
}

/* Insert into the file chain after p. */
#define insexf(p, hp) {							\
        (p)->next = (hp)->next;						\
        (p)->prev = (EXF *)(hp);					\
        (hp)->next->prev = (p);						\
        (hp)->next = (p);						\
}

/* Insert into the file chain before p. */
#define instailexf(p, hp) {						\
	(hp)->prev->next = (p);						\
	(p)->prev = (hp)->prev;						\
	(hp)->prev = (p);						\
	(p)->next = (EXF *)(hp);					\
}

/* Get current line. */
#define	GETLINE(p, lno, len) {						\
	u_long __lno = (lno);						\
	p = file_gline(curf, __lno, &(len));				\
}

/* Get current line; an error if fail. */
#define	EGETLINE(p, lno, len) {						\
	u_long __lno = (lno);						\
	p = file_gline(curf, __lno, &(len));				\
	if (p == NULL) {						\
		bell();							\
		msg("Unable to retrieve line %lu.", __lno);		\
		return (1);						\
	}								\
}

int	 file_aline __P((EXF *, recno_t, char *, size_t));
int	 file_dline __P((EXF *, recno_t));
EXF	*file_first __P((void));
char	*file_gline __P((EXF *, recno_t, size_t *));
int	 file_iline __P((EXF *, recno_t, char *, size_t));
void	 file_init __P((void));
int	 file_ins __P((EXF *, char *, int));
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
