/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: msg.h,v 8.7 1993/11/18 10:55:04 bostic Exp $ (Berkeley) $Date: 1993/11/18 10:55:04 $
 */

/*
 * M_BERR	-- Error: ring a bell if O_VERBOSE not set, else
 *		   display in inverse video.
 * M_ERR	-- Error: display in inverse video.
 * M_INFO	-- Info: display in normal video.
 * M_SYSERR	-- M_ERR, but use standard error message.
 * M_VINFO	-- Info: display only if O_VERBOSE set.
 *
 * In historical vi, O_VERBOSE didn't exist, and O_TERSE made the
 * error messages shorter.  In this version, O_TERSE has no effect
 * and O_VERBOSE results in informational displays about common
 * errors.
 */
enum msgtype { M_BERR, M_ERR, M_INFO, M_SYSERR, M_VINFO };

typedef struct _msgh MSGH;	/* MESG list head structure. */
LIST_HEAD(_msgh, _msg);

struct _msg {
	LIST_ENTRY(_msg) q;	/* Linked list of messages. */
	char *mbuf;		/* Message. */
	size_t blen;		/* Message buffer length. */
	size_t len;		/* Message length. */

#define	M_EMPTY		0x01	/* No message. */
#define	M_INV_VIDEO	0x02	/* Inverse video. */
	u_int flags;		/* Flags. */
};

/* Messages. */
void	msg_app __P((GS *, SCR *, int, char *, size_t));
int	msg_rpt __P((SCR *, int));
void	msgq __P((SCR *, enum msgtype, const char *, ...));
