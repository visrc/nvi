/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_util.c,v 10.7 1995/07/08 12:44:33 bostic Exp $ (Berkeley) $Date: 1995/07/08 12:44:33 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
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

/*
 * ex_cbuild --
 *	Build an EX command structure.
 *
 * PUBLIC: void ex_cbuild __P((EXCMD *,
 * PUBLIC:   int, int, recno_t, recno_t, int, ARGS *[2], ARGS *a, char *));
 */
void
ex_cbuild(cmdp, cmd_id, naddr, lno1, lno2, force, ap, a, arg)
	EXCMD *cmdp;
	int cmd_id, force, naddr;
	recno_t lno1, lno2;
	ARGS *ap[2], *a;
	char *arg;
{
	memset(cmdp, 0, sizeof(EXCMD));
	cmdp->cmd = &cmds[cmd_id];
	cmdp->addrcnt = naddr;
	cmdp->addr1.lno = lno1;
	cmdp->addr2.lno = lno2;
	cmdp->addr1.cno = cmdp->addr2.cno = 1;
	if (force)
		cmdp->iflags |= E_C_FORCE;
	if ((a->bp = arg) == NULL) {
		cmdp->argc = 0;
		a->len = 0;
	} else {
		cmdp->argc = 1;
		a->len = strlen(arg);
	}
	ap[0] = a;
	ap[1] = NULL;
	cmdp->argv = ap;
}

/*
 * ex_getline --
 *	Return a line from the file.
 *
 * PUBLIC: int ex_getline __P((SCR *, FILE *, size_t *));
 */
int
ex_getline(sp, fp, lenp)
	SCR *sp;
	FILE *fp;
	size_t *lenp;
{
	EX_PRIVATE *exp;
	size_t off;
	int ch;
	char *p;

	exp = EXP(sp);
	for (errno = 0, off = 0, p = exp->ibp;;) {
		if (off >= exp->ibp_len) {
			BINC_RET(sp, exp->ibp, exp->ibp_len, off + 1);
			p = exp->ibp + off;
		}
		if ((ch = getc(fp)) == EOF && !feof(fp)) {
			if (errno == EINTR) {
				errno = 0;
				clearerr(fp);
				continue;
			}
			return (1);
		}
		if (ch == EOF || ch == '\n') {
			if (ch == EOF && !off)
				return (1);
			*lenp = off;
			return (0);
		}
		*p++ = ch;
		++off;
	}
	/* NOTREACHED */
}

/*
 * ex_ncheck --
 *	Check for more files to edit.
 *
 * PUBLIC: int ex_ncheck __P((SCR *, int));
 */
int
ex_ncheck(sp, force)
	SCR *sp;
	int force;
{
	/*
	 * !!!
	 * Historic practice: quit! or two quit's done in succession
	 * (where ZZ counts as a quit) didn't check for other files.
	 */
	if (!force && sp->ccnt != sp->q_ccnt + 1 &&
	    sp->cargv != NULL && sp->cargv[1] != NULL) {
		sp->q_ccnt = sp->ccnt;
		msgq(sp, M_ERR,
"167|More files to edit; use n[ext] to go to the next file, q[uit]! to quit");
		return (1);
	}
	return (0);
}

/*
 * ex_message --
 *	Display a few common messages.
 *
 * PUBLIC: void ex_message __P((SCR *, char *, exm_t));
 */
void
ex_message(sp, p, which)
	SCR *sp;
	char *p;
	exm_t which;
{
	switch (which) {
	case EXM_EMPTYBUF:
		msgq(sp, M_ERR, "168|Buffer %s is empty", p);
		break;
	case EXM_INTERRUPT:
		msgq(sp, M_ERR, v_event_flush(sp, CH_MAPPED) ?
		    "169|Interrupted: mapped keys discarded" :
		    "170|Interrupted");
		break;
	case EXM_NOCANON:
		msgq(sp, M_ERR,
	"268|That form of the %s command requires the ex terminal interface",
		    p);
		break;
	case EXM_NOFILEYET:
		if (p == NULL)
			msgq(sp, M_ERR,
			    "281|Command failed, no file read in yet.");
		else
			msgq(sp, M_ERR,
	"173|The %s command requires that a file have already been read in", p);
		break;
	case EXM_NOPREVBUF:
		msgq(sp, M_ERR, "171|No previous buffer to execute");
		break;
	case EXM_NOPREVRE:
		msgq(sp, M_ERR, "172|No previous regular expression");
		break;
	case EXM_USAGE:
		msgq(sp, M_ERR, "174|Usage: %s", p);
		break;
	}
}
