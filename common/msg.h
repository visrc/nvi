/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: msg.h,v 5.3 1993/03/26 13:37:52 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:37:52 $
 */

#define	M_BELL		0x01	/* Schedule a bell event. */
#define	M_DISPLAY	0x02	/* Always display. */
#define	M_EMPTY		0x04	/* No message. */
#define	M_ERROR		0x08	/* Bell, always display, inverse video. */

typedef struct _msg {
	struct _msg *next;	/* Linked list of messages. */
	char *mbuf;		/* Message. */
	size_t blen;		/* Message buffer length. */
	size_t len;		/* Message length. */
	u_int flags;		/* Flags. */
} MSG;

/* Messages. */
void	bell __P((struct _scr *));
void	msgq __P((struct _scr *, u_int, const char *, ...));
