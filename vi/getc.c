/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: getc.c,v 8.12 1994/08/31 17:15:04 bostic Exp $ (Berkeley) $Date: 1994/08/31 17:15:04 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

/*
 * Character stream routines --
 *	These routines return the file a character at a time.  There are two
 *	special cases.  First, the end of a line, end of a file, start of a
 *	file and empty lines are returned as special cases, and no character
 *	is returned.  Second, empty lines include lines that have only white
 *	space in them, because the vi search functions don't care about white
 *	space, and this makes it easier for them to be consistent.
 */

/*
 * cs_init --
 *	Initialize character stream routines.
 */
int
cs_init(sp, ep, csp)
	SCR *sp;
	EXF *ep;
	VCS *csp;
{
	recno_t lno;

	if ((csp->cs_bp =
	    file_gline(sp, ep, csp->cs_lno, &csp->cs_len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			msgq(sp, M_BERR, "173|Empty file");
		else
			GETLINE_ERR(sp, csp->cs_lno);
		return (1);
	}
	if (csp->cs_len == 0 || v_isempty(csp->cs_bp, csp->cs_len)) {
		csp->cs_cno = 0;
		csp->cs_flags = CS_EMP;
	} else {
		csp->cs_flags = 0;
		csp->cs_ch = csp->cs_bp[csp->cs_cno];
	}
	return (0);
}

/*
 * cs_next --
 *	Retrieve the next character.
 */
int
cs_next(sp, ep, csp)
	SCR *sp;
	EXF *ep;
	VCS *csp;
{
	recno_t slno;

	switch (csp->cs_flags) {
	case CS_EMP:				/* EMP; get next line. */
	case CS_EOL:				/* EOL; get next line. */
		slno = csp->cs_lno;		/* Save current line. */
		if ((csp->cs_bp =
		    file_gline(sp, ep, ++csp->cs_lno, &csp->cs_len)) == NULL) {
			csp->cs_lno = slno;
			if (file_lline(sp, ep, &slno))
				return (1);
			if (slno > csp->cs_lno) {
				GETLINE_ERR(sp, csp->cs_lno);
				return (1);
			}
			csp->cs_flags = CS_EOF;
		} else if (csp->cs_len == 0 ||
		    v_isempty(csp->cs_bp, csp->cs_len)) {
			csp->cs_cno = 0;
			csp->cs_flags = CS_EMP;
		} else {
			csp->cs_flags = 0;
			csp->cs_ch = csp->cs_bp[csp->cs_cno = 0];
		}
		break;
	case 0:
		if (csp->cs_cno == csp->cs_len - 1)
			csp->cs_flags = CS_EOL;
		else
			csp->cs_ch = csp->cs_bp[++csp->cs_cno];
		break;
	case CS_EOF:				/* EOF. */
		break;
	default:
		abort();
		/* NOTREACHED */
	}
	return (0);
}

/*
 * cs_fspace --
 *	If on a space, eat forward until something other than a
 *	whitespace character.
 *
 * XXX
 * Semantics of checking the current character were coded for the fword()
 * function -- once the other word routines are converted, they may have
 * to change.
 */
int
cs_fspace(sp, ep, csp)
	SCR *sp;
	EXF *ep;
	VCS *csp;
{
	if (csp->cs_flags != 0 || !isblank(csp->cs_ch))
		return (0);
	for (;;) {
		if (cs_next(sp, ep, csp))
			return (1);
		if (csp->cs_flags != 0 || !isblank(csp->cs_ch))
			break;
	}
	return (0);
}

/*
 * cs_fblank --
 *	Eat forward to the next non-whitespace character.
 */
int
cs_fblank(sp, ep, csp)
	SCR *sp;
	EXF *ep;
	VCS *csp;
{
	for (;;) {
		if (cs_next(sp, ep, csp))
			return (1);
		if (csp->cs_flags == CS_EOL || csp->cs_flags == CS_EMP ||
		    csp->cs_flags == 0 && isblank(csp->cs_ch))
			continue;
		break;
	}
	return (0);
}

/*
 * cs_prev --
 *	Retrieve the previous character.
 */
int
cs_prev(sp, ep, csp)
	SCR *sp;
	EXF *ep;
	VCS *csp;
{
	recno_t slno;

	switch (csp->cs_flags) {
	case CS_EMP:				/* EMP; get previous line. */
	case CS_EOL:				/* EOL; get previous line. */
		if (csp->cs_lno == 1) {		/* SOF. */
			csp->cs_flags = CS_SOF;
			break;
		}
		slno = csp->cs_lno;		/* Save current line. */
		if ((csp->cs_bp =		/* Line should exist. */
		    file_gline(sp, ep, --csp->cs_lno, &csp->cs_len)) == NULL) {
			GETLINE_ERR(sp, csp->cs_lno);
			csp->cs_lno = slno;
			return (1);
		}
		if (csp->cs_len == 0 || v_isempty(csp->cs_bp, csp->cs_len)) {
			csp->cs_cno = 0;
			csp->cs_flags = CS_EMP;
		} else {
			csp->cs_flags = 0;
			csp->cs_cno = csp->cs_len - 1;
			csp->cs_ch = csp->cs_bp[csp->cs_cno];
		}
		break;
	case 0:
		if (csp->cs_cno == 0)
			if (csp->cs_lno == 1)
				csp->cs_flags = CS_SOF;
			else
				csp->cs_flags = CS_EOL;
		else
			csp->cs_ch = csp->cs_bp[--csp->cs_cno];
		break;
	case CS_SOF:				/* SOF. */
		break;
	default:
		abort();
		/* NOTREACHED */
	}
	return (0);
}

/*
 * cs_bblank --
 *	Eat backward to the next non-whitespace character.
 */
int
cs_bblank(sp, ep, csp)
	SCR *sp;
	EXF *ep;
	VCS *csp;
{
	for (;;) {
		if (cs_prev(sp, ep, csp))
			return (1);
		if (csp->cs_flags == CS_EOL || csp->cs_flags == CS_EMP ||
		    csp->cs_flags == 0 && isblank(csp->cs_ch))
			continue;
		break;
	}
	return (0);
}
