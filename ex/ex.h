/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: ex.h,v 8.21 1993/10/06 16:58:39 bostic Exp $ (Berkeley) $Date: 1993/10/06 16:58:39 $
 */

struct _excmdarg;

/* Ex command structure. */
typedef struct _excmdlist {
	char	*name;			/* Command name. */
					/* Underlying function. */
	int (*fn) __P((SCR *, EXF *, struct _excmdarg *));

#define	E_ADDR1		0x000001	/* One address. */
#define	E_ADDR2		0x000002	/* Two address. */
#define	E_ADDR2_ALL	0x000004	/* Zero/two addresses; zero == all. */
#define	E_ADDR2_NONE	0x000008	/* Zero/two addresses; zero == none. */
#define	E_ADDRDEF	0x000010	/* Default addresses used. */
#define	E_COUNT		0x000020	/* Count supplied. */
#define	E_FORCE		0x000040	/*  ! */

#define	E_F_CARAT	0x000080	/*  ^ flag. */
#define	E_F_DASH	0x000100	/*  - flag. */
#define	E_F_DOT		0x000200	/*  . flag. */
#define	E_F_EQUAL	0x000400	/*  = flag. */
#define	E_F_HASH	0x000800	/*  # flag. */
#define	E_F_LIST	0x001000	/*  l flag. */
#define	E_F_PLUS	0x002000	/*  + flag. */
#define	E_F_PRINT	0x004000	/*  p flag. */

#define	E_F_PRCLEAR	0x008000	/* Clear the print (#, l, p) flags. */
#define	E_NOGLOBAL	0x010000	/* Not in a global. */
#define	E_NOPERM	0x020000	/* Permission denied for now. */
#define	E_NORC		0x040000	/* Not from a .exrc or EXINIT. */
#define	E_SETLAST	0x080000	/* Reset last command. */
#define	E_ZERO		0x100000	/* 0 is a legal addr1. */
#define	E_ZERODEF	0x200000	/* 0 is default addr1 of empty files. */
	u_int	 flags;
	char	*syntax;		/* Syntax script. */
	char	*usage;			/* Usage line. */
} EXCMDLIST;
extern EXCMDLIST const cmds[];		/* List of ex commands. */

/* Structure passed around to functions implementing ex commands. */
typedef struct _excmdarg {
	EXCMDLIST const *cmd;	/* Command entry in command table. */
	int	addrcnt;	/* Number of addresses (0, 1 or 2). */
	MARK	addr1;		/* 1st address. */
	MARK	addr2;		/* 2nd address. */
	recno_t	lineno;		/* Line number. */
	u_long	count;		/* Specified count. */
	int	buffer;		/* Named buffer. */
	int	argc;		/* Count of file/word arguments. */
	char  **argv;		/* List of file/word arguments. */
	u_int	flags;		/* Selected flags from EXCMDLIST. */
} EXCMDARG;

/* Macro to set up the structure. */
#define	SETCMDARG(s, _cmd, _addrcnt, _lno1, _lno2, _force, _arg) {	\
	memset(&s, 0, sizeof(EXCMDARG));				\
	s.cmd = &cmds[_cmd];						\
	s.addrcnt = (_addrcnt);						\
	s.addr1.lno = (_lno1);						\
	s.addr2.lno = (_lno2);						\
	s.addr1.cno = s.addr2.cno = 1;					\
	if (_force)							\
		s.flags |= E_FORCE;					\
	sp->ex_argv[0] = _arg;						\
	s.argc = _arg ? 1 : 0;						\
	s.argv = sp->ex_argv;						\
}

/*
 * :next, :prev, :rewind, :tag, :tagpush, :tagpop modifications check.
 * If force is set, the autowrite is skipped.
 */
#define	MODIFY_CHECK(sp, ep, force) {					\
	if (F_ISSET((ep), F_MODIFIED))					\
		if (O_ISSET((sp), O_AUTOWRITE)) {			\
			if (!(force) &&					\
			    file_write((sp), (ep), NULL, NULL, NULL,	\
			    FS_ALL | FS_POSSIBLE))			\
				return (1);				\
		} else if (ep->refcnt <= 1 && !(force)) {		\
			msgq(sp, M_ERR,					\
	"Modified since last write; write or use ! to override.");	\
			return (1);					\
		}							\
}

/* Ex function prototypes. */
int	ex_exec_process __P((SCR *, const u_char *, const u_char *, int));
int	file_argv __P((SCR *, EXF *, char *, int *, char ***));
int	free_argv __P((SCR *));
int	proc_wait __P((SCR *, long, const char *, int));
int	word_argv __P((SCR *, EXF *, char *, int *, char ***));

/*
 * Filter actions:
 *
 *	FILTER		Filter text through the utility.
 *	FILTER_READ	Read from the utility into the file.
 *	FILTER_WRITE	Write to the utility, display its output.
 */
enum filtertype { FILTER, FILTER_READ, FILTER_WRITE };
int	filtercmd __P((SCR *, EXF *,
	    MARK *, MARK *, MARK *, char *, enum filtertype));

int	ex __P((SCR *, EXF *));
int	ex_cfile __P((SCR *, EXF *, char *));
int	ex_cmd __P((SCR *, EXF *, char *, int));
int	ex_cstring __P((SCR *, EXF *, char *, int));
int	ex_end __P((SCR *));
int	ex_gb __P((SCR *, EXF *, HDR *, int, u_int));
int	ex_getline __P((SCR *, FILE *, size_t *));
int	ex_init __P((SCR *, EXF *));
int	ex_print __P((SCR *, EXF *, MARK *, MARK *, int));
int	ex_readfp __P((SCR *, EXF *, char *, FILE *, MARK *, recno_t *, int));
int	ex_suspend __P((SCR *));
int	ex_writefp __P((SCR *, EXF *,					\
	    char *, FILE *, MARK *, MARK *, u_long *, u_long *));
void	ex_refresh __P((SCR *, EXF *));

#define	EXPROTO(type, name)						\
	type	name __P((SCR *, EXF *, EXCMDARG *))

EXPROTO(int, ex_abbr);
EXPROTO(int, ex_append);
EXPROTO(int, ex_args);
EXPROTO(int, ex_at);
EXPROTO(int, ex_bang);
EXPROTO(int, ex_bdisplay);
EXPROTO(int, ex_cc);
EXPROTO(int, ex_cd);
EXPROTO(int, ex_change);
EXPROTO(int, ex_color);
EXPROTO(int, ex_copy);
EXPROTO(int, ex_debug);
EXPROTO(int, ex_delete);
EXPROTO(int, ex_digraph);
EXPROTO(int, ex_edit);
EXPROTO(int, ex_equal);
EXPROTO(int, ex_errlist);
EXPROTO(int, ex_file);
EXPROTO(int, ex_global);
EXPROTO(int, ex_join);
EXPROTO(int, ex_list);
EXPROTO(int, ex_make);
EXPROTO(int, ex_map);
EXPROTO(int, ex_mark);
EXPROTO(int, ex_mkexrc);
EXPROTO(int, ex_move);
EXPROTO(int, ex_next);
EXPROTO(int, ex_number);
EXPROTO(int, ex_open);
EXPROTO(int, ex_pr);
EXPROTO(int, ex_preserve);
EXPROTO(int, ex_prev);
EXPROTO(int, ex_put);
EXPROTO(int, ex_quit);
EXPROTO(int, ex_read);
EXPROTO(int, ex_rew);
EXPROTO(int, ex_set);
EXPROTO(int, ex_shell);
EXPROTO(int, ex_shiftl);
EXPROTO(int, ex_shiftr);
EXPROTO(int, ex_source);
EXPROTO(int, ex_split);
EXPROTO(int, ex_stop);
EXPROTO(int, ex_subagain);
EXPROTO(int, ex_substitute);
EXPROTO(int, ex_tagpop);
EXPROTO(int, ex_tagpush);
EXPROTO(int, ex_tagtop);
EXPROTO(int, ex_unabbr);
EXPROTO(int, ex_undo);
EXPROTO(int, ex_undol);
EXPROTO(int, ex_unmap);
EXPROTO(int, ex_usage);
EXPROTO(int, ex_validate);
EXPROTO(int, ex_version);
EXPROTO(int, ex_vglobal);
EXPROTO(int, ex_visual);
EXPROTO(int, ex_viusage);
EXPROTO(int, ex_wq);
EXPROTO(int, ex_write);
EXPROTO(int, ex_xit);
EXPROTO(int, ex_yank);
EXPROTO(int, ex_z);
