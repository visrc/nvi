/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: exf.h,v 5.9 1992/10/10 13:35:41 bostic Exp $ (Berkeley) $Date: 1992/10/10 13:35:41 $
 */

#ifndef _EXF_H_
#define	_EXF_H_

#include <db.h>

typedef struct exf {
	struct exf *next, *prev;		/* Linked list of files. */

					/* Screen state. */
	recno_t top;			/* 1-N: Physical screen top line. */
	recno_t lno;			/* 1-N:     Physical cursor line. */
	recno_t olno;			/* 0-N: Old physical cursor line. */
	size_t cno;			/* 0-N:     Physical cursor col. */
	size_t ocno;			/* 0-N: Old physical cursor col. */
	size_t scno;			/* 0-N: Logical cursor col. */
	size_t shift;			/* 0-N: Shift count. */
	size_t rcm;			/* 0-N: Column suck. */
#define	RCM_FNB		0x01		/* Column suck: first non-blank. */
#define	RCM_LAST	0x02		/* Column suck: last. */
	u_char rcmflags;
	u_short uwindow;		/* Lines to move window up. */
	u_short dwindow;		/* Lines to move window down. */

	char *name;			/* File name. */
	size_t nlen;			/* File name length. */
	DB *db;				/* File db structure. */
	DB *sdb;			/* Shadow db structure. */

#define	F_CREATED	0x001		/* File was created. */
#define	F_MODIFIED	0x002		/* File was modified. */
#define	F_NAMECHANGED	0x004		/* File name changed. */
#define	F_NONAME	0x008		/* File has no name. */
#define	F_RDONLY	0x010		/* File is read-only. */
#define	F_WRITTEN	0x020		/* File has been written. */
	u_int flags;
} EXF;

extern EXF *curf;			/* Current file. */

enum upmode 
    {LINE_APPEND, LINE_DELETE, LINE_INSERT, LINE_RESET, LINE_SET};

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
	recno_t __lno = (lno);						\
	p = file_gline(curf, __lno, &(len));				\
}

/* Get current line; an error if fail. */
#define	EGETLINE(p, lno, len) {						\
	recno_t __lno = (lno);						\
	p = file_gline(curf, __lno, &(len));				\
	if (p == NULL) {						\
		bell();							\
		msg("Unable to retrieve line %lu.", __lno);		\
		return (1);						\
	}								\
}

/*
 * There's a shadow recno db that parallels underlying file db.  For each
 * line in the file db there are two bytes of information as defined by
 * the following structure.  The shared data structure between the screen
 * routines and the rest of the editor is the file.  Rather than have the
 * screen routines figure out what the rest of the editor already knows,
 * or have the rest of the editor know too much about the screen, information
 * about each line is passed in this parallel structure.
 */
typedef struct {
#define	LINE_WIDECH	0x01			/* Line has wide characters. */
	u1byte_t flags;
#define	MAXTABCNT	255			/* Max representable. */
	u1byte_t tabcnt;			/* Leading tab count. */
} SLINE; 

int	 file_aline __P((EXF *, recno_t, u_char *, size_t));
int	 file_dline __P((EXF *, recno_t));
EXF	*file_first __P((void));
u_char	*file_gline __P((EXF *, recno_t, size_t *));
int	 file_iline __P((EXF *, recno_t, u_char *, size_t));
void	 file_init __P((void));
int	 file_ins __P((EXF *, char *, int));
int	 file_iscurrent __P((char *));
recno_t	 file_lline __P((EXF *));
EXF	*file_next __P((EXF *));
EXF	*file_prev __P((EXF *));
int	 file_set __P((int, char *[]));
int	 file_sline __P((EXF *, recno_t, u_char *, size_t));
int	 file_start __P((EXF *));
int	 file_stop __P((EXF *, int));
int	 file_sync __P((EXF *, int));
#endif /* !_EXF_H_ */
