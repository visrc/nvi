/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: msg.h,v 10.2 1995/06/08 18:57:45 bostic Exp $ (Berkeley) $Date: 1995/06/08 18:57:45 $
 */

/*
 * Message types.
 *
 * !!!
 * In historical vi, O_VERBOSE didn't exist, and O_TERSE made the error
 * messages shorter.  In this implementation, O_TERSE has no effect and
 * O_VERBOSE results in informational displays about common errors, for
 * naive users.
 *
 * M_NONE	Uninitialized value.
 * M_CAT	Display to the user, no reformatting, no nothing.
 *
 * M_BERR	Error: M_ERR if O_VERBOSE, else bell.
 * M_ERR	Error: Display in inverse video.
 * M_INFO	 Info: Display in normal video.
 * M_SYSERR	Error: M_ERR, using strerror(3) message.
 * M_VINFO	 Info: M_INFO if O_VERBOSE, else ignore.
 *
 * The underlying message display routines only need to know about M_CAT,
 * M_ERR and M_INFO -- all the other message types are converted into one
 * of them by the message routines.
 */
typedef enum {
	M_NONE = 0, M_BERR, M_CAT, M_ERR, M_INFO, M_SYSERR, M_VINFO } mtype_t;

/*
 * It's possible for vi to generate messages before we have a screen in which
 * to display them.  There's a queue in the global area to hold them.
 */
typedef struct _msgh MSGH;	/* MSG list head structure. */
LIST_HEAD(_msgh, _msg);
struct _msg {
	LIST_ENTRY(_msg) q;	/* Linked list of messages. */
	mtype_t	 mtype;		/* Message type: M_CAT, M_ERR, M_INFO. */
	char	*buf;		/* Message buffer. */
	size_t	 len;		/* Message length. */
};
