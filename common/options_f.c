/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: options_f.c,v 10.3 1995/06/09 12:47:54 bostic Exp $ (Berkeley) $Date: 1995/06/09 12:47:54 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "../ex/tag.h"

static int	opt_dup __P((SCR *, int, char *));
static int	prset __P((SCR *, CHAR_T *, int, int));

#define	DECL(f)								\
	int								\
	f(sp, op, str, val)						\
		SCR *sp;						\
		OPTION *op;						\
		char *str;						\
		u_long val;
#define	CALL(f)								\
	f(sp, op, str, val)

#define	turnoff	val

/*
 * PUBLIC: int f_altwerase __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_altwerase)
{
	if (turnoff)
		O_CLR(sp, O_ALTWERASE);
	else {
		O_SET(sp, O_ALTWERASE);
		O_CLR(sp, O_TTYWERASE);
	}
	return (0);
}

/*
 * PUBLIC: int f_cdpath __P((SCR *, OPTION *, char *, u_long)); 
 */
DECL(f_cdpath)
{
	return (opt_dup(sp, O_CDPATH, str));
}

/*
 * PUBLIC: int f_columns __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_columns)
{
	/* Validate the number. */
	if (val < MINIMUM_SCREEN_COLS) {
		msgq(sp, M_ERR, "040|Screen columns too small, less than %d",
		    MINIMUM_SCREEN_COLS);
		return (1);
	}

	/*
	 * !!!
	 * It's not uncommon for allocation of huge chunks of memory to cause
	 * core dumps on various systems.  So, we prune out numbers that are
	 * "obviously" wrong.  Vi will not work correctly if it has the wrong
	 * number of lines/columns for the screen, but at least we don't drop
	 * core.
	 */
#define	MAXIMUM_SCREEN_COLS	500
	if (val > MAXIMUM_SCREEN_COLS) {
		msgq(sp, M_ERR, "041|Screen columns too large, greater than %d",
		    MAXIMUM_SCREEN_COLS);
		return (1);
	}

	/* This is expensive, don't do it unless it's necessary. */
	if (O_VAL(sp, O_COLUMNS) == val)
		return (0);

	/* Set the value. */
	O_VAL(sp, O_COLUMNS) =  val;
	return (0);
}

/*
 * PUBLIC: int f_extended __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_extended)
{
	if (turnoff)
		O_CLR(sp, O_EXTENDED);
	else
		O_SET(sp, O_EXTENDED);
	F_SET(sp, S_RE_RECOMPILE);
	return (0);
}

/*
 * PUBLIC: int f_ignorecase __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_ignorecase)
{
	if (turnoff)
		O_CLR(sp, O_IGNORECASE);
	else
		O_SET(sp, O_IGNORECASE);
	F_SET(sp, S_RE_RECOMPILE);
	return (0);
}

/*
 * PUBLIC: int f_leftright __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_leftright)
{
	if (turnoff)
		O_CLR(sp, O_LEFTRIGHT);
	else
		O_SET(sp, O_LEFTRIGHT);
	F_SET(sp, S_SCR_REFORMAT);
	return (0);
}

/*
 * PUBLIC: int f_lines __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_lines)
{
	/* Validate the number. */
	if (val < MINIMUM_SCREEN_ROWS) {
		msgq(sp, M_ERR, "042|Screen lines too small, less than %d",
		    MINIMUM_SCREEN_ROWS);
		return (1);
	}

	/*
	 * !!!
	 * It's not uncommon for allocation of huge chunks of memory to cause
	 * core dumps on various systems.  So, we prune out numbers that are
	 * "obviously" wrong.  Vi will not work correctly if it has the wrong
	 * number of lines/columns for the screen, but at least we don't drop
	 * core.
	 */
#define	MAXIMUM_SCREEN_ROWS	500
	if (val > MAXIMUM_SCREEN_ROWS) {
		msgq(sp, M_ERR, "043|Screen lines too large, greater than %d",
		    MAXIMUM_SCREEN_ROWS);
		return (1);
	}

	/* This is expensive, don't do it unless it's necessary. */
	if (O_VAL(sp, O_LINES) == val)
		return (0);

	/*
	 * Set the value, and the related scroll value.  If no window
	 * value set, set a new default window.
	 */
	if ((O_VAL(sp, O_LINES) = val) == 1) {
		sp->defscroll = 1;

		if (O_VAL(sp, O_WINDOW) == O_D_VAL(sp, O_WINDOW) ||
		    O_VAL(sp, O_WINDOW) > val)
			O_VAL(sp, O_WINDOW) = O_D_VAL(sp, O_WINDOW) = 1;
	} else {
		sp->defscroll = (val - 1) / 2;

		if (O_VAL(sp, O_WINDOW) == O_D_VAL(sp, O_WINDOW) ||
		    O_VAL(sp, O_WINDOW) > val)
			O_VAL(sp, O_WINDOW) = O_D_VAL(sp, O_WINDOW) = val - 1;
	}
	return (0);
}

/*
 * PUBLIC: int f_lisp __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_lisp)
{
	msgq(sp, M_ERR, "044|The lisp option is not implemented");
	return (0);
}

/*
 * PUBLIC: int f_list __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_list)
{
	if (turnoff)
		O_CLR(sp, O_LIST);
	else
		O_SET(sp, O_LIST);

	F_SET(sp, S_SCR_REFORMAT);
	return (0);
}

/*
 * PUBLIC: int f_mesg __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_mesg)
{
	struct stat sb;
	char *tty;

	/* Find the tty. */
	if ((tty = ttyname(STDERR_FILENO)) == NULL) {
		msgq(sp, M_SYSERR, "stderr");
		return (1);
	}

	/* Save the tty mode for later; only save it once. */
	if (!F_ISSET(sp->gp, G_SETMODE)) {
		F_SET(sp->gp, G_SETMODE);
		if (stat(tty, &sb) < 0) {
			msgq(sp, M_SYSERR, "%s", tty);
			return (1);
		}
		sp->gp->origmode = sb.st_mode;
	}

	if (turnoff) {
		if (chmod(tty, sp->gp->origmode & ~S_IWGRP) < 0) {
			msgq(sp, M_SYSERR,
			    "045|messages not turned off: %s", tty);
			return (1);
		}
		O_CLR(sp, O_MESG);
	} else {
		if (chmod(tty, sp->gp->origmode | S_IWGRP) < 0) {
			msgq(sp, M_SYSERR,
			    "046|messages not turned on: %s", tty);
			return (1);
		}
		O_SET(sp, O_MESG);
	}
	return (0);
}

/*
 * f_modeline --
 *	This has been documented in historical systems as both "modeline"
 *	and as "modelines".  Regardless of the name, this option represents
 *	a security problem of mammoth proportions, not to mention a stunning
 *	example of what your intro CS professor referred to as the perils of
 *	mixing code and data.  Don't add it, or I will kill you.
 *
 *
 * PUBLIC: int f_modeline __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_modeline)
{
	if (!turnoff)
		msgq(sp, M_ERR, "047|The modeline(s) option may never be set");
	return (0);
}

/*
 * PUBLIC: int f_msgcat __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_msgcat)
{
	(void)msg_open(sp, str);
	return (opt_dup(sp, O_MSGCAT, str));
}

/*
 * PUBLIC: int f_number __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_number)
{
	if (turnoff)
		O_CLR(sp, O_NUMBER);
	else
		O_SET(sp, O_NUMBER);

	F_SET(sp, S_SCR_REFORMAT);
	return (0);
}

/*
 * PUBLIC: int f_octal __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_octal)
{
	if (turnoff)
		O_CLR(sp, O_OCTAL);
	else
		O_SET(sp, O_OCTAL);

	/* Reinitialize the key fast lookup table. */
	v_key_ilookup(sp);

	/* Reformat the screen. */
	F_SET(sp, S_SCR_REFORMAT);
	return (0);
}

/*
 * PUBLIC: int f_paragraph __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_paragraph)
{
	if (strlen(str) & 1) {
		msgq(sp, M_ERR,
		    "048|The paragraph option must be in two character groups");
		return (1);
	}
	return (opt_dup(sp, O_PARAGRAPHS, str));
}

/*
 * PUBLIC: int f_noprint __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_noprint)
{
	return (prset(sp, str, O_NOPRINT, O_PRINT));
}

/*
 * PUBLIC: int f_print __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_print)
{
	return (prset(sp, str, O_PRINT, O_NOPRINT));
}

static int
prset(sp, str, set_index, unset_index)
	SCR *sp;
	CHAR_T *str;
	int set_index, unset_index;
{
	CHAR_T *p, *s, *t;
	size_t len;

	/* Delete characters from the unset index edit option. */
	if ((p = O_STR(sp, unset_index)) != NULL)
		for (s = str; *s != NULL; ++s) {
			for (p = t = O_STR(sp, unset_index); *p != '\0'; ++p)
				if (*p != *s)
					*t++ = *p;
			*t = '\0';
		}

	/*
	 * Create a new copy of the set_index edit option, and integrate
	 * the new characters from str into it, avoiding any duplication.
	 */
	len = strlen(str);
	if (O_STR(sp, set_index) != NULL)
		len += strlen(O_STR(sp, set_index));
	MALLOC_RET(sp, p, CHAR_T *, (len + 1) * sizeof(CHAR_T));
	if (O_STR(sp, set_index) != NULL)
		MEMMOVE(p, O_STR(sp, set_index), len + 1);
	else
		p[0] = '\0';
	for (s = str; *s != NULL; ++s) {
		for (t = p; *t != '\0'; ++t)
			if (*t == *s)
				break;
		if (*t == '\0') {
			*t = *s;
			*++t = '\0';
		}
	}
	if (O_STR(sp, set_index) != NULL)
		free(O_STR(sp, set_index));
	O_STR(sp, set_index) = p;

	/* Reinitialize the key fast lookup table. */
	v_key_ilookup(sp);

	/* Reformat the screen. */
	F_SET(sp, S_SCR_REFORMAT);
	return (0);
}

/*
 * PUBLIC: int f_readonly __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_readonly)
{
	if (turnoff) {
		O_CLR(sp, O_READONLY);
		if (sp->frp != NULL)
			F_CLR(sp->frp, FR_RDONLY);
	} else {
		O_SET(sp, O_READONLY);
		if (sp->frp != NULL)
			F_SET(sp->frp, FR_RDONLY);
	}
	return (0);
}

/*
 * PUBLIC: int f_section __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_section)
{
	if (strlen(str) & 1) {
		msgq(sp, M_ERR,
		    "049|The section option must be in two character groups");
		return (1);
	}
	return (opt_dup(sp, O_SECTIONS, str));
}

/*
 * PUBLIC: int f_shiftwidth __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_shiftwidth)
{
	if (val == 0) {
		msgq(sp, M_ERR, "050|The shiftwidth may not be set to 0");
		return (1);
	}
	O_VAL(sp, O_SHIFTWIDTH) = val;
	return (0);
}

/*
 * f_sourceany --
 *	Historic vi, on startup, source'd $HOME/.exrc and ./.exrc, if they
 *	were owned by the user.  The sourceany option was an undocumented
 *	feature of historic vi which permitted the startup source'ing of
 *	.exrc files the user didn't own.  This is an obvious security problem,
 *	and we ignore the option.
 *
 * PUBLIC: int f_sourceany __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_sourceany)
{
	if (!turnoff)
		msgq(sp, M_ERR, "051|The sourceany option may never be set");
	return (0);
}

/*
 * PUBLIC: int f_tabstop __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_tabstop)
{
	if (val == 0) {
		msgq(sp, M_ERR, "052|Tab stops may not be set to 0");
		return (1);
	}
	O_VAL(sp, O_TABSTOP) = val;

	F_SET(sp, S_SCR_REFORMAT);
	return (0);
}

/*
 * PUBLIC: int f_tags __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_tags)
{
	return (opt_dup(sp, O_TAGS, str));
}

/*
 * PUBLIC: int f_term __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_term)
{
	return (opt_dup(sp, O_TERM, str));
}

/*
 * PUBLIC: int f_ttywerase __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_ttywerase)
{
	if (turnoff)
		O_CLR(sp, O_TTYWERASE);
	else {
		O_SET(sp, O_TTYWERASE);
		O_CLR(sp, O_ALTWERASE);
	}
	return (0);
}

/*
 * PUBLIC: int f_w300 __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_w300)
{
	/* Historical behavior for w300 was < 1200. */
	if (baud_from_bval(sp) >= 1200)
		return (0);

	if (CALL(f_window))
		return (1);

	if (val >= O_VAL(sp, O_LINES) - 1 &&
	    (val = O_VAL(sp, O_LINES) - 1) == 0)
		val = 1;
	O_VAL(sp, O_W300) = val;
	return (0);
}

/*
 * PUBLIC: int f_w1200 __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_w1200)
{
	u_long v;

	/* Historical behavior for w1200 was == 1200. */
	v = baud_from_bval(sp);
	if (v < 1200 || v > 4800)
		return (0);

	if (CALL(f_window))
		return (1);

	if (val >= O_VAL(sp, O_LINES) - 1 &&
	    (val = O_VAL(sp, O_LINES) - 1) == 0)
		val = 1;
	O_VAL(sp, O_W1200) = val;
	return (0);
}

/*
 * PUBLIC: int f_w9600 __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_w9600)
{
	speed_t v;

	/* Historical behavior for w9600 was > 1200. */
	v = baud_from_bval(sp);
	if (v <= 4800)
		return (0);

	if (CALL(f_window))
		return (1);

	if (val >= O_VAL(sp, O_LINES) - 1 &&
	    (val = O_VAL(sp, O_LINES) - 1) == 0)
		val = 1;
	O_VAL(sp, O_W9600) = val;
	return (0);
}

/*
 * PUBLIC: int f_window __P((SCR *, OPTION *, char *, u_long));
 */
DECL(f_window)
{
	if (val >= O_VAL(sp, O_LINES) - 1 &&
	    (val = O_VAL(sp, O_LINES) - 1) == 0)
		val = 1;
	O_VAL(sp, O_WINDOW) = val;

	return (0);
}

/*
 * opt_dup --
 *	Copy a string value for user display.
 */
static int
opt_dup(sp, opt, str)
	SCR *sp;
	int opt;
	char *str;
{
	char *p;

	/* Copy for user display. */
	if ((p = strdup(str)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}
	if (O_STR(sp, opt) != NULL)
		free(O_STR(sp, opt));
	O_STR(sp, opt) = p;
	return (0);
}

/*
 * baud_from_bval --
 *	Return the baud rate using the standard defines.
 *
 * PUBLIC: u_long baud_from_bval __P((SCR *));
 */
u_long
baud_from_bval(sp)
	SCR *sp;
{
	if (!F_ISSET(sp->gp, G_TERMIOS_SET))
		return (9600);

	/*
	 * XXX
	 * There's no portable way to get a "baud rate" -- cfgetospeed(3)
	 * returns the value associated with some #define, which we may
	 * never have heard of, or which may be a purely local speed.  Vi
	 * only cares if it's SLOW (w300), slow (w1200) or fast (w9600).
	 * Try and detect the slow ones, and default to fast.
	 */
	switch (cfgetospeed(&sp->gp->original_termios)) {
	case B50:
	case B75:
	case B110:
	case B134:
	case B150:
	case B200:
	case B300:
	case B600:
		return (600);
	case B1200:
		return (1200);
	}
	return (9600);
}
