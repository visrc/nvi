/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: options.h,v 9.4 1994/12/16 20:04:37 bostic Exp $ (Berkeley) $Date: 1994/12/16 20:04:37 $
 */

struct _option {
	union {
		u_long	 val;		/* Value or boolean. */
		char	*str;		/* String. */
	} o_cur;
	union {
		u_long	 val;		/* Value or boolean. */
		char	*str;		/* String. */
	} o_def;

#define	OPT_SELECTED	0x01		/* Selected for display. */
	u_int8_t flags;
};

struct _optlist {
	char	*name;			/* Name. */
					/* Change function. */
	int	(*func) __P((SCR *, OPTION *, char *, u_long));
					/* Type of object. */
	enum { OPT_0BOOL, OPT_1BOOL, OPT_NUM, OPT_STR } type;

#define	OPT_ADISP	0x01		/* Always display the option. */
#define	OPT_NDISP	0x02		/* Never display the option. */
#define	OPT_NOSAVE	0x04		/* Mkexrc command doesn't save. */
	u_int8_t flags;
};

/* Clear, set, test boolean options. */
#define	O_SET(sp, o)		(sp)->opts[(o)].o_cur.val = 1
#define	O_CLR(sp, o)		(sp)->opts[(o)].o_cur.val = 0
#define	O_ISSET(sp, o)		((sp)->opts[(o)].o_cur.val)

#define	O_D_SET(sp, o)		(sp)->opts[(o)].o_def.val = 1
#define	O_D_CLR(sp, o)		(sp)->opts[(o)].o_def.val = 0
#define	O_D_ISSET(sp, o)	((sp)->opts[(o)].o_def.val)

/* Get option values. */
#define	O_STR(sp, o)		(sp)->opts[(o)].o_cur.str
#define	O_VAL(sp, o)		(sp)->opts[(o)].o_cur.val

#define	O_D_STR(sp, o)		(sp)->opts[(o)].o_def.str
#define	O_D_VAL(sp, o)		(sp)->opts[(o)].o_def.val

/* Option routines. */
u_long	 baud_from_bval __P((SCR *));

int	opts_copy __P((SCR *, SCR *));
void	opts_free __P((SCR *));
int	opts_init __P((SCR *, int *));
int	opts_save __P((SCR *, FILE *));
int	opts_set __P((SCR *, ARGS *[], int, char *));

enum optdisp { NO_DISPLAY, ALL_DISPLAY, CHANGED_DISPLAY, SELECT_DISPLAY };
void	opts_dump __P((SCR *, enum optdisp));

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
int	f_msgcat __P((SCR *, OPTION *, char *, u_long));
int	f_noprint __P((SCR *, OPTION *, char *, u_long));
int	f_number __P((SCR *, OPTION *, char *, u_long));
int	f_octal __P((SCR *, OPTION *, char *, u_long));
int	f_paragraph __P((SCR *, OPTION *, char *, u_long));
int	f_print __P((SCR *, OPTION *, char *, u_long));
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
