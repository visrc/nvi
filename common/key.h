/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: key.h,v 8.8 1993/09/30 12:02:19 bostic Exp $ (Berkeley) $Date: 1993/09/30 12:02:19 $
 */

/* Structure for a key input buffer. */
typedef struct _ibuf {
	char	*buf;		/* Buffer itself. */
	int	 cnt;		/* Count of characters. */
	int	 len;		/* Buffer length. */
	int	 next;		/* Offset of next character. */
} IBUF;
				/* Flush keys from expansion buffer. */
#define	TERM_KEY_FLUSH(sp)	((sp)->gp->key->cnt = (sp)->gp->key->next = 0)
				/* Return if more keys in expansion buffer. */
#define	TERM_KEY_MORE(sp)	((sp)->gp->key->cnt)

/*
 * Structure to name a character.  Used both as an interface to the screen
 * and to name objects referenced by characters in error messages.
 */
typedef struct _chname {
	char	*name;		/* Character name. */
	u_char	 len;		/* Length of the character name. */
} CHNAME;

/* The maximum number of columns any character can take up on a screen. */
#define	MAX_CHARACTER_COLUMNS	4

/*
 * Routines that return a key as a side-effect return:
 *
 *	INP_OK		Returning a character; must be 0.
 *	INP_EOF		EOF.
 *	INP_ERR		Error.
 *
 * Routines that return a confirmation return:
 *
 *	CONF_NO		User answered no.
 *	CONF_QUIT	User answered quit, eof or an error.
 *	CONF_YES	User answered yes.
 *
 * The vi structure depends on the key routines being able to return INP_EOF
 * multiple times without failing -- eventually enough things will end due to
 * INP_EOF that vi will reach the command level for the screen, at which point
 * the exit flags will be set and vi will exit.
 */
enum confirm	{ CONF_NO, CONF_QUIT, CONF_YES };
enum input	{ INP_OK=0, INP_EOF, INP_ERR };

/*
 * Ex/vi commands are generally separated by whitespace characters.  We
 * can't use the standard isspace(3) macro because it returns true for
 * characters like ^K in the ASCII character set.  The 4.4BSD isblank(3)
 * macro does exactly what we want, but it's not portable yet.
 *
 * XXX
 * Note side effect, ch is evaluated multiple times.
 */
#ifndef isblank
#define	isblank(ch)	((ch) == ' ' || (ch) == '\t')
#endif

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

/* Various special characters. */
#define	END_CH		'$'			/* End of a range. */
#define	YES_CH		'y'			/* Yes. */
#define	QUIT_CH		'q'			/* Quit. */
#define	NO_CH		'n'			/* No. */
#define	CONFSTRING	"confirm? [ynq]"
#define	CONTMSG		"Enter return to continue: "
#define	CONTMSG_I	"Enter return to continue [q to quit]: "

/* Flags describing how input is handled. */
#define	TXT_AICHARS	0x000001	/* Leading autoindent chars. */
#define	TXT_ALTWERASE	0x000002	/* Option: altwerase. */
#define	TXT_APPENDEOL	0x000004	/* Appending after EOL. */
#define	TXT_AUTOINDENT	0x000008	/* Autoindent set this line. */
#define	TXT_BEAUTIFY	0x000010	/* Only printable characters. */
#define	TXT_BS		0x000020	/* Backspace returns the buffer. */
#define	TXT_CNTRLT	0x000040	/* Control-T is an indent special. */
#define	TXT_CR		0x000080	/* CR returns the buffer. */
#define	TXT_EMARK	0x000100	/* End of replacement mark. */
#define	TXT_ESCAPE	0x000200	/* Escape returns the buffer. */
#define	TXT_MAPCOMMAND	0x000400	/* Apply the command map. */
#define	TXT_MAPINPUT	0x000800	/* Apply the input map. */
#define	TXT_NLECHO	0x001000	/* Echo the newline. */
#define	TXT_OVERWRITE	0x002000	/* Overwrite characters. */
#define	TXT_PROMPT	0x004000	/* Display a prompt. */
#define	TXT_RECORD	0x008000	/* Record for replay. */
#define	TXT_REPLACE	0x010000	/* Replace; don't delete overwrite. */
#define	TXT_REPLAY	0x020000	/* Replay the last input. */
#define	TXT_RESOLVE	0x040000	/* Resolve the text into the file. */
#define	TXT_SHOWMATCH	0x080000	/* Option: showmatch. */

#define	TXT_VALID_EX							\
	(TXT_BEAUTIFY | TXT_CR | TXT_NLECHO | TXT_PROMPT)

#define	TXT_GETKEY_MASK							\
	(TXT_BEAUTIFY | TXT_MAPCOMMAND | TXT_MAPINPUT)

/* Support keyboard routines. */
int	term_init __P((struct _scr *));
enum input
	term_key __P((struct _scr *, CHAR_T *, u_int));
int	term_push __P((struct _scr *, IBUF *, char *, size_t));
int	term_waiting __P((struct _scr *));
