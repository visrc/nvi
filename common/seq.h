/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: seq.h,v 10.1 1995/04/13 17:18:37 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:18:37 $
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
struct _seq {
	LIST_ENTRY(_seq) q;		/* Linked list of all sequences. */
	seq_t	 stype;			/* Sequence type. */
	CHAR_T	*name;			/* Sequence name (if any). */
	size_t	 nlen;			/* Name length. */
	CHAR_T	*input;			/* Sequence input keys. */
	size_t	 ilen;			/* Input keys length. */
	CHAR_T	*output;		/* Sequence output keys. */
	size_t	 olen;			/* Output keys length. */

#define	SEQ_FUNCMAP	0x01		/* If unresolved function key.*/
#define	SEQ_NOOVERWRITE	0x02		/* Don't replace existing entry. */
#define	SEQ_SCREEN	0x04		/* If screen specific. */
#define	SEQ_USERDEF	0x08		/* If user defined. */
	u_int8_t flags;
};

void	 seq_close __P((GS *));
int	 seq_delete __P((SCR *, CHAR_T *, size_t, seq_t));
int	 seq_dump __P((SCR *, seq_t, int));
SEQ	*seq_find __P((SCR *, SEQ **, EVENT *, CHAR_T *, size_t, seq_t, int *));
void	 seq_init __P((SCR *));
int	 seq_mdel __P((SEQ *));
int	 seq_save __P((SCR *, FILE *, char *, seq_t));
int	 seq_set __P((SCR *, CHAR_T *, size_t,
	    CHAR_T *, size_t, CHAR_T *, size_t, seq_t, int));
