/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: seq.h,v 5.1 1992/04/04 16:32:03 bostic Exp $ (Berkeley) $Date: 1992/04/04 16:32:03 $
 */

/*
 * The map structure is an array of U_CHAR MAP pointers which are NULL or
 * valid depending if the key starts any mapping sequences.  If the pointer
 * is valid, it references a linked list of MAP pointers.  This is intended
 * to support fast lookup on non-mapped characters and too much flexibility
 * for mapped characters in general.  In order to keep the overhead of the
 * map array as small as possible, only a single pointer is maintained, so
 * the first element of the linked list has a NULL prev pointer and the last
 * element of the list has a NULL next pointer.
 */
enum whenmapped { COMMAND, INPUT };

typedef struct _map {
	struct _map *next, *prev;	/* Chain off of map array. */
	struct _map *lnext, *lprev;	/* Linked list of entries. */
	char *name;			/* Name of the map, if any. */
	char *input;			/* Input key sequence. */
	char *output;			/* Output key sequence. */
	int ilen;			/* Input key sequence length. */
	enum whenmapped when;		/* When mapping in effect. */

#define	M_USERDEF	0x01		/* If mapping user defined. */
	u_char flags;
} MAP;

typedef struct _mhead {
	struct _map *lnext, *lprev;	/* Linked list of entries. */
} MAPLIST;

#define	ismapped(ch)	(map[(ch)])	/* If character possibly mapped. */

extern MAPLIST mhead;			/* Key list head. */
extern MAP *map[UCHAR_MAX];		/* Key map. */

void	 map_init __P((void));
void	 map_save __P((FILE *));
MAP	*map_search __P((u_char *, enum whenmapped, int *));
