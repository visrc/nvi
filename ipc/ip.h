/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	$Id: ip.h,v 8.7 1996/12/04 18:17:58 bostic Exp $ (Berkeley) $Date: 1996/12/04 18:17:58 $
 */

typedef struct _ip_private {
	int	 i_fd;		/* Input file descriptor. */
	int	 o_fd;		/* Output file descriptor. */

	size_t	 row;		/* Current row. */
	size_t	 col;		/* Current column. */

	size_t	 iblen;		/* Input buffer length. */
	size_t	 iskip;		/* Returned input buffer. */
	char	 ibuf[256];	/* Input buffer. */

#define	IP_SCR_VI_INIT  0x0001  /* Vi screen initialized. */
	u_int32_t flags;
} IP_PRIVATE;

#define	IPP(sp)		((IP_PRIVATE *)((sp)->gp->ip_private))
#define	GIPP(gp)	((IP_PRIVATE *)((gp)->ip_private))

/* The screen line relative to a specific window. */
#define	RLNO(sp, lno)	(sp)->roff + (lno)

/*
 * The IP protocol consists of frames, each containing:
 *
 *	<IPO_><object>
 *
 * XXX
 * We should have a marking byte, 0xaa to delimit frames.
 *
 */
#define	IPO_CODE	1	/* An event specification. */
#define	IPO_INT		2	/* 4-byte, network order integer. */
#define	IPO_STR		3	/* IPO_INT: followed by N bytes. */

#define	IPO_CODE_LEN	1
#define	IPO_INT_LEN	4

/* A structure that can hold the information for any frame. */
typedef struct _ip_buf {
	int code;		/* Event code. */
	const char *str;	/* String. */
	size_t len;		/* String length. */
	u_int32_t val1;		/* First value. */
	u_int32_t val2;		/* Second value. */
} IP_BUF;

/*
 * Screen/editor IP_CODE's.
 *
 * The program structure depends on the event loop being able to return
 * IPO_EOF/IPOE_ERR multiple times -- eventually enough things will end
 * due to the events that vi will reach the command level for the screen,
 * at which point the exit flags will be set and vi will exit.
 *
 * IP events sent from the screen to vi.
 */
#define	IPO_EOF		 1	/* End of input (NOT ^D). */
#define	IPO_ERR		 2	/* Input error. */
#define	IPO_INTERRUPT	 3	/* Interrupt. */
#define	IPO_MOUSE_MOVE	 4	/* Mouse click move: IPO_INT, IPO_INT. */
#define	IPO_QUIT	 5	/* Quit. */
#define	IPO_RESIZE	 6	/* Screen resize: IPO_INT, IPO_INT. */
#define	IPO_SIGHUP	 7	/* SIGHUP. */
#define	IPO_SIGTERM	 8	/* SIGTERM. */
#define	IPO_STRING	 9	/* Input string: IPO_STR. */
#define	IPO_WQ		10	/* Write and quit. */
#define	IPO_WRITE	11	/* Write. */
#define	IPO_WRITEAS	12	/* Write as another file: IPO_STR. */

/*
 * IP events sent from vi to the screen.
 */
#define	IPO_ADDSTR	 1	/* Add a string: IPO_STR. */
#define	IPO_ATTRIBUTE	 2	/* Set screen attribute: IPO_INT, IPO_INT. */
#define	IPO_BELL	 3	/* Beep/bell/flash the terminal. */
#define	IPO_BUSY_OFF	 4	/* Display a busy message: IPO_STR. */
#define	IPO_BUSY_ON	 5	/* Display a busy message: IPO_STR. */
#define	IPO_CLRTOEOL	 6	/* Clear to the end of the line. */
#define	IPO_DELETELN	 7	/* Delete a line. */
#define	IPO_DISCARD	 8	/* Discard the screen. */
#define	IPO_INSERTLN	 9	/* Insert a line. */
#define	IPO_MOVE	10	/* Move the cursor: IPO_INT, IPO_INT. */
#define	IPO_REDRAW	11	/* Redraw the screen. */
#define	IPO_REFRESH	12	/* Refresh the screen. */
#define	IPO_RENAME	13	/* Rename the screen: IPO_STR. */
#define	IPO_REWRITE	14	/* Rewrite a line: IPO_INT. */
#define	IPO_SPLIT	15	/* Split the screen. */
#define	IPO_EVENT_MAX	15

#include "ip_extern.h"
