/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: util.h,v 9.8 1995/01/30 12:05:00 bostic Exp $ (Berkeley) $Date: 1995/01/30 12:05:00 $
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

/* Offset to next column of stop size, e.g. tab offsets. */
#define	COL_OFF(c, stop)	((stop) - ((c) % (stop)))

/*
 * Number handling defines and protoypes.
 *
 * NNFITS:	test for addition of two negative numbers under a limit
 * NPFITS:	test for addition of two positive numbers under a limit
 * NADD_SLONG:	test for addition of two signed longs
 * NADD_USLONG:	test for addition of two unsigned longs
 */
enum nresult { NUM_ERR, NUM_OK, NUM_OVER, NUM_UNDER };
#define	NNFITS(min, cur, add)						\
	(((long)(min)) - (cur) <= (add))
#define	NPFITS(max, cur, add)						\
	(((unsigned long)(max)) - (cur) >= (add))
#define	NADD_SLONG(sp, v1, v2)						\
	((v1) < 0 ?							\
	    ((v2) < 0 &&						\
	    NNFITS(LONG_MIN, (v1), (v2))) ? NUM_UNDER : NUM_OK :	\
	 (v1) > 0 ?							\
	    (v2) > 0 &&							\
	    NPFITS(LONG_MAX, (v1), (v2)) ? NUM_OK : NUM_OVER :		\
	 NUM_OK)
#define	NADD_USLONG(sp, v1, v2)						\
	(NPFITS(ULONG_MAX, (v1), (v2)) ? NUM_OK : NUM_OVER)
enum nresult nget_slong __P((SCR *, long *, char *, char **, int));
enum nresult nget_uslong __P((SCR *, u_long *, char *, char **, int));

/* Digraphs (not currently real). */
int	digraph __P((SCR *, int, int));
int	digraph_init __P((SCR *));
void	digraph_save __P((SCR *, int));

/* Function prototypes that don't seem to belong anywhere else. */
int	 nonblank __P((SCR *, recno_t, size_t *));
void	 set_alt_name __P((SCR *, char *));
char	*tail __P((char *));
int	 vi_main __P((int, char *[], int (*)(SCR *, int)));
void	 vi_putchar __P((int));

#ifdef DEBUG
void	TRACE __P((SCR *, const char *, ...));
#endif
