/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: ex.h,v 5.3 1992/04/05 15:47:37 bostic Exp $ (Berkeley) $Date: 1992/04/05 15:47:37 $
 */

struct _cmdarg;
struct _cmdlist;

/* Ex command structure. */
typedef struct _cmdlist {
	char *name;			/* Command name. */
					/* Underlying function. */
	int (*fn) __P((struct _cmdarg *));

#define	E_ADDR1		0x0001		/* One address. */
#define	E_ADDR2		0x0002		/* Two address. */
#define	E_EXRCOK	0x0004		/* OK in a .exrc file. */
#define	E_FORCE		0x0008		/* ! flag. */
#define	E_F_CARAT	0x0010		/* ^ flag. */
#define	E_F_DASH	0x0020		/* - flag. */
#define	E_F_DOT		0x0040		/* . flag. */
#define	E_F_HASH	0x0080		/* # flag. */
#define	E_F_LIST	0x0100		/* l flag. */
#define	E_F_PLUS	0x0200		/* + flag. */
#define	E_F_PRINT	0x0400		/* p flag. */
#define	E_F_RIGHT	0x0800		/* >> */
#define	E_NL		0x1000		/* Newline first if not MODE_EX mode.*/
#define	E_NOPERM	0x2000		/* Permission denied for now. */
#define	E_ZERO		0x4000		/* 0 is a legal (first) address.*/
	u_int flags;
	char *syntax;			/* Syntax script. */
	char *usage;			/* Usage line. */
} CMDLIST;
extern CMDLIST cmds[];			/* List of ex commands. */

/*
 * Structure passed around to functions implementing ex commands.
 */
typedef struct _cmdarg {
	struct _cmdlist *cmd;	/* Command entry in command table. */
	int addrcnt;		/* Number of addresses (0, 1 or 2). */
	MARK addr1;		/* 1st address. */
	MARK addr2;		/* 2nd address. */
	MARK lineno;		/* Line number. */
	u_int flags;		/* E_F_* flags from CMDLIST. */
	int argc;		/* Count of file/word arguments. */
	char **argv;		/* List of file/word arguments. */
	char *command;		/* Command line, if parse locally. */
	char *plus;		/* '+' command word. */
	char *string;		/* String. */
	u_char buffer;		/* Named buffer. */
} CMDARG;

/* Macro to set up the structure. */
#define	SETCMDARG(s, _cmd, _addrcnt, _addr1, _addr2, _force, _arg) { \
	bzero(&s, sizeof(CMDARG)); \
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

void	ex __P((void));
int	excmd __P((char *));
int	exfile __P((char *, int));
void	exrefresh __P((void));
int	exstring __P((char *, int));

int	ex_abbr __P((CMDARG *));
int	ex_append __P((CMDARG *));
int	ex_args __P((CMDARG *));
int	ex_at __P((CMDARG *));
int	ex_bang __P((CMDARG *));
int	ex_cc __P((CMDARG *));
int	ex_cd __P((CMDARG *));
int	ex_change __P((CMDARG *));
int	ex_color __P((CMDARG *));
int	ex_copy __P((CMDARG *));
int	ex_debug __P((CMDARG *));
int	ex_delete __P((CMDARG *));
int	ex_digraph __P((CMDARG *));
int	ex_edit __P((CMDARG *));
int	ex_equal __P((CMDARG *));
int	ex_errlist __P((CMDARG *));
int	ex_file __P((CMDARG *));
int	ex_global __P((CMDARG *));
int	ex_join __P((CMDARG *));
int	ex_list __P((CMDARG *));
int	ex_make __P((CMDARG *));
int	ex_map __P((CMDARG *));
int	ex_mark __P((CMDARG *));
int	ex_mkexrc __P((CMDARG *));
int	ex_move __P((CMDARG *));
int	ex_next __P((CMDARG *));
int	ex_number __P((CMDARG *));
int	ex_prev __P((CMDARG *));
int	ex_print __P((CMDARG *));
int	ex_put __P((CMDARG *));
int	ex_quit __P((CMDARG *));
int	ex_read __P((CMDARG *));
int	ex_rew __P((CMDARG *));
int	ex_set __P((CMDARG *));
int	ex_shell __P((CMDARG *));
int	ex_shiftl __P((CMDARG *));
int	ex_shiftr __P((CMDARG *));
int	ex_source __P((CMDARG *));
int	ex_subagain __P((CMDARG *));
int	ex_substitute __P((CMDARG *));
int	ex_tag __P((CMDARG *));
int	ex_unabbr __P((CMDARG *));
int	ex_undo __P((CMDARG *));
int	ex_unmap __P((CMDARG *));
int	ex_validate __P((CMDARG *));
int	ex_version __P((CMDARG *));
int	ex_vglobal __P((CMDARG *));
int	ex_visual __P((CMDARG *));
int	ex_wq __P((CMDARG *));
int	ex_write __P((CMDARG *));
int	ex_xit __P((CMDARG *));
int	ex_yank __P((CMDARG *));

char	*linespec __P((char *, CMDARG *));
