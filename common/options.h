/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: options.h,v 8.3 1993/09/30 19:00:35 bostic Exp $ (Berkeley) $Date: 1993/09/30 19:00:35 $
 */

typedef struct _option {
	union {
		u_long	 val;		/* Value, boolean. */
		char	*str;		/* String. */
	} o_u;

#define	OPT_ALLOCATED	0x01		/* Allocated space. */
#define	OPT_SET		0x02		/* Set (display for the user). */
	u_int	flags;
} OPTION;

typedef struct _optlist {
	char	*name;			/* Name. */
					/* Change function. */
	int	(*func) __P((struct _scr *, struct _option *, char *, u_long));
					/* Type of object. */	
	enum { OPT_0BOOL, OPT_1BOOL, OPT_NUM, OPT_STR } type;

#define	OPT_NOSAVE	0x01		/* Option not saved by mkexrc. */
	u_int	 flags;
} OPTLIST;

/* Clear, set, test boolean options. */
#define	O_SET(sp, o)		(sp)->opts[(o)].o_u.val = 1
#define	O_CLR(sp, o)		(sp)->opts[(o)].o_u.val = 0
#define	O_ISSET(sp, o)		((sp)->opts[(o)].o_u.val)

/* Get option values. */
#define	O_VAL(sp, o)		(sp)->opts[(o)].o_u.val
#define	O_STR(sp, o)		(sp)->opts[(o)].o_u.str

/* Option routines. */
void	opts_dump __P((struct _scr *, int));
int	opts_init __P((struct _scr *));
int	opts_save __P((struct _scr *, FILE *));
int	opts_set __P((struct _scr *, char **));

/* Per-option change routines. */
int	f_columns __P((struct _scr *, struct _option *, char *, u_long));
int	f_keytime __P((struct _scr *, struct _option *, char *, u_long));
int	f_leftright __P((struct _scr *, struct _option *, char *, u_long));
int	f_lines __P((struct _scr *, struct _option *, char *, u_long));
int	f_list __P((struct _scr *, struct _option *, char *, u_long));
int	f_mesg __P((struct _scr *, struct _option *, char *, u_long));
int	f_modelines __P((struct _scr *, struct _option *, char *, u_long));
int	f_number __P((struct _scr *, struct _option *, char *, u_long));
int	f_paragraph __P((struct _scr *, struct _option *, char *, u_long));
int	f_readonly __P((struct _scr *, struct _option *, char *, u_long));
int	f_ruler __P((struct _scr *, struct _option *, char *, u_long));
int	f_section __P((struct _scr *, struct _option *, char *, u_long));
int	f_shiftwidth __P((struct _scr *, struct _option *, char *, u_long));
int	f_sidescroll __P((struct _scr *, struct _option *, char *, u_long));
int	f_tabstop __P((struct _scr *, struct _option *, char *, u_long));
int	f_tags __P((struct _scr *, struct _option *, char *, u_long));
int	f_term __P((struct _scr *, struct _option *, char *, u_long));
int	f_w300 __P((struct _scr *, struct _option *, char *, u_long));
int	f_w1200 __P((struct _scr *, struct _option *, char *, u_long));
int	f_w9600 __P((struct _scr *, struct _option *, char *, u_long));
int	f_window __P((struct _scr *, struct _option *, char *, u_long));
int	f_wrapmargin __P((struct _scr *, struct _option *, char *, u_long));
