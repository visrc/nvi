/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: seq.h,v 5.10 1993/03/26 13:37:58 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:37:58 $
 */

/*
 * Map and abbreviation structures.
 *
 * The map structure is an UCHAR_MAX size array of SEQ pointers which are
 * NULL or valid depending if the offset key begins any map or abbreviation
 * sequences.  If the pointer is valid, it references a doubly linked list
 * of SEQ pointers, threaded through the snext and sprev pointers.  This is
 * based on the belief that most normal characters won't start sequences so
 * lookup will be fast on non-mapped characters.  Only a single pointer is
 * maintained to keep the overhead of the map array fairly small, so the first
 * element of the linked list has a NULL sprev pointer and the last element
 * of the list has a NULL snext pointer.  The structures in this list are
 * ordered by length, shortest to longest.  This is so that short matches 
 * will happen before long matches when the list is searched.
 *
 * In addition, each SEQ structure is on another doubly linked list of SEQ
 * pointers, threaded through the next and prev pointers.  This is a list
 * of all of the sequences.  This list is used by the routines that display
 * all of the sequences to the screen or write them to a file.
 */
					/* Sequence type. */
enum seqtype { SEQ_ABBREV, SEQ_COMMAND, SEQ_INPUT };

typedef struct _seq {
	struct _seq *next, *prev;	/* Linked list of all sequences. */
	struct _seq *snext, *sprev;	/* Linked list of ch sequences. */
	enum seqtype stype;		/* Sequence type. */
	u_char	*name;			/* Name of the sequence, if any. */
	u_char	*input;			/* Input key sequence. */
	u_char	*output;		/* Output key sequence. */
	int	 ilen;			/* Input key sequence length. */

#define	S_USERDEF	0x01		/* If sequence user defined. */
	u_char	 flags;
} SEQ;

int	 abbr_save __P((struct _scr *, FILE *));
int	 map_save __P((struct _scr *, FILE *));

int	 seq_delete __P((struct _scr *, u_char *, enum seqtype));
int	 seq_dump __P((struct _scr *, enum seqtype, int));
struct _seq *
	 seq_find __P((struct _scr *, u_char *, size_t, enum seqtype, int *));
void	 seq_init __P((struct _scr *));
int	 seq_save __P((struct _scr *, FILE *, u_char *, enum seqtype));
int	 seq_set __P((struct _scr *,
	    u_char *, u_char *, u_char *, enum seqtype, int));
