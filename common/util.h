/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: util.h,v 8.1 1994/10/09 14:02:14 bostic Exp $ (Berkeley) $Date: 1994/10/09 14:02:14 $
 */

/*
 * XXX
 * MIN/MAX have traditionally been in <sys/param.h>.  Don't
 * try to get them from there, it's just not worth the effort.
 */
#ifndef	MAX
#define	MAX(_a,_b)	((_a)<(_b)?(_b):(_a))
#endif
#ifndef	MIN
#define	MIN(_a,_b)	((_a)<(_b)?(_a):(_b))
#endif

/* Function prototypes that don't seem to belong anywhere else. */
int	 nonblank __P((SCR *, EXF *, recno_t, size_t *));
void	 set_alt_name __P((SCR *, char *));
char	*tail __P((char *));
CHAR_T	*v_strdup __P((SCR *, const CHAR_T *, size_t));
void	 vi_putchar __P((int));

int	 add_slong __P((SCR *, long, long, char *, char *));
int	 add_uslong __P((SCR *, u_long, u_long, char *));
int	 get_slong __P((SCR *,
	    long, long *, int, char *, char **, char *, char *));
int	 get_uslong __P((SCR *, u_long, u_long *, char *, char **, char *));

#ifdef DEBUG
void	TRACE __P((SCR *, const char *, ...));
#endif

/* Digraphs (not currently real). */
int	digraph __P((SCR *, int, int));
int	digraph_init __P((SCR *));
void	digraph_save __P((SCR *, int));
