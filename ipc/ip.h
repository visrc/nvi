/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	$Id: ip.h,v 8.11 1996/12/10 21:00:14 bostic Exp $ (Berkeley) $Date: 1996/12/10 21:00:14 $
 */

extern int vi_ofd;		/* Output file descriptor. */

typedef struct _ip_private {
	int	 i_fd;		/* Input file descriptor. */

	size_t	 row;		/* Current row. */
	size_t	 col;		/* Current column. */

	recno_t	 sb_total;	/* scrollbar: total lines in file. */
	recno_t	 sb_top;	/* scrollbar: top line on screen. */
	size_t	 sb_num;	/* scrollbar: number of lines on screen. */

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
	u_int32_t val3;		/* Third value. */
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
#define	VI_C_BOL	 1	/* Cursor to start of line. */
#define	VI_C_BOTTOM	 2	/* Cursor to bottom. */
#define	VI_C_DEL	 3	/* Cursor delete. */
#define	VI_C_DOWN	 4	/* Cursor down N lines: IPO_INT. */
#define	VI_C_EOL	 5	/* Cursor to end of line. */
#define	VI_C_INSERT	 6	/* Cursor: enter insert mode. */
#define	VI_C_LEFT	 7	/* Cursor left. */
#define	VI_C_PGDOWN	 8	/* Cursor down N pages: IPO_INT. */
#define	VI_C_PGUP	 9	/* Cursor up N lines: IPO_INT. */
#define	VI_C_RIGHT	10	/* Cursor right. */
#define	VI_C_TOP	11	/* Cursor to top. */
#define	VI_C_UP		12	/* Cursor up N lines: IPO_INT. */
#define	VI_EDIT		13	/* Edit a file: IPO_STR. */
#define	VI_EDITSPLIT	14	/* Split to a file: IPO_STR. */
#define	VI_EOF		15	/* End of input (NOT ^D). */
#define	VI_ERR		16	/* Input error. */
#define	VI_INTERRUPT	17	/* Interrupt. */
#define	VI_MOUSE_MOVE	18	/* Mouse click move: IPO_INT, IPO_INT. */
#define	VI_QUIT		19	/* Quit. */
#define	VI_RESIZE	20	/* Screen resize: IPO_INT, IPO_INT. */
#define	VI_SIGHUP	21	/* SIGHUP. */
#define	VI_SIGTERM	22	/* SIGTERM. */
#define	VI_STRING	23	/* Input string: IPO_STR. */
#define	VI_TAG		24	/* Tag. */
#define	VI_TAGAS	25	/* Tag to a string: IPO_STR. */
#define	VI_TAGSPLIT	26	/* Split to a tag. */
#define	VI_UNDO		27	/* Undo. */
#define	VI_WQ		28	/* Write and quit. */
#define	VI_WRITE	29	/* Write. */
#define	VI_WRITEAS	30	/* Write as another file: IPO_STR. */

/*
 * IP events sent from vi to the screen.
 */
#define	SI_ADDSTR	 1	/* Add a string: IPO_STR. */
#define	SI_ATTRIBUTE	 2	/* Set screen attribute: 2 * IPO_INT. */
#define	SI_BELL		 3	/* Beep/bell/flash the terminal. */
#define	SI_BUSY_OFF	 4	/* Display a busy message: IPO_STR. */
#define	SI_BUSY_ON	 5	/* Display a busy message: IPO_STR. */
#define	SI_CLRTOEOL	 6	/* Clear to the end of the line. */
#define	SI_DELETELN	 7	/* Delete a line. */
#define	SI_DISCARD	 8	/* Discard the screen. */
#define	SI_INSERTLN	 9	/* Insert a line. */
#define	SI_MOVE		10	/* Move the cursor: 2 * IPO_INT. */
#define	SI_QUIT		11	/* Quit. */
#define	SI_REDRAW	12	/* Redraw the screen. */
#define	SI_REFRESH	13	/* Refresh the screen. */
#define	SI_RENAME	14	/* Rename the screen: IPO_STR. */
#define	SI_REWRITE	15	/* Rewrite a line: IPO_INT. */
#define	SI_SCROLLBAR	16	/* Reset the scrollbar: 3 * IPO_INT. */
#define	SI_SPLIT	17	/* Split the screen. */
#define	SI_EVENT_MAX	17

#include "ip_extern.h"
