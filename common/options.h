/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: options.h,v 8.21 1994/07/22 19:48:56 bostic Exp $ (Berkeley) $Date: 1994/07/22 19:48:56 $
 */

struct _option {
	union {
		u_long	 val;		/* Value or boolean. */
		char	*str;		/* String. */
	} o_u;
	size_t	len;			/* String length. */

#define	OPT_ALLOCATED	0x01		/* Allocated space. */
#define	OPT_SELECTED	0x02		/* Selected for display. */
#define	OPT_SET		0x04		/* Set (display for the user). */
	u_char	flags;
};

struct _optlist {
	char	*name;			/* Name. */
					/* Change function. */
	int	(*func) __P((SCR *, OPTION *, char *, u_long));
					/* Type of object. */
	enum { OPT_0BOOL, OPT_1BOOL, OPT_NUM, OPT_STR } type;

#define	OPT_NEVER	0x01		/* Never display the option. */
#define	OPT_NOSAVE	0x02		/* Mkexrc command doesn't save. */
#define	OPT_NOSTR	0x04		/* String that takes a "no". */
	u_int	 flags;
};

/* Clear, set, test boolean options. */
#define	O_SET(sp, o)		(sp)->opts[(o)].o_u.val = 1
#define	O_CLR(sp, o)		(sp)->opts[(o)].o_u.val = 0
#define	O_ISSET(sp, o)		((sp)->opts[(o)].o_u.val)

/* Get option values. */
#define	O_LEN(sp, o)		(sp)->opts[(o)].len
#define	O_STR(sp, o)		(sp)->opts[(o)].o_u.str
#define	O_VAL(sp, o)		(sp)->opts[(o)].o_u.val

/* Option routines. */
enum optdisp { NO_DISPLAY, ALL_DISPLAY, CHANGED_DISPLAY, SELECT_DISPLAY };

int	opts_copy __P((SCR *, SCR *));
void	opts_dump __P((SCR *, enum optdisp));
void	opts_free __P((SCR *));
int	opts_init __P((SCR *));
int	opts_save __P((SCR *, FILE *));
int	opts_set __P((SCR *, char *, ARGS *[]));

/* Per-option change routines. */
int	f_altwerase __P((SCR *, OPTION *, char *, u_long));
int	f_cdpath __P((SCR *, OPTION *, char *, u_long));
int	f_columns __P((SCR *, OPTION *, char *, u_long));
int	f_leftright __P((SCR *, OPTION *, char *, u_long));
int	f_lines __P((SCR *, OPTION *, char *, u_long));
int	f_lisp __P((SCR *, OPTION *, char *, u_long));
int	f_list __P((SCR *, OPTION *, char *, u_long));
int	f_mesg __P((SCR *, OPTION *, char *, u_long));
int	f_modeline __P((SCR *, OPTION *, char *, u_long));
int	f_number __P((SCR *, OPTION *, char *, u_long));
int	f_octal __P((SCR *, OPTION *, char *, u_long));
int	f_paragraph __P((SCR *, OPTION *, char *, u_long));
int	f_readonly __P((SCR *, OPTION *, char *, u_long));
int	f_section __P((SCR *, OPTION *, char *, u_long));
int	f_shiftwidth __P((SCR *, OPTION *, char *, u_long));
int	f_sourceany __P((SCR *, OPTION *, char *, u_long));
int	f_tabstop __P((SCR *, OPTION *, char *, u_long));
int	f_tags __P((SCR *, OPTION *, char *, u_long));
int	f_term __P((SCR *, OPTION *, char *, u_long));
int	f_ttywerase __P((SCR *, OPTION *, char *, u_long));
int	f_w1200 __P((SCR *, OPTION *, char *, u_long));
int	f_w300 __P((SCR *, OPTION *, char *, u_long));
int	f_w9600 __P((SCR *, OPTION *, char *, u_long));
int	f_window __P((SCR *, OPTION *, char *, u_long));
