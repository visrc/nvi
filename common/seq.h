/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: seq.h,v 5.2 1992/04/05 09:46:02 bostic Exp $ (Berkeley) $Date: 1992/04/05 09:46:02 $
 */

/*
 * The map structure is an UCHAR_MAX size array of MAP pointers which
 * are NULL or valid depending if the key starts any mapping sequences.
 * If the pointer is valid, it references a doubly linked list of MAP
 * pointers, threaded through the next and prev pointers.  This is
 * intended to support fast lookup on non-mapped characters and much
 * too much flexibility for mapped strings in general.  In order to
 * keep the overhead of the map array fairly small, only a single
 * pointer is maintained, so the first element of the linked list has
 * a NULL prev pointer and the last element of the list has a NULL
 * next pointer.
 *
 * In addition, each MAP structure is on another doubly linked list
 * of MAP pointers, threaded through the lnext and lprev pointers.
 * This is a linked list of all of the mapped sequences, headed by
 * the mhead MAPLIST structure.  This structure is used by the routines
 * that display all of the map entries on the screen or write them to
 * a file.
 */
enum whenmapped { COMMAND, INPUT };	/* When mapping in effect. */

typedef struct _map {
	struct _map *next, *prev;	/* Linked list from map array. */
	struct _map *lnext, *lprev;	/* Linked list of all entries. */
	char *name;			/* Name of the map, if any. */
	char *input;			/* Input key sequence. */
	char *output;			/* Output key sequence. */
	int ilen;			/* Input key sequence length. */
	enum whenmapped when;		/* When mapping in effect. */

#define	M_USERDEF	0x01		/* If mapping user defined. */
	u_char flags;
} MAP;

typedef struct _mhead {
	struct _map *lnext, *lprev;	/* Linked list of all entries. */
} MAPLIST;

#define	ismapped(ch)	(map[(ch)])	/* If ch starts any map entries. */

extern MAPLIST mhead;			/* All entries map structure. */
extern MAP *map[UCHAR_MAX];		/* Character map structure. */

int	 map_init __P((void));
int	 map_save __P((FILE *));
MAP	*map_find __P((u_char *, enum whenmapped, int *));
