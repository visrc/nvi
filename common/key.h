/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: key.h,v 10.1 1995/04/13 17:18:41 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:18:41 $
 */

/*
 * Fundamental character types.
 *
 * CHAR_T	An integral type that can hold any character.
 * ARG_CHAR_T	The type of a CHAR_T when passed as an argument using
 *		traditional promotion rules.  It should also be able
 *		to be compared against any CHAR_T for equality without
 *		problems.
 * MAX_CHAR_T	The maximum value of any character.
 *
 * If no integral type can hold a character, don't even try the port.
 */
typedef	u_char		CHAR_T;
typedef	u_int		ARG_CHAR_T;
#define	MAX_CHAR_T	0xff

/* The maximum number of columns any character can take up on a screen. */
#define	MAX_CHARACTER_COLUMNS	4

/*
 * Event types.
 *
 * The program structure depends on the event loop being able to return
 * E_EOF/E_ERR multiple times -- eventually enough things will end due
 * to the events that vi will reach the command level for the screen, at
 * which point the exit flags will be set and vi will exit.
 */
typedef enum {
	E_CHARACTER,		/* Input character: e_c set. */
	E_EOF,			/* End of input (NOT ^D). */
	E_ERR,			/* Input error. */
	E_INTERRUPT,		/* Interrupt. */
	E_REPAINT,		/* Repaint: e_flno, e_tlno set. */
	E_RESIZE,		/* SIGWINCH arrived: e_lno, e_cno set. */
	E_SIGCONT,		/* SIGCONT arrived. */
	E_SIGHUP,		/* SIGHUP arrived. */
	E_SIGTERM,		/* SIGTERM arrived. */
	E_START,		/* Start ex/vi. */
	E_STOP,			/* Stop ex/vi. */
	E_STRING,		/* Input string: e_csp, e_len set. */
	E_TIMEOUT,		/* Timeout. */
} e_event_t;

/*
 * Character values.
 */
typedef enum {
	K_NOTUSED = 0,		/* Not set. */
	K_BACKSLASH,		/*  \ */
	K_CARAT,		/*  ^ */
	K_CNTRLD,		/* ^D */
	K_CNTRLR,		/* ^R */
	K_CNTRLT,		/* ^T */
	K_CNTRLZ,		/* ^Z */
	K_COLON,		/*  : */
	K_CR,			/* \r */
	K_ESCAPE,		/* ^[ */
	K_FORMFEED,		/* \f */
	K_HEXCHAR,		/* ^X */
	K_NL,			/* \n */
	K_RIGHTBRACE,		/*  } */
	K_RIGHTPAREN,		/*  ) */
	K_TAB,			/* \t */
	K_VERASE,		/* set from tty: default ^H */
	K_VKILL,		/* set from tty: default ^U */
	K_VLNEXT,		/* set from tty: default ^V */
	K_VWERASE,		/* set from tty: default ^W */
	K_ZERO,			/*  0 */
} e_key_t;

struct _event {
	TAILQ_ENTRY(_event) q;		/* Linked list of events. */
	e_event_t e_event;		/* Event type. */
	union {
		struct {		/* Input character. */
			CHAR_T c;	/* Character. */
			e_key_t value;	/* Key type. */

#define	CH_ABBREVIATED	0x01		/* Character is from an abbreviation. */
#define	CH_MAPPED	0x02		/* Character is from a map. */
#define	CH_NOMAP	0x04		/* Do not map the character. */
#define	CH_QUOTED	0x08		/* Character is already quoted. */
			u_int8_t flags;
		} _e_ch;
#define	e_ch	_u_event._e_ch		/* !!! The structure, not the char. */
#define	e_c	_u_event._e_ch.c
#define	e_value	_u_event._e_ch.value
#define	e_flags	_u_event._e_ch.flags

		struct {		/* Screen position, size. */
			size_t lno1;	/* Line number. */
			size_t cno1;	/* Column number. */
			size_t lno2;	/* Line number. */
			size_t cno2;	/* Column number. */
		} _e_mark;
#define	e_lno	_u_event._e_mark.lno1	/* Single location. */
#define	e_cno	_u_event._e_mark.cno1
#define	e_flno	_u_event._e_mark.lno1	/* Text region. */
#define	e_fcno	_u_event._e_mark.cno1
#define	e_tlno	_u_event._e_mark.lno2
#define	e_tcno	_u_event._e_mark.cno2

		struct {		/* Input string. */
			CHAR_T	*csp;	/* String. */
			size_t	 len;	/* String length. */
		} _e_str;
#define	e_csp	_u_event._e_str.csp
#define	e_len	_u_event._e_str.len
	} _u_event;
};

typedef struct _keylist {
	e_key_t value;		/* Special value. */
	CHAR_T ch;		/* Key. */
} KEYLIST;
extern KEYLIST keylist[];

				/* Return if more keys in queue. */
#define	KEYS_WAITING(sp)	((sp)->gp->i_cnt != 0)
#define	MAPPED_KEYS_WAITING(sp)						\
	(KEYS_WAITING(sp) &&						\
	    F_ISSET(&sp->gp->i_event[sp->gp->i_next].e_ch, CH_MAPPED))

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

/* The "standard" tab width, for displaying things to users. */
#define	STANDARD_TAB	6

/* Various special characters, messages. */
#define	CH_BSEARCH	'?'			/* Backward search prompt. */
#define	CH_CURSOR	' '			/* Cursor character. */
#define	CH_ENDMARK	'$'			/* End of a range. */
#define	CH_EXPROMPT	':'			/* Ex prompt. */
#define	CH_FSEARCH	'/'			/* Forward search prompt. */
#define	CH_HEX		'\030'			/* Leading hex character. */
#define	CH_LITERAL	'\026'			/* ASCII ^V. */
#define	CH_NO		'n'			/* No. */
#define	CH_NOT_DIGIT	'a'			/* A non-isdigit() character. */
#define	CH_QUIT		'q'			/* Quit. */
#define	CH_YES		'y'			/* Yes. */

#define	STR_CONFIRM	"confirm? [ynq]"
#define	STR_CMSG	"Press any key to continue: "
#define	STR_CMSG_S	" cont?"
#define	STR_QMSG	"Press any key to continue [q to quit]: "

/* Flags describing text input special cases. */
#define	TXT_ADDNEWLINE	0x00000001	/* Replay starts on a new line. */
#define	TXT_AICHARS	0x00000002	/* Leading autoindent chars. */
#define	TXT_ALTWERASE	0x00000004	/* Option: altwerase. */
#define	TXT_APPENDEOL	0x00000008	/* Appending after EOL. */
#define	TXT_AUTOINDENT	0x00000010	/* Autoindent set this line. */
#define	TXT_BACKSLASH	0x00000020	/* Backslashes escape characters. */
#define	TXT_BEAUTIFY	0x00000040	/* Only printable characters. */
#define	TXT_BS		0x00000080	/* Backspace returns the buffer. */
#define	TXT_CNTRLD	0x00000100	/* Control-D is a command. */
#define	TXT_CNTRLT	0x00000200	/* Control-T is an indent special. */
#define	TXT_CR		0x00000400	/* CR returns the buffer. */
#define	TXT_DOTTERM	0x00000800	/* Leading '.' terminates the input. */
#define	TXT_EMARK	0x00001000	/* End of replacement mark. */
#define	TXT_EOFCHAR	0x00002000	/* ICANON set, return EOF character. */
#define	TXT_ESCAPE	0x00004000	/* Escape returns the buffer. */
#define	TXT_INFOLINE	0x00008000	/* Editing the info line. */
#define	TXT_NLECHO	0x00010000	/* Echo the newline. */
#define	TXT_NUMBER	0x00020000	/* Number the line. */
#define	TXT_OVERWRITE	0x00040000	/* Overwrite characters. */
#define	TXT_PROMPT	0x00080000	/* Display a prompt. */
#define	TXT_RECORD	0x00100000	/* Record for replay. */
#define	TXT_REPLACE	0x00200000	/* Replace; don't delete overwrite. */
#define	TXT_REPLAY	0x00400000	/* Replay the last input. */
#define	TXT_RESOLVE	0x00800000	/* Resolve the text into the file. */
#define	TXT_SHOWMATCH	0x01000000	/* Option: showmatch. */
#define	TXT_TTYWERASE	0x02000000	/* Option: ttywerase. */
#define	TXT_WRAPMARGIN	0x04000000	/* Option: wrapmargin. */

/* Support event/key routines. */
int	   v_event_flush __P((SCR *, u_int));
int	   v_event_handler __P((SCR *, EVENT *, u_int32_t *));
int	   v_event_pull __P((SCR *, EVENT **));
int	   v_event_push __P((SCR *, CHAR_T *, size_t, u_int));
void	   v_key_ilookup __P((SCR *));
int	   v_key_init __P((SCR *));
size_t	 __v_key_len __P((SCR *, ARG_CHAR_T));
CHAR_T	*__v_key_name __P((SCR *, ARG_CHAR_T));
int	 __v_key_val __P((SCR *, ARG_CHAR_T));
