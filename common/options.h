/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: options.h,v 10.5 1996/02/04 19:00:41 bostic Exp $ (Berkeley) $Date: 1996/02/04 19:00:41 $
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

#define	OPT_GLOBAL	0x01		/* Option is global. */
#define	OPT_SELECTED	0x02		/* Selected for display. */
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
#define	OPT_NOSAVE	0x04		/* Option may not be unset. */
#define	OPT_NOUNSET	0x08		/* Mkexrc command doesn't save. */
	u_int8_t flags;
};

/*
 * Macros to retrieve boolean, integral and string option values, and to
 * set, clear and test boolean option values.  Some options (secure, lines,
 * columns, terminal type) are global in scope, and are therefore stored
 * in the global area.  The offset in the global options array is stored
 * in the screen's value field.  This is set up when the options are first
 * initialized.
 */
#define	O_V(sp, o, fld)							\
	(F_ISSET(&(sp)->opts[(o)], OPT_GLOBAL) ?			\
	    (sp)->gp->opts[(sp)->opts[(o)].o_cur.val].fld :		\
	    (sp)->opts[(o)].fld)
#define	O_SET(sp, o)		O_V(sp, o, o_cur.val) = 1
#define	O_CLR(sp, o)		O_V(sp, o, o_cur.val) = 0
#define	O_ISSET(sp, o)		O_V(sp, o, o_cur.val)
#define	O_D_SET(sp, o)		O_V(sp, o, o_def.val) = 1
#define	O_D_CLR(sp, o)		O_V(sp, o, o_def.val) = 0
#define	O_D_ISSET(sp, o)	O_V(sp, o, o_def.val)
#define	O_STR(sp, o)		O_V(sp, o, o_cur.str)
#define	O_D_STR(sp, o)		O_V(sp, o, o_def.str)
#define	O_VAL(sp, o)		O_V(sp, o, o_cur.val)
#define	O_D_VAL(sp, o)		O_V(sp, o, o_def.val)

#define	OG_VAL(gp, o)		(gp)->opts[(o)].o_cur.val
#define	OG_D_VAL(gp, o)		(gp)->opts[(o)].o_def.val
#define	OG_STR(gp, o)		(gp)->opts[(o)].o_cur.str
#define	OG_D_STR(gp, o)		(gp)->opts[(o)].o_def.str

/* Option argument to opts_dump(). */
enum optdisp { NO_DISPLAY, ALL_DISPLAY, CHANGED_DISPLAY, SELECT_DISPLAY };

/* Options array. */
extern OPTLIST const optlist[];

#include "options_define.h"
