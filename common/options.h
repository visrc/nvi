/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: options.h,v 5.2 1992/04/04 16:26:32 bostic Exp $ (Berkeley) $Date: 1992/04/04 16:26:32 $
 */

/* Offset macros. */
#define	ISFSET(option, flag)	(opts[(option)].flags & (flag))
#define	FSET(option, flag)	(opts[(option)].flags |= (flag))
#define	FUNSET(option, flag)	(opts[(option)].flags &= ~(flag))

/* Pointer macros. */
#define	ISFSETP(op, flag)	((op)->flags & (flag))
#define	FSETP(op, flag)		((op)->flags |= (flag))
#define	FUNSETP(op, flag)	((op)->flags &= ~(flag))

#define	ISSET(option) 		(ISFSET(option, OPT_1BOOL))
#define	SET(option) { \
	int __offset = option; \
	FUNSET(__offset, OPT_0BOOL); \
	FSET(__offset, OPT_1BOOL); \
}
#define	UNSET(option) { \
	int __offset = option; \
	FUNSET(__offset, OPT_1BOOL); \
	FSET(__offset, OPT_0BOOL); \
}

/*
 * First array slot is the current value, second and third are the low and
 * and high ends of the range.
 */
#define	NUM_CUR_VALUE	0
#define	NUM_MIN_VALUE	1
#define	NUM_MAX_VALUE	2
#define	LVAL(option) 		(((long *)opts[(option)].value)[NUM_CUR_VALUE])
#define	LVALP(op)		(((long *)(op)->value)[NUM_CUR_VALUE])
#define	MAXLVALP(op)		(((long *)(op)->value)[NUM_MAX_VALUE])
#define	MINLVALP(op)		(((long *)(op)->value)[NUM_MIN_VALUE])
#define	PVAL(option)		((char *)opts[(option)].value)
#define	PVALP(option)		((char *)((op)->value))

typedef struct _option {
	char	*name;		/* Name. */
	void	*value;		/* Value */

#define	OPT_0BOOL	0x001	/* Boolean (off). */
#define	OPT_1BOOL	0x002	/* Boolean (on). */
#define	OPT_NUM		0x004	/* Number. */
#define	OPT_STR		0x008	/* String. */
#define	OPT_TYPE	0x00f	/* Type mask. */

#define	OPT_ALLOCATED	0x010	/* Allocated space. */
#define	OPT_NOSAVE	0x040	/* Option should not be saved by mkexrc. */
#define	OPT_NOSET	0x080	/* Option can't be set. */
#define	OPT_REDRAW	0x100	/* Option affects the way text is displayed. */
#define	OPT_SET		0x200	/* Set (display for the user). */
	u_int	flags;
} OPTIONS;

extern OPTIONS opts[];

void	opts_dump __P((int));
void	opts_init __P((void));
void	opts_save __P((FILE *));
void	opts_set __P((char **));
