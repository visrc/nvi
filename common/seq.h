/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: seq.h,v 8.8 1994/03/07 16:53:54 bostic Exp $ (Berkeley) $Date: 1994/03/07 16:53:54 $
 */

/*
 * Map and abbreviation structures.
 *
 * The map structure is doubly linked list, sorted by input string and by
 * input length within the string.  (The latter is necessary so that short
 * matches will happen before long matches when the list is searched.)
 * Additionally, there is a bitmap which has bits set if there are entries
 * starting with the corresponding character.  This keeps us from walking
 * the list unless it's necessary.
 *
 * The name and the output fields of a SEQ can be empty, i.e. NULL.
 * Only the input field is required.
 *
 * XXX
 * The fast-lookup bits are never turned off -- users don't usually unmap
 * things, though, so it's probably not a big deal.
 */
					/* Sequence type. */
enum seqtype { SEQ_ABBREV, SEQ_COMMAND, SEQ_INPUT };

struct _seq {
	LIST_ENTRY(_seq) q;		/* Linked list of all sequences. */
	enum seqtype stype;		/* Sequence type. */
	char	*name;			/* Sequence name (if any). */
	size_t	 nlen;			/* Name length. */
	char	*input;			/* Sequence input keys. */
	size_t	 ilen;			/* Input keys length. */
	char	*output;		/* Sequence output keys. */
	size_t	 olen;			/* Output keys length. */

#define	S_USERDEF	0x01		/* If sequence user defined. */
	u_char	 flags;
};

int	 abbr_save __P((SCR *, FILE *));
int	 map_save __P((SCR *, FILE *));
int	 seq_delete __P((SCR *, char *, size_t, enum seqtype));
int	 seq_dump __P((SCR *, enum seqtype, int));
SEQ	*seq_find __P((SCR *, SEQ **, char *, size_t, enum seqtype, int *));
void	 seq_init __P((SCR *));
int	 seq_save __P((SCR *, FILE *, char *, enum seqtype));
int	 seq_set __P((SCR *, char *, size_t,
	    char *, size_t, char *, size_t, enum seqtype, int));
