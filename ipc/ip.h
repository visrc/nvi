/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	$Id: ip.h,v 8.17 1996/12/18 10:27:47 bostic Exp $ (Berkeley) $Date: 1996/12/18 10:27:47 $
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
	const char *str1;	/* String #1. */
	u_int32_t len1;		/* String #1 length. */
	const char *str2;	/* String #1. */
	u_int32_t len2;		/* String #1 length. */
	u_int32_t val1;		/* Value #1. */
	u_int32_t val2;		/* Value #2. */
	u_int32_t val3;		/* Value #3. */
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
#define	VI_C_SEARCH	11	/* Cursor: search: IPO_INT, IPO_STR. */
#define	VI_C_SETTOP	12	/* Cursor: set screen top line: IPO_INT. */
#define	VI_C_TOP	13	/* Cursor to top. */
#define	VI_C_UP		14	/* Cursor up N lines: IPO_INT. */
#define	VI_EDIT		15	/* Edit a file: IPO_STR. */
#define	VI_EDITOPT	16	/* Edit option: 2 * IPO_STR, IPO_INT. */
#define	VI_EDITSPLIT	17	/* Split to a file: IPO_STR. */
#define	VI_EOF		18	/* End of input (NOT ^D). */
#define	VI_ERR		19	/* Input error. */
#define	VI_INTERRUPT	20	/* Interrupt. */
#define	VI_MOUSE_MOVE	21	/* Mouse click move: IPO_INT, IPO_INT. */
#define	VI_QUIT		22	/* Quit. */
#define	VI_RESIZE	23	/* Screen resize: IPO_INT, IPO_INT. */
#define	VI_SEL_END	24	/* Select end: IPO_INT, IPO_INT. */
#define	VI_SEL_START	25	/* Select start: IPO_INT, IPO_INT. */
#define	VI_SIGHUP	26	/* SIGHUP. */
#define	VI_SIGTERM	27	/* SIGTERM. */
#define	VI_STRING	28	/* Input string: IPO_STR. */
#define	VI_TAG		29	/* Tag. */
#define	VI_TAGAS	30	/* Tag to a string: IPO_STR. */
#define	VI_TAGSPLIT	31	/* Split to a tag. */
#define	VI_UNDO		32	/* Undo. */
#define	VI_WQ		33	/* Write and quit. */
#define	VI_WRITE	34	/* Write. */
#define	VI_WRITEAS	35	/* Write as another file: IPO_STR. */

#define	VI_SEARCH_EXT	0x001	/* VI_C_SEARCH: ignore case. */
#define	VI_SEARCH_IC	0x002	/* VI_C_SEARCH: ignore case. */
#define	VI_SEARCH_ICL	0x004	/* VI_C_SEARCH: ignore case if lower-case. */
#define	VI_SEARCH_INCR	0x008	/* VI_C_SEARCH: incremental search. */
#define	VI_SEARCH_LIT	0x010	/* VI_C_SEARCH: literal string. */
#define	VI_SEARCH_REV	0x020	/* VI_C_SEARCH: reverse direction. */
#define	VI_SEARCH_WR	0x040	/* VI_C_SEARCH: wrap at sof/eof. */

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
#define	SI_EDITOPT	 9	/* Edit option: 2 * IPO_STR, IPO_INT. */
#define	SI_INSERTLN	10	/* Insert a line. */
#define	SI_MOVE		11	/* Move the cursor: 2 * IPO_INT. */
#define	SI_QUIT		12	/* Quit. */
#define	SI_REDRAW	13	/* Redraw the screen. */
#define	SI_REFRESH	14	/* Refresh the screen. */
#define	SI_RENAME	15	/* Rename the screen: IPO_STR. */
#define	SI_REWRITE	16	/* Rewrite a line: IPO_INT. */
#define	SI_SCROLLBAR	17	/* Reset the scrollbar: 3 * IPO_INT. */
#define	SI_SELECT	18	/* Select area: IPO_STR. */
#define	SI_SPLIT	19	/* Split the screen. */
#define	SI_EVENT_MAX	19

#include "extern.h"
