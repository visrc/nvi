/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: ex.h,v 5.19 1992/10/10 13:58:03 bostic Exp $ (Berkeley) $Date: 1992/10/10 13:58:03 $
 */

#include "exf.h"
#include "mark.h"

struct excmdarg;

/* Ex command structure. */
typedef struct {
	char *name;			/* Command name. */
					/* Underlying function. */
	int (*fn) __P((struct excmdarg *));

#define	E_ADDR1		0x00001		/* One address. */
#define	E_ADDR2		0x00002		/* Two address. */
#define	E_ADDR2_ALL	0x00004		/* Zero/two addresses; zero == all. */
#define	E_ADDR2_NONE	0x00008		/* Zero/two addresses; zero == none. */
#define	E_APPEND	0x00010		/* >> */
#define	E_EXRCOK	0x00020		/* OK in a .exrc file. */
#define	E_FORCE		0x00040		/*  ! */

#define	E_F_CARAT	0x00080		/*  ^ flag. */
#define	E_F_DASH	0x00100		/*  - flag. */
#define	E_F_DOT		0x00200		/*  . flag. */
#define	E_F_HASH	0x00400		/*  # flag. */
#define	E_F_LIST	0x00800		/*  l flag. */
#define	E_F_PLUS	0x01000		/*  + flag. */
#define	E_F_PRINT	0x02000		/*  p flag. */
#define	E_F_MASK	0x03f80		/* Flag mask. */

#define	E_NL		0x04000		/* Newline first if not MODE_EX mode.*/
#define	E_NOPERM	0x08000		/* Permission denied for now. */
#define	E_SETLAST	0x10000		/* Reset last command. */
#define	E_ZERO		0x20000		/* 0 is a legal (first) address.*/
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
	u_int flags;		/* E_F_* flags from EXCMDLIST. */
	int argc;		/* Count of file/word arguments. */
	u_char **argv;		/* List of file/word arguments. */
	u_char *command;	/* Command line, if parse locally. */
	u_char *plus;		/* '+' command word. */
	u_char *string;		/* String. */
	int buffer;		/* Named buffer. */
} EXCMDARG;

extern u_char *defcmdarg[2];	/* Default array. */

/* Macro to set up the structure. */
#define	SETCMDARG(s, _cmd, _addrcnt, _lno1, _lno2, _force, _arg) { \
	bzero(&s, sizeof(EXCMDARG)); \
	s.cmd = &cmds[_cmd]; \
	s.addrcnt = (_addrcnt); \
	s.addr1.lno = (_lno1); \
	s.addr2.lno = (_lno2); \
	s.addr1.cno = s.addr2.cno = 1; \
	if (_force) \
		s.flags |= E_FORCE; \
	s.argc = _arg ? 1 : 0; \
	s.argv = defcmdarg; \
	defcmdarg[0] = (u_char *)_arg; \
}

/*
 * Macro to do standard force/write/save stuff.
 *
 * If autowrite set, write the file; otherwise warn the user if the file has
 * been modified but not written.
 */
#define	DEFMODSYNC { \
	if (ISSET(O_AUTOWRITE)) { \
		if (file_sync(curf, 0)) \
			return (1); \
	} else if (ISSET(O_WARN) && curf->flags & F_MODIFIED) { \
		msg("%s has been modified but not written.", curf->name); \
		return (1); \
	} \
}

/* Control character. */
#define	ctrl(ch)	((ch) & 0x1f)

void	 ex __P((void));
u_char	*linespec __P((u_char *, EXCMDARG *));
int	 buildargv __P((u_char *, int, EXCMDARG *));

int	ex_abbr __P((EXCMDARG *));
int	ex_append __P((EXCMDARG *));
int	ex_args __P((EXCMDARG *));
int	ex_at __P((EXCMDARG *));
int	ex_bang __P((EXCMDARG *));
int	ex_cc __P((EXCMDARG *));
int	ex_cd __P((EXCMDARG *));
int	ex_cfile __P((char *, int));
int	ex_change __P((EXCMDARG *));
int	ex_cmd __P((u_char *));
int	ex_color __P((EXCMDARG *));
int	ex_copy __P((EXCMDARG *));
int	ex_cstring __P((u_char *, int, int));
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
int	ex_readfp __P((char *, FILE *, MARK *, recno_t *));
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
int	ex_writefp __P((char *, FILE *, MARK *, MARK *, int));
int	ex_xit __P((EXCMDARG *));
int	ex_yank __P((EXCMDARG *));
