/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: exf.h,v 5.2 1992/05/04 11:50:01 bostic Exp $ (Berkeley) $Date: 1992/05/04 11:50:01 $
 */

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
	DB *db;					/* Underlying db structure. */
	u_long lno;				/* Last line number. */
	u_long cno;				/* Last column number. */
	u_long nlines;				/* Number of lines. */
	MARK pos;				/* XXX Last position in file. */
	char *name;				/* File name. */
	size_t nlen;				/* File name length. */
	dev_t dev;				/* File device. */
	ino_t ino;				/* File inode. */

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

int	 file_default __P((void));
EXF	*file_first __P((void));
void	 file_init __P((void));
int	 file_ins __P((EXF *, char *));
int	 file_iscurrent __P((char *));
char	*file_line __P((EXF *, u_long, size_t *));
EXF	*file_next __P((EXF *));
EXF	*file_prev __P((EXF *));
int	 file_set __P((int, char *[]));
int	 file_start __P((EXF *));
int	 file_stop __P((EXF *, int));
int	 file_sync __P((EXF *, int));
