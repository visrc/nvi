/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: ex.h,v 10.1 1995/04/13 17:22:44 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:22:44 $
 */

#define	PROMPTCHAR	':'		/* Prompt character. */

typedef struct _excmdlist {		/* Ex command table structure. */
	char *name;			/* Command name, underlying function. */
	int (*fn) __P((SCR *, EXCMD *));

#define	E_ADDR1		0x00000001	/* One address. */
#define	E_ADDR2		0x00000002	/* Two addresses. */
#define	E_ADDR2_ALL	0x00000004	/* Zero/two addresses; zero == all. */
#define	E_ADDR2_NONE	0x00000008	/* Zero/two addresses; zero == none. */
#define	E_ADDR_ZERO	0x00000010	/* 0 is a legal addr1. */
#define	E_ADDR_ZERODEF	0x00000020	/* 0 is default addr1 of empty files. */
#define	E_AUTOPRINT	0x00000040	/* Command always sets autoprint. */
#define	E_CLRFLAG	0x00000080	/* Clear the print (#, l, p) flags. */
#define	E_NEWSCREEN	0x00000100	/* Create a new screen. */
#define	E_NOPERM	0x00000200	/* Permission denied for now. */
#define	E_VIONLY	0x00000400	/* Meaningful only in vi. */
#define	__INUSE1	0xfffff800	/* Same name space as EX_PRIVATE. */
	u_int16_t flags;

	char *syntax;			/* Syntax script. */
	char *usage;			/* Usage line. */
	char *help;			/* Help line. */
} EXCMDLIST;

#define	MAXCMDNAMELEN	12		/* Longest command name. */
extern EXCMDLIST const cmds[];		/* Table of ex commands. */

/*
 * !!!
 * QUOTING NOTE:
 *
 * Historically, .exrc files and EXINIT variables could only use ^V
 * as an escape character, neither ^Q or a user specified character
 * worked.  We enforce that here, just in case someone depends on it.
 */
#define	IS_ESCAPE(sp, cmdp, ch)						\
	(F_ISSET(cmdp, E_VLITONLY) ?					\
	    (ch) == CH_LITERAL : KEY_VAL(sp, ch) == K_VLNEXT)

/*
 * File state must be checked for each command -- any ex command may be entered
 * at any time, and most of them won't work well if a file hasn't yet been read
 * in.  Historic vi generally took the easy way out and dropped core.
 */
#define	NEEDFILE(sp, cmdp) {						\
	if ((sp)->ep == NULL) {						\
		ex_message(sp, (cmdp)->name, EXM_NORC);			\
		return (1);						\
	}								\
}

/* Ex command structure (EXCMD) and ex state. */
typedef enum {
	ES_PARSE=0,			/* Parser: Ready (initial state). */
	ES_CTEXT_TEARDOWN,		/* Command: after text input mode. */
	ES_GET_CMD,			/* Command: initial input. */
	ES_ITEXT_TEARDOWN,		/* Text input mode. */
	ES_PARSE_ERROR,			/* Parser had an error. */
	ES_PARSE_EXIT,			/* Parser: after screen switch */
	ES_PARSE_FUNC,			/* Parser: after function call. */
	ES_PARSE_LINE,			/* Parser: in line search. */
	ES_PARSE_RANGE,			/* Parser: in range search. */
	ES_RUNNING,			/* Something else is running. */
} s_ex_t;

struct _excmd {
	EXCMDLIST const *cmd;		/* Command: entry in command table. */
	EXCMDLIST rcmd;			/* Command: table entryreplacement. */

	CHAR_T	  buffer;		/* Command: named buffer. */
	recno_t	  lineno;		/* Command: line number. */
	long	  count;		/* Command: signed count. */
	long	  flagoff;		/* Command: signed flag offset. */
	int	  addrcnt;		/* Command: addresses (0, 1 or 2). */
	MARK	  addr1;		/* Command: 1st address. */
	MARK	  addr2;		/* Command: 2nd address. */
	ARGS	**argv;			/* Command: array of arguments. */
	int	  argc;			/* Command: count of arguments. */

#define	E_C_BUFFER	0x00000001	/* Buffer name specified. */
#define	E_C_CARAT	0x00000002	/*  ^ flag. */
#define	E_C_COUNT	0x00000004	/* Count specified. */
#define	E_C_COUNT_NEG	0x00000008	/* Count was signed negative. */
#define	E_C_COUNT_POS	0x00000010	/* Count was signed positive. */
#define	E_C_DASH	0x00000020	/*  - flag. */
#define	E_C_DOT		0x00000040	/*  . flag. */
#define	E_C_EQUAL	0x00000080	/*  = flag. */
#define	E_C_FORCE	0x00000100	/*  ! flag. */
#define	E_C_HASH	0x00000200	/*  # flag. */
#define	E_C_LIST	0x00000400	/*  l flag. */
#define	E_C_PLUS	0x00000800	/*  + flag. */
#define	E_C_PRINT	0x00001000	/*  p flag. */
	u_int16_t iflags;		/* User input information. */

	char	 *cp;			/* Parser: current command text. */
	size_t	  cplen;		/* Parser: current command length. */
	char	 *save_cmd;		/* Parser: remaining command. */
	size_t	  save_cmdlen;		/* Parser: remaining command length. */
	char	 *arg1;			/* Parser: + argument text. */
	size_t	  arg1_len;		/* Parser: + argument length. */
	MARK	  caddr;		/* Parser: current address. */
	enum				/* Parser: command separator state. */
	    { SEP_NOTSET, SEP_NEED_N, SEP_NEED_NR, SEP_NONE } sep;
	enum				/* Parser: range address state. */
	    { ADDR_FOUND, ADDR_FOUND_DELTA, ADDR_NEED, ADDR_NONE } addr;

#define	__INUSE2	0x000004ff	/* Same name space as EXCMDLIST. */
#define	E_ABSMARK	0x00000800	/* Set the absolute mark. */
#define	E_ADDR_DEF	0x00001000	/* Default addresses used. */
#define	E_BLIGNORE	0x00002000	/* Ignore blank lines. */
#define	E_DELTA		0x00004000	/* Search address with delta. */
#define	E_MODIFY	0x00008000	/* File name expansion modified arg. */
#define	E_MOVETOEND	0x00010000	/* Move to the end of the file first. */
#define	E_NEEDSEP	0x00020000	/* Need ex output separator. */
#define	E_NEWLINE	0x00040000	/* Found ending <newline>. */
#define	E_NOAUTO	0x00080000	/* Don't do autoprint output. */
#define	E_NOPRDEF	0x00100000	/* Don't print as default. */
#define	E_OPTNUM	0x00200000	/* Number edit option affect. */
#define	E_USELASTCMD	0x00400000	/* Use the last command. */
#define	E_VISEARCH	0x00800000	/* It's really a vi search command. */
#define	E_VLITONLY	0x01000000	/* Use ^V quoting only. */
	u_int32_t flags;		/* Current flags. */
};

/* Global ranges. */
typedef struct _range	RANGE;
struct _range {				/* Global command range. */
	CIRCLEQ_ENTRY(_range) q;	/* Linked list of ranges. */
	recno_t start, stop;		/* Start/stop of the range. */
};

typedef struct _gat	GAT;
struct _gat {				/* Global and @ buffer commands. */
	LIST_ENTRY(_gat) q;		/* Linked list of @/global commands. */

	char	*cmd;			/* Command. */
	size_t	 cmd_len;		/* Command length. */

	char	*save_cmd;		/* Saved command. */
	size_t	 save_cmdlen;		/* Saved command length. */

					/* Linked list of ranges. */
	CIRCLEQ_HEAD(_rangeh, _range) rangeq;
	recno_t  range_lno;		/* Range set line number. */

	recno_t	 gstart;		/* Global start line number. */
	recno_t	 gend;			/* Global end line number. */

#define	GAT_ISGLOBAL	0x01		/* Global command. */
#define	GAT_ISV		0x02		/* V command. */
	u_int8_t flags;
};

/* Cd paths. */
typedef struct _cdpath	CDPATH;
struct _cdpath {			/* Cd path structure. */
	TAILQ_ENTRY(_cdpath) q;		/* Linked list of cd paths. */
	char *path;			/* Path. */
};

/* Ex private, per-screen memory. */
typedef struct _ex_private {
	TAILQ_HEAD(_tagh, _tag) tagq;	/* Tag list (stack). */
	TAILQ_HEAD(_tagfh, _tagf) tagfq;/* Tag file list. */
	char	*tlast;			/* Saved last tag. */

	TAILQ_HEAD(_cdh, _cdpath) cdq;	/* Cd path list. */

	LIST_HEAD(_gath, _gat) gatq;	/* Linked list of @/global commands. */

	ARGS   **args;			/* Command: argument list. */
	int	 argscnt;		/* Command: argument list count. */
	int	 argsoff;		/* Command: offset into arguments. */

	u_int32_t fdef;			/* Saved E_C_* default command flags. */

	char	*ibp;			/* File line input buffer. */
	size_t	 ibp_len;		/* File line input buffer length. */

	CHAR_T	*lastbcomm;		/* Last bang command. */

	/* XXX: Should be struct timespec's, but time_t is more portable. */
	time_t leave_atime;		/* ex_[sr]leave old access time. */
	time_t leave_mtime;		/* ex_[sr]leave old mod time. */
	struct termios leave_term;	/* ex_[sr]leave tty state. */

					/* Running function. */
	int	(*run_func) __P((SCR *, EVENT *, int *));

					/* Ex text input mode state. */
	carat_t	 im_carat;		/* State of the "[^0]^D" sequences. */
	TEXTH	 im_tiq;		/* Text input queue. */
	TEXT	*im_tp;			/* Input text structure. */
	TEXT	 im_ait;		/* Autoindent text structure. */
	CHAR_T	 im_prompt;		/* Text prompt. */
	recno_t	 im_lno;		/* Starting input line. */
	u_int32_t
		 im_flags;		/* Current TXT_* flags. */
} EX_PRIVATE;
#define	EXP(sp)	((EX_PRIVATE *)((sp)->ex_private))

/* Generic interfaces to ex. */
int	ex_optchange __P((SCR *, int));
int	ex_screen_copy __P((SCR *, SCR *));
int	ex_screen_end __P((SCR *));

/*
 * Filter actions:
 *
 *	FILTER		Filter text through the utility.
 *	FILTER_READ	Read from the utility into the file.
 *	FILTER_WRITE	Write to the utility, display its output.
 */
enum filtertype { FILTER, FILTER_READ, FILTER_WRITE };
int	filtercmd __P((SCR *, MARK *, MARK *, MARK *, char *, enum filtertype));

/* Argument expansion routines. */
int	argv_init __P((SCR *, EXCMD *));
int	argv_exp0 __P((SCR *, EXCMD *, char *, size_t));
int	argv_exp1 __P((SCR *, EXCMD *, char *, size_t, int));
int	argv_exp2 __P((SCR *, EXCMD *, char *, size_t));
int	argv_exp3 __P((SCR *, EXCMD *, char *, size_t));
int	argv_free __P((SCR *));

/* Ex common messages. */
enum exmtype { EXM_EMPTYBUF, EXM_NOPREVBUF, EXM_NOPREVRE, EXM_NORC, EXM_USAGE };
void	ex_message __P((SCR *, char *, enum exmtype));

/* Ex address errors. */
enum badaddr { A_COMBO, A_EMPTY, A_EOF, A_NOTSET, A_ZERO };
void	ex_badaddr __P((SCR *, EXCMDLIST const *, enum badaddr, enum nresult));

/* Ex function prototypes. */
int	ex __P((SCR *, EVENT *));
int	ex_aci_td __P((SCR *));
void	ex_cbuild __P((EXCMD *, int, int, recno_t, recno_t, int, ARGS *[], ARGS *, char *));
int	ex_cdalloc __P((SCR *, char *));
int	ex_cdfree __P((SCR *));
int	ex_cfile __P((SCR *, char *, u_int));
int	ex_cmd __P((SCR *));
int	ex_exec_proc __P((SCR *, char *, char *, char *));
int	ex_gb __P((SCR *, TEXTH *, int, u_int));
int	ex_getline __P((SCR *, FILE *, size_t *));
int	ex_icmd __P((SCR *, char *));
int	ex_is_abbrev __P((char *, size_t));
int	ex_is_unmap __P((char *, size_t));
int	ex_ldisplay __P((SCR *, const char *, size_t, size_t, u_int));
int	ex_ncheck __P((SCR *, int));
int	ex_print __P((SCR *, MARK *, MARK *, int));
int	ex_range __P((SCR *, EXCMD *));
int	ex_readfp __P((SCR *, char *, FILE *, MARK *, recno_t *, int));
void	ex_refresh __P((SCR *));
void	ex_rleave __P((SCR *));
int	ex_scprint __P((SCR *, MARK *, MARK *));
int	ex_screen_copy __P((SCR *, SCR *));
int	ex_screen_end __P((SCR *));
int	ex_sdisplay __P((SCR *));
int	ex_sleave __P((SCR *));
int	ex_suspend __P((SCR *));
int	ex_tdisplay __P((SCR *));
int	ex_tload __P((SCR *));
int	ex_txt_ev __P((SCR *, EVENT *, int *));
int	ex_txt_setup __P((SCR *, ARG_CHAR_T));
int	ex_writefp __P((SCR *, char *, FILE *, MARK *, MARK *, u_long *, u_long *));
void	global_insdel __P((SCR *, lnop_t, recno_t));
int	proc_wait __P((SCR *, long, const char *, int));
int	sscr_end __P((SCR *));
int	sscr_exec __P((SCR *, recno_t));
int	sscr_input __P((SCR *));

#define	EXPROTO(name)	int name __P((SCR *, EXCMD *))
EXPROTO(ex_abbr);
EXPROTO(ex_append);
EXPROTO(ex_args);
EXPROTO(ex_at);
EXPROTO(ex_bang);
EXPROTO(ex_bg);
EXPROTO(ex_cd);
EXPROTO(ex_change);
EXPROTO(ex_color);
EXPROTO(ex_copy);
EXPROTO(ex_debug);
EXPROTO(ex_delete);
EXPROTO(ex_digraph);
EXPROTO(ex_display);
EXPROTO(ex_edit);
EXPROTO(ex_equal);
EXPROTO(ex_fg);
EXPROTO(ex_file);
EXPROTO(ex_global);
EXPROTO(ex_help);
EXPROTO(ex_insert);
EXPROTO(ex_join);
EXPROTO(ex_list);
EXPROTO(ex_map);
EXPROTO(ex_mark);
EXPROTO(ex_mkexrc);
EXPROTO(ex_move);
EXPROTO(ex_next);
EXPROTO(ex_number);
EXPROTO(ex_open);
EXPROTO(ex_pr);
EXPROTO(ex_preserve);
EXPROTO(ex_prev);
EXPROTO(ex_put);
EXPROTO(ex_quit);
EXPROTO(ex_read);
EXPROTO(ex_recover);
EXPROTO(ex_resize);
EXPROTO(ex_rew);
EXPROTO(ex_s);
EXPROTO(ex_script);
EXPROTO(ex_set);
EXPROTO(ex_shell);
EXPROTO(ex_shiftl);
EXPROTO(ex_shiftr);
EXPROTO(ex_source);
EXPROTO(ex_split);
EXPROTO(ex_stop);
EXPROTO(ex_subagain);
EXPROTO(ex_subtilde);
EXPROTO(ex_tagpop);
EXPROTO(ex_tagpush);
EXPROTO(ex_tagtop);
EXPROTO(ex_unabbr);
EXPROTO(ex_undo);
EXPROTO(ex_unmap);
EXPROTO(ex_usage);
EXPROTO(ex_v);
EXPROTO(ex_validate);
EXPROTO(ex_version);
EXPROTO(ex_visual);
EXPROTO(ex_viusage);
EXPROTO(ex_wn);
EXPROTO(ex_wq);
EXPROTO(ex_write);
EXPROTO(ex_xit);
EXPROTO(ex_yank);
EXPROTO(ex_z);
