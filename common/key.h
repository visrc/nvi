/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: key.h,v 5.25 1993/04/18 09:37:36 bostic Exp $ (Berkeley) $Date: 1993/04/18 09:37:36 $
 */

/* Special character lookup values. */
#define	K_CARAT		 1
#define	K_CNTRLD	 2
#define	K_CNTRLR	 3
#define	K_CNTRLT	 4
#define	K_CNTRLZ	 5
#define	K_CR		 6
#define	K_ESCAPE	 7
#define	K_FORMFEED	 8
#define	K_NL		 9
#define	K_TAB		10
#define	K_VERASE	11
#define	K_VKILL		12
#define	K_VLNEXT	13
#define	K_VWERASE	14
#define	K_ZERO		15

/* The mark at the end of a range. */
#define	END_CH		'$'

/* Flags describing how input is handled. */
#define	TXT_APPENDEOL	0x000001	/* Appending after EOL. */
#define	TXT_AUTOINDENT	0x000002	/* Autoindent set this line. */
#define	TXT_BEAUTIFY	0x000004	/* Only printable characters. */
#define	TXT_BS		0x000008	/* Backspace returns the buffer. */
#define	TXT_CNTRLT	0x000010	/* Control-T is an indent special. */
#define	TXT_CR		0x000020	/* CR returns the buffer. */
#define	TXT_EMARK	0x000040	/* End of replacement mark. */
#define	TXT_ESCAPE	0x000080	/* Escape returns the buffer. */
#define	TXT_MAPCOMMAND	0x000100	/* Apply the command map. */
#define	TXT_MAPINPUT	0x000200	/* Apply the input map. */
#define	TXT_NLECHO	0x000400	/* Echo the newline. */
#define	TXT_OVERWRITE	0x000800	/* Overwrite characters. */
#define	TXT_PROMPT	0x001000	/* Display a prompt. */
#define	TXT_RECORD	0x002000	/* Record for replay. */
#define	TXT_REPLACE	0x004000	/* Replace; don't delete overwrite. */
#define	TXT_REPLAY	0x008000	/* Replay the last input. */
#define	TXT_RESOLVE	0x010000	/* Resolve the text into the file. */

#define	TXT_VALID_VI							\
	(TXT_APPENDEOL | TXT_AUTOINDENT | TXT_BEAUTIFY | TXT_CNTRLT |	\
	 TXT_CR | TXT_EMARK | TXT_ESCAPE | TXT_MAPCOMMAND |		\
	 TXT_MAPINPUT | TXT_OVERWRITE | TXT_PROMPT | TXT_RECORD |	\
	 TXT_REPLACE | TXT_REPLAY | TXT_RESOLVE)

#define	TXT_VALID_EX							\
	(TXT_BEAUTIFY | TXT_CR | TXT_MAPCOMMAND | TXT_MAPINPUT |	\
	 TXT_NLECHO | TXT_PROMPT)

#define	TXT_GETKEY_MASK							\
	(TXT_BEAUTIFY | TXT_MAPCOMMAND | TXT_MAPINPUT)

/* Support keyboard routines. */
int	 getkey __P((SCR *, u_int));
int	 key_special __P((SCR *));
