/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: common.h,v 5.35 1993/02/14 18:02:44 bostic Exp $ (Berkeley) $Date: 1993/02/14 18:02:44 $
 */

#include <limits.h>		/* XXX */
#include <db.h>

/*
 * Most of the arrays, names, etc. in ex/vi are u_char's, since we want
 * it to be 8-bit clean.  Several of the C-library routines take char *'s,
 * though.  The following #defines try to handle this problem.
 *
 * XXX
 * I think that the correct approach is to have a set of data types as
 * follows:
 *	typedef char 		CFNAME	(file name character)
 *	typedef unsigned char	CDATA	(data character)
 *	typedef unsigned char	CINPUT	(input, command character)
 */
#define	UATOI(a)		atoi((char *)(a))
#define	USTRCMP(a, b)		strcmp((char *)(a), (char *)(b))
#define	USTRDUP(a)		(u_char *)strdup((char *)(a))
#define	USTRLEN(a)		strlen((char *)(a))
#define	USTRNCMP(a, b, n)	strncmp((char *)(a), (char *)(b), n)
#define	USTRPBRK(a, b)		(u_char *)strpbrk((char *)(a), (char *)(b))
#define	USTRRCHR(a, b)		(u_char *)strrchr((char *)(a), b)
#define	USTRSEP(a, b)		(u_char *)strsep((char **)(a), b)
#define	USTRTOL(a, b, c)	strtol((char *)(a), (char **)(b), c)

/* Confirmation stuff. */
#define	CONFIRMCHAR	'y'		/* Make change character. */
#define	QUITCHAR	'q'		/* Quit character. */
enum confirmation { YES, NO, QUIT };

#ifndef DEFFILEMODE			/* Default file permissions. */
#define	DEFFILEMODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif

typedef void (*sig_ret_t) __P((int));	/* Type of signal function. */

/* Buffer allocation. */
#define	BINC(lp, llen, nlen) {						\
	if ((nlen) > llen &&						\
	    binc(&(lp), &(llen), nlen))					\
		return (1);						\
}
int	binc __P((u_char **, size_t *, size_t));

/* Visual bell. */
extern char *VB;

/* Editor mode. */
enum editmode {MODE_EX, MODE_VI, MODE_QUIT};
extern enum editmode mode;

/* Messages. */
extern int msgcnt;		/* Current message count. */
extern char *msglist[];		/* Message list. */
void	msg __P((const char *, ...));
void	msg_eflush __P((void));

#ifdef DEBUG
void	TRACE __P((const char *, ...));
#endif

/* Digraphs. */
int	digraph __P((int, int));
void	digraph_init __P((void));
void	digraph_save __P((int));

/* Signals. */
void	onhup __P((int));
void	onwinch __P((int));
void	trapint __P((int));

/* Random stuff. */
void	 __putchar __P((int));
void	 bell __P((void));
int	 nonblank __P((recno_t, size_t *));
char	*tail __P((char *));

/* Display characters. */
#define	CHARNAME(c)	(asciiname[c & 0xff])
extern char *asciiname[UCHAR_MAX + 1];
extern u_char asciilen[UCHAR_MAX + 1];

int	ex __P((void));
int	vi __P((void));
