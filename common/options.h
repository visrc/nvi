/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: options.h,v 5.11 1993/03/25 15:00:24 bostic Exp $ (Berkeley) $Date: 1993/03/25 15:00:24 $
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

#define	LVAL(option) 		(*(long *)opts[(option)].value)
#define	LVALP(op)		(*(long *)(op)->value)
#define	PVAL(option)		((u_char *)opts[(option)].value)

typedef struct _option {
	char	*name;			/* Name. */
	void	*value;			/* Value. */
					/* Initialization function. */
	int	(*func) __P((SCR *, void *, char *));

#define	OPT_0BOOL	0x001		/* Boolean (off). */
#define	OPT_1BOOL	0x002		/* Boolean (on). */
#define	OPT_NUM		0x004		/* Number. */
#define	OPT_STR		0x008		/* String. */
#define	OPT_TYPE	0x00f		/* Type mask. */

#define	OPT_ALLOCATED	0x010		/* Allocated space. */
#define	OPT_NOSAVE	0x020		/* Option not saved by mkexrc. */
#define	OPT_NOSET	0x040		/* Option can't be set. */
#define	OPT_REDRAW	0x080		/* Option requires a redraw. */
#define	OPT_SET		0x100		/* Set (display for the user). */
	u_int	flags;
} OPTIONS;

extern OPTIONS opts[];

/* Option routines. */
void	opts_dump __P((SCR *, int));
int	opts_init __P((SCR *));
int	opts_save __P((SCR *, FILE *));
int	opts_set __P((SCR *, char **));

/* Per-option routines. */
int	f_columns __P((SCR *, void *, char *));
int	f_flash __P((SCR *, void *, char *));
int	f_keytime __P((SCR *, void *, char *));
int	f_leftright __P((SCR *, void *, char *));
int	f_lines __P((SCR *, void *, char *));
int	f_list __P((SCR *, void *, char *));
int	f_mesg __P((SCR *, void *, char *));
int	f_modelines __P((SCR *, void *, char *));
int	f_ruler __P((SCR *, void *, char *));
int	f_shiftwidth __P((SCR *, void *, char *));
int	f_sidescroll __P((SCR *, void *, char *));
int	f_tabstop __P((SCR *, void *, char *));
int	f_tags __P((SCR *, void *, char *));
int	f_term __P((SCR *, void *, char *));
int	f_wrapmargin __P((SCR *, void *, char *));
