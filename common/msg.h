/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: msg.h,v 8.10 1994/03/16 08:05:52 bostic Exp $ (Berkeley) $Date: 1994/03/16 08:05:52 $
 */

/*
 * Message types.
 *
 * !!!
 * In historical vi, O_VERBOSE didn't exist, and O_TERSE made the error
 * messages shorter.  In this implementation, O_TERSE has no effect and
 * O_VERBOSE results in informational displays about common errors for
 * naive users.
 *
 * M_BERR	Error: M_ERR if O_VERBOSE, else bell.
 * M_ERR	Error: Display in inverse video.
 * M_INFO	 Info: Display in normal video.
 * M_SYSERR	Error: M_ERR, using strerror(3) message.
 * M_VINFO	 Info: M_INFO if O_VERBOSE, else ignore.
 */
enum msgtype { M_BERR, M_ERR, M_INFO, M_SYSERR, M_VINFO };

typedef struct _msgh MSGH;	/* MESG list head structure. */
LIST_HEAD(_msgh, _msg);

struct _msg {
	LIST_ENTRY(_msg) q;	/* Linked list of messages. */
	char	*mbuf;		/* Message buffer. */
	size_t	 blen;		/* Message buffer length. */
	size_t	 len;		/* Message length. */

#define	M_EMPTY		0x01	/* No message. */
#define	M_INV_VIDEO	0x02	/* Inverse video. */
	u_int	 flags;		/* Flags. */
};

/* Messages. */
void	msg_app __P((GS *, SCR *, int, char *, size_t));
int	msg_rpt __P((SCR *, int));
void	msgq __P((SCR *, enum msgtype, const char *, ...));
