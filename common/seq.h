/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: seq.h,v 5.6 1992/05/07 12:47:31 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:47:31 $
 */

/*
 * Map and abbreviation structures.
 *
 * The map structure is an UCHAR_MAX size array of SEQ pointers which are
 * NULL or valid depending if the offset key begins any map or abbreviation
 * sequences.  If the pointer is valid, it references a doubly linked list
 * of SEQ pointers, threaded through the next and prev pointers.  This is
 * based on the belief that most normal characters won't start sequences so
 * lookup will be fast on non-mapped characters.  Only a single pointer is
 * maintained to keep the overhead of the map array fairly small, so the first
 * element of the linked list has a NULL prev pointer and the last element
 * of the list has a NULL next pointer.  The structures in this list are
 * ordered by length, shortest to longest.  This is so that short matches 
 * will happen before long matches when the list is searched.
 *
 * In addition, each SEQ structure is on another doubly linked list of SEQ
 * pointers, threaded through the lnext and lprev pointers.  This is a list
 * of all of the sequences, headed by the mhead SEQLIST structure.  This list
 * is used by the routines that display all of the sequences to the screen
 * or write them to a file.
 */
					/* Sequence type. */
enum seqtype { ABBREV, COMMAND, INPUT };

typedef struct _seq {
	struct _seq *lnext, *lprev;	/* Linked list of all sequences. */
	struct _seq *next, *prev;	/* Linked list of ch sequences. */
	char *name;			/* Name of the sequence, if any. */
	char *input;			/* Input key sequence. */
	char *output;			/* Output key sequence. */
	int ilen;			/* Input key sequence length. */
	enum seqtype stype;		/* Sequence type. */

#define	S_USERDEF	0x01		/* If sequence user defined. */
	u_char flags;
} SEQ;
extern SEQ *seq[UCHAR_MAX];		/* Linked list of ch sequences. */

typedef struct {
	SEQ *lnext, *lprev;
} SEQLIST;
extern SEQLIST seqhead;			/* Linked list of all sequences. */

					/* If need to check abbreviations. */
#define	chkabbr(ch)	(have_abbr && !inword(ch))
#define	seqstart(ch)	(seq[(ch)])	/* If ch starts any sequences. */

#define	rmseq(p) { \
	(p)->lprev->lnext = (p)->lnext; \
	(p)->lnext->lprev = (p)->lprev; \
}

#define inseq(p, hp) { \
	(p)->lnext = (hp)->lnext; \
        (p)->lprev = (SEQ *)(hp); \
        (hp)->lnext->lprev = (p); \
        (hp)->lnext = (p); \
}

extern int have_abbr;			/* If any abbreviations. */
int	 abbr_save __P((FILE *));

int	 map_save __P((FILE *));

int	 seq_delete __P((char *, enum seqtype));
int	 seq_dump __P((enum seqtype, int));
SEQ	*seq_find __P((char *, size_t, enum seqtype, int *));
void	 seq_init __P((void));
int	 seq_save __P((FILE *, char *, enum seqtype));
int	 seq_set __P((char *, char *, char *, enum seqtype, int));
