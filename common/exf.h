/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: exf.h,v 5.1 1992/05/03 08:17:09 bostic Exp $ (Berkeley) $Date: 1992/05/03 08:17:09 $
 */

typedef struct exf {
	struct exf *next, *prev;		/* Linked list of files. */
	u_long lno;				/* Last line number. */
	u_long cno;				/* Last column number. */
	MARK pos;				/* Last position in file. */
	char *name;				/* File name. */

#define	F_MODIFIED	0x001			/* File has been modified. */
#define	F_NONAME	0x002			/* File has no name. */
#define	F_RDONLY	0x004			/* File is read-only. */
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

/* Insert into the file chain at the tail. */
#define instailexf(p, hp) { \
	(hp)->prev->next = (p); \
	(p)->prev = (hp)->prev; \
	(hp)->prev = (p); \
	(p)->next = (EXF *)(hp); \
}

int	 file_default __P((void));
EXF	*file_first __P((void));
void	 file_init __P((void));
int	 file_ins __P((char *));
int	 file_iscurrent __P((char *));
EXF	*file_next __P((EXF *));
EXF	*file_prev __P((EXF *));
int	 file_set __P((int, char *[]));
int	 file_start __P((EXF *));
