/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: ex.h,v 5.33 1993/02/28 11:53:34 bostic Exp $ (Berkeley) $Date: 1993/02/28 11:53:34 $
 */

struct excmdarg;

/* Ex command structure. */
typedef struct {
	char *name;			/* Command name. */
					/* Underlying function. */
	int (*fn) __P((EXF *, struct excmdarg *));

#define	E_ADDR1		0x00001		/* One address. */
#define	E_ADDR2		0x00002		/* Two address. */
#define	E_ADDR2_ALL	0x00004		/* Zero/two addresses; zero == all. */
#define	E_ADDR2_NONE	0x00008		/* Zero/two addresses; zero == none. */
#define	E_FORCE		0x00010		/*  ! */

#define	E_F_CARAT	0x00020		/*  ^ flag. */
#define	E_F_DASH	0x00040		/*  - flag. */
#define	E_F_DOT		0x00080		/*  . flag. */
#define	E_F_HASH	0x00100		/*  # flag. */
#define	E_F_LIST	0x00200		/*  l flag. */
#define	E_F_PLUS	0x00400		/*  + flag. */
#define	E_F_PRINT	0x00800		/*  p flag. */
#define	E_F_MASK	0x00fe0		/* Flag mask. */
#define	E_F_PRCLEAR	0x01000		/* Clear the print (#, l, p) flags. */

#define	E_NOGLOBAL	0x02000		/* Not in a global. */
#define	E_NOPERM	0x04000		/* Permission denied for now. */
#define	E_NORC		0x08000		/* Not from a .exrc or EXINIT. */
#define	E_SETLAST	0x10000		/* Reset last command. */
#define	E_ZERO		0x20000		/* 0 is a legal addr1. */
#define	E_ZERODEF	0x40000		/* 0 is default addr1 of empty files. */
	u_int flags;
	char *syntax;			/* Syntax script. */
	char *usage;			/* Usage line. */
} EXCMDLIST;
extern EXCMDLIST cmds[];		/* List of ex commands. */

/* Structure passed around to functions implementing ex commands. */
typedef struct excmdarg {
	EXCMDLIST *cmd;		/* Command entry in command table. */
	int addrcnt;		/* Number of addresses (0, 1 or 2). */
	MARK addr1;		/* 1st address. */
	MARK addr2;		/* 2nd address. */
	recno_t lineno;		/* Line number. */
	u_int flags;		/* Selected flags from EXCMDLIST. */
	int argc;		/* Count of file/word arguments. */
	u_char **argv;		/* List of file/word arguments. */
	u_char *command;	/* Command line, if parse locally. */
	u_char *plus;		/* '+' command word. */
	u_char *string;		/* String. */
	int buffer;		/* Named buffer. */
} EXCMDARG;

extern u_char *defcmdarg[2];	/* Default array. */

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
	s.argc = _arg ? 1 : 0;						\
	s.argv = defcmdarg;						\
	s.string = (u_char *)"";					\
	defcmdarg[0] = (u_char *)_arg;					\
}

/* Control character. */
#define	ctrl(ch)	((ch) & 0x1f)

int	 buildargv __P((EXF *, u_char *, int, EXCMDARG *));
int	 esystem __P((EXF *, const u_char *, const u_char *));

int	 ex_abbr __P((EXF *, EXCMDARG *));
int	 ex_append __P((EXF *, EXCMDARG *));
int	 ex_args __P((EXF *, EXCMDARG *));
int	 ex_at __P((EXF *, EXCMDARG *));
int	 ex_bang __P((EXF *, EXCMDARG *));
int	 ex_bdisplay __P((EXF *, EXCMDARG *));
int	 ex_cc __P((EXF *, EXCMDARG *));
int	 ex_cd __P((EXF *, EXCMDARG *));
int	 ex_cfile __P((EXF *, char *, int));
int	 ex_change __P((EXF *, EXCMDARG *));
int	 ex_cmd __P((EXF *, u_char *));
int	 ex_color __P((EXF *, EXCMDARG *));
enum confirmation ex_confirm __P((EXF *, MARK *, MARK *));
int	 ex_copy __P((EXF *, EXCMDARG *));
int	 ex_cstring __P((EXF *, u_char *, int, int));
int	 ex_debug __P((EXF *, EXCMDARG *));
int	 ex_delete __P((EXF *, EXCMDARG *));
int	 ex_digraph __P((EXF *, EXCMDARG *));
int	 ex_edit __P((EXF *, EXCMDARG *));
int	 ex_end __P((EXF *));
int	 ex_equal __P((EXF *, EXCMDARG *));
int	 ex_errlist __P((EXF *, EXCMDARG *));
int	 ex_file __P((EXF *, EXCMDARG *));
u_char	*ex_getline __P((EXF *, FILE *, size_t *));
int	 ex_global __P((EXF *, EXCMDARG *));
int	 ex_init __P((EXF *));
int	 ex_join __P((EXF *, EXCMDARG *));
int	 ex_list __P((EXF *, EXCMDARG *));
int	 ex_make __P((EXF *, EXCMDARG *));
int	 ex_map __P((EXF *, EXCMDARG *));
int	 ex_mark __P((EXF *, EXCMDARG *));
int	 ex_mkexrc __P((EXF *, EXCMDARG *));
int	 ex_move __P((EXF *, EXCMDARG *));
int	 ex_next __P((EXF *, EXCMDARG *));
int	 ex_number __P((EXF *, EXCMDARG *));
int	 ex_prev __P((EXF *, EXCMDARG *));
int	 ex_pr __P((EXF *, EXCMDARG *));
int	 ex_print __P((EXF *, MARK *, MARK *, int));
int	 ex_put __P((EXF *, EXCMDARG *));
int	 ex_quit __P((EXF *, EXCMDARG *));
int	 ex_read __P((EXF *, EXCMDARG *));
int	 ex_readfp __P((EXF *, char *, FILE *, MARK *, recno_t *));
void	 ex_refresh __P((EXF *));
int	 ex_rew __P((EXF *, EXCMDARG *));
int	 ex_set __P((EXF *, EXCMDARG *));
int	 ex_shell __P((EXF *, EXCMDARG *));
int	 ex_shiftl __P((EXF *, EXCMDARG *));
int	 ex_shiftr __P((EXF *, EXCMDARG *));
int	 ex_source __P((EXF *, EXCMDARG *));
int	 ex_stop __P((EXF *, EXCMDARG *));
int	 ex_subagain __P((EXF *, EXCMDARG *));
int	 ex_substitute __P((EXF *, EXCMDARG *));
int	 ex_suspend __P((EXF *));
int	 ex_tagpop __P((EXF *, EXCMDARG *));
int	 ex_tagpush __P((EXF *, EXCMDARG *));
int	 ex_tagtop __P((EXF *, EXCMDARG *));
int	 ex_unabbr __P((EXF *, EXCMDARG *));
int	 ex_undo __P((EXF *, EXCMDARG *));
int	 ex_unmap __P((EXF *, EXCMDARG *));
int	 ex_usage __P((EXF *, EXCMDARG *));
int	 ex_validate __P((EXF *, EXCMDARG *));
int	 ex_version __P((EXF *, EXCMDARG *));
int	 ex_vglobal __P((EXF *, EXCMDARG *));
int	 ex_visual __P((EXF *, EXCMDARG *));
int	 ex_viusage __P((EXF *, EXCMDARG *));
int	 ex_wq __P((EXF *, EXCMDARG *));
int	 ex_write __P((EXF *, EXCMDARG *));
int	 ex_writefp __P((EXF *, char *, FILE *, MARK *, MARK *, int));
int	 ex_xit __P((EXF *, EXCMDARG *));
int	 ex_yank __P((EXF *, EXCMDARG *));
