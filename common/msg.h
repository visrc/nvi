/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: msg.h,v 5.4 1993/04/06 11:36:22 bostic Exp $ (Berkeley) $Date: 1993/04/06 11:36:22 $
 */

typedef struct _msg {
	struct _msg *next;	/* Linked list of messages. */
	char *mbuf;		/* Message. */
	size_t blen;		/* Message buffer length. */
	size_t len;		/* Message length. */

#define	M_BELL		0x01	/* Schedule a bell event. */
#define	M_DISPLAY	0x02	/* Always display. */
#define	M_EMPTY		0x04	/* No message. */
#define	M_ERROR		0x08	/* Bell, always display, inverse video. */
	u_int flags;		/* Flags. */
} MSG;

/* Messages. */
void	bell __P((struct _scr *));
void	msga __P((struct _gs *, struct _scr *, u_int, char *, size_t));
void	msgq __P((struct _scr *, u_int, const char *, ...));
