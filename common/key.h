/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: key.h,v 8.1 1993/06/09 22:22:25 bostic Exp $ (Berkeley) $Date: 1993/06/09 22:22:25 $
 */

/* Special character lookup values. */
#define	K_CARAT		 1
#define	K_CNTRLD	 2
#define	K_CNTRLR	 3
#define	K_CNTRLT	 4
#define	K_CNTRLZ	 5
#define	K_COLON	 	 6
#define	K_CR		 7
#define	K_ESCAPE	 8
#define	K_FORMFEED	 9
#define	K_NL		10
#define	K_RIGHTBRACE	11
#define	K_RIGHTPAREN	12
#define	K_TAB		13
#define	K_VERASE	14
#define	K_VKILL		15
#define	K_VLNEXT	16
#define	K_VWERASE	17
#define	K_ZERO		18

/* The mark at the end of a range. */
#define	END_CH		'$'

/* Flags describing how input is handled. */
#define	TXT_AICHARS	0x000001	/* Leading autoindent chars. */
#define	TXT_APPENDEOL	0x000002	/* Appending after EOL. */
#define	TXT_AUTOINDENT	0x000004	/* Autoindent set this line. */
#define	TXT_BEAUTIFY	0x000008	/* Only printable characters. */
#define	TXT_BS		0x000010	/* Backspace returns the buffer. */
#define	TXT_CNTRLT	0x000020	/* Control-T is an indent special. */
#define	TXT_CR		0x000040	/* CR returns the buffer. */
#define	TXT_EMARK	0x000080	/* End of replacement mark. */
#define	TXT_ESCAPE	0x000100	/* Escape returns the buffer. */
#define	TXT_MAPCOMMAND	0x000200	/* Apply the command map. */
#define	TXT_MAPINPUT	0x000400	/* Apply the input map. */
#define	TXT_NLECHO	0x000800	/* Echo the newline. */
#define	TXT_OVERWRITE	0x001000	/* Overwrite characters. */
#define	TXT_PROMPT	0x002000	/* Display a prompt. */
#define	TXT_RECORD	0x004000	/* Record for replay. */
#define	TXT_REPLACE	0x008000	/* Replace; don't delete overwrite. */
#define	TXT_REPLAY	0x010000	/* Replay the last input. */
#define	TXT_RESOLVE	0x020000	/* Resolve the text into the file. */
#define	TXT_SHOWMATCH	0x040000	/* Showmatch flag is set. */

#define	TXT_VALID_VI							\
	(TXT_APPENDEOL | TXT_AUTOINDENT | TXT_BEAUTIFY | TXT_CNTRLT |	\
	 TXT_CR | TXT_EMARK | TXT_ESCAPE | TXT_MAPCOMMAND |		\
	 TXT_OVERWRITE | TXT_PROMPT | TXT_RECORD | TXT_REPLACE |	\
	 TXT_REPLAY | TXT_RESOLVE)

#define	TXT_VALID_EX							\
	(TXT_BEAUTIFY | TXT_CR | TXT_NLECHO | TXT_PROMPT)

#define	TXT_GETKEY_MASK							\
	(TXT_BEAUTIFY | TXT_MAPCOMMAND)

/* Support keyboard routines. */
void	term_flush_pseudo __P((SCR *));
int	term_init __P((SCR *));
int	term_key __P((SCR *, u_int));
int	term_more_pseudo __P((SCR *));
int	term_waiting __P((SCR *));
