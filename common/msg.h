/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: msg.h,v 5.7 1993/05/04 16:12:05 bostic Exp $ (Berkeley) $Date: 1993/05/04 16:12:05 $
 */

/*
 * M_BERR	-- Error: ring a bell if O_VERBOSE not set, else
 *		   display in inverse video.
 * M_ERR	-- Error: display in inverse video.
 * M_INFO	-- Info: display in normal video.
 * M_VINFO	-- Info: display only if O_VERBOSE set.
 *
 * In historical vi, O_VERBOSE didn't exist, and O_TERSE made the
 * error messages shorter.  In this version, O_TERSE has no effect
 * and O_VERBOSE results in informational displays about common
 * errors.
 */
enum msgtype { M_BERR, M_ERR, M_INFO, M_VINFO };

typedef struct _msg {
	struct _msg *next;	/* Linked list of messages. */
	char *mbuf;		/* Message. */
	size_t blen;		/* Message buffer length. */
	size_t len;		/* Message length. */

#define	M_EMPTY		0x01	/* No message. */
#define	M_INV_VIDEO	0x02	/* Inverse video. */
	u_int flags;		/* Flags. */
} MSG;

/* Messages. */
void	bell __P((struct _scr *));
void	msga __P((struct _gs *, struct _scr *, int, char *, size_t));
void	msgq __P((struct _scr *, enum msgtype, const char *, ...));
