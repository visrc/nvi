/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: options.h,v 10.2 1995/05/18 15:36:51 bostic Exp $ (Berkeley) $Date: 1995/05/18 15:36:51 $
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

/* Option argument to opts_dump(). */
enum optdisp { NO_DISPLAY, ALL_DISPLAY, CHANGED_DISPLAY, SELECT_DISPLAY };

#include "options_define.h"
