/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: ex.h,v 5.10 1992/04/19 10:53:38 bostic Exp $ (Berkeley) $Date: 1992/04/19 10:53:38 $
 */

struct excmdarg;

/* Ex command structure. */
typedef struct {
	char *name;			/* Command name. */
					/* Underlying function. */
	int (*fn) __P((struct excmdarg *));

#define	E_ADDR1		0x00001		/* One address. */
#define	E_ADDR2		0x00002		/* Two address. */
#define	E_ADDR2_OR_0	0x00004		/* Either 0 or two addresses. */
#define	E_APPEND	0x00008		/* >> */
#define	E_EXRCOK	0x00010		/* OK in a .exrc file. */
#define	E_FORCE		0x00020		/*  ! */

#define	E_F_CARAT	0x00040		/*  ^ flag. */
#define	E_F_DASH	0x00080		/*  - flag. */
#define	E_F_DOT		0x00100		/*  . flag. */
#define	E_F_HASH	0x00200		/*  # flag. */
#define	E_F_LIST	0x00400		/*  l flag. */
#define	E_F_PLUS	0x00800		/*  + flag. */
#define	E_F_PRINT	0x01000		/*  p flag. */
#define	E_F_MASK	0x01fc0		/* Flag mask. */

#define	E_NL		0x02000		/* Newline first if not MODE_EX mode.*/
#define	E_NOPERM	0x04000		/* Permission denied for now. */
#define	E_SETLAST	0x08000		/* Reset last command. */
#define	E_ZERO		0x10000		/* 0 is a legal (first) address.*/
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
	MARK lineno;		/* Line number. */
	u_int flags;		/* E_F_* flags from EXCMDLIST. */
	u_int flagoff;		/* Offset for #, l and p commands. */
	int argc;		/* Count of file/word arguments. */
	char **argv;		/* List of file/word arguments. */
	char *command;		/* Command line, if parse locally. */
	char *plus;		/* '+' command word. */
	char *string;		/* String. */
	u_char buffer;		/* Named buffer. */
} EXCMDARG;

/* Macro to set up the structure. */
#define	SETCMDARG(s, _cmd, _addrcnt, _addr1, _addr2, _force, _arg) { \
	bzero(&s, sizeof(EXCMDARG)); \
	s.cmd = &cmds[_cmd]; \
	s.addrcnt = (_addrcnt); \
	s.addr1 = (_addr1); \
	s.addr2 = (_addr2); \
	if (_force) \
		s.flags |= E_FORCE; \
	s.argc = _arg ? 1 : 0; \
	s.argv = defcmdarg; \
	defcmdarg[0] = _arg; \
}
extern char *defcmdarg[2];

void	 ex __P((void));
char	*linespec __P((char *, EXCMDARG *));
int	 buildargv __P((char *, int, EXCMDARG *));

int	ex_abbr __P((EXCMDARG *));
int	ex_append __P((EXCMDARG *));
int	ex_args __P((EXCMDARG *));
int	ex_at __P((EXCMDARG *));
int	ex_bang __P((EXCMDARG *));
int	ex_cc __P((EXCMDARG *));
int	ex_cd __P((EXCMDARG *));
int	ex_cfile __P((char *, int));
int	ex_change __P((EXCMDARG *));
int	ex_cmd __P((char *));
int	ex_color __P((EXCMDARG *));
int	ex_copy __P((EXCMDARG *));
int	ex_cstring __P((char *, int, int));
int	ex_debug __P((EXCMDARG *));
int	ex_delete __P((EXCMDARG *));
int	ex_digraph __P((EXCMDARG *));
int	ex_edit __P((EXCMDARG *));
int	ex_equal __P((EXCMDARG *));
int	ex_errlist __P((EXCMDARG *));
int	ex_file __P((EXCMDARG *));
int	ex_global __P((EXCMDARG *));
int	ex_join __P((EXCMDARG *));
int	ex_list __P((EXCMDARG *));
int	ex_make __P((EXCMDARG *));
int	ex_map __P((EXCMDARG *));
int	ex_mark __P((EXCMDARG *));
int	ex_mkexrc __P((EXCMDARG *));
int	ex_move __P((EXCMDARG *));
int	ex_next __P((EXCMDARG *));
int	ex_number __P((EXCMDARG *));
int	ex_prev __P((EXCMDARG *));
int	ex_print __P((EXCMDARG *));
int	ex_put __P((EXCMDARG *));
int	ex_quit __P((EXCMDARG *));
int	ex_read __P((EXCMDARG *));
int	ex_readfp __P((char *, FILE *, MARK, long *));
void	ex_refresh __P((void));
int	ex_rew __P((EXCMDARG *));
int	ex_set __P((EXCMDARG *));
int	ex_shell __P((EXCMDARG *));
int	ex_shiftl __P((EXCMDARG *));
int	ex_shiftr __P((EXCMDARG *));
int	ex_source __P((EXCMDARG *));
int	ex_subagain __P((EXCMDARG *));
int	ex_substitute __P((EXCMDARG *));
int	ex_tag __P((EXCMDARG *));
int	ex_unabbr __P((EXCMDARG *));
int	ex_undo __P((EXCMDARG *));
int	ex_unmap __P((EXCMDARG *));
int	ex_validate __P((EXCMDARG *));
int	ex_version __P((EXCMDARG *));
int	ex_vglobal __P((EXCMDARG *));
int	ex_visual __P((EXCMDARG *));
int	ex_wq __P((EXCMDARG *));
int	ex_write __P((EXCMDARG *));
int	ex_writefp __P((char *, FILE *, MARK, MARK, int));
int	ex_xit __P((EXCMDARG *));
int	ex_yank __P((EXCMDARG *));
