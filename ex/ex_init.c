/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_init.c,v 8.7 1993/11/13 18:02:24 bostic Exp $ (Berkeley) $Date: 1993/11/13 18:02:24 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "tag.h"

/*
 * ex_screen_copy --
 *	Copy ex screen.
 */
int
ex_screen_copy(orig, sp)
	SCR *orig, *sp;
{
	EX_PRIVATE *oexp, *nexp;

	/* Create the private ex structure. */
	if ((sp->ex_private = nexp = malloc(sizeof(EX_PRIVATE))) == NULL)
		goto mem;
	memset(nexp, 0, sizeof(EX_PRIVATE));

	/* Initialize queues. */
	queue_init(&nexp->tagq);
	queue_init(&nexp->tagfq);

	if (orig == NULL) {
		nexp->at_lbuf_set = 0;
	} else {
		oexp = EXP(orig);

		nexp->at_lbuf = oexp->at_lbuf;
		nexp->at_lbuf_set = oexp->at_lbuf_set;

		if (oexp->lastbcomm != NULL &&
		    (nexp->lastbcomm = strdup(oexp->lastbcomm)) == NULL) {
mem:			msgq(sp, M_SYSERR, NULL);
			return(1);
		}

		if (ex_tagcopy(orig, sp))
			return (1);
	}

	nexp->lastcmd = &cmds[C_PRINT];
	return (0);
}

/*
 * ex_screen_end --
 *	End a vi screen.
 */
int
ex_screen_end(sp)
	SCR *sp;
{
	EX_PRIVATE *exp;
	int rval;

	rval = 0;
	exp = EXP(sp);

	if (argv_free(sp))
		rval = 1;

	if (exp->ibp != NULL)
		FREE(exp->ibp, exp->ibp_len);

	if (exp->lastbcomm != NULL)
		FREE(exp->lastbcomm, strlen(exp->lastbcomm) + 1);

	FREE(exp, sizeof(EX_PRIVATE));
	return (rval);
}

/*
 * ex_init --
 *	Initialize ex.
 */
int
ex_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	size_t len;

	/*
	 * The default address is the last line of the file.  If the address
	 * set bit is on for this file, load the address, ensuring that it
	 * exists.
	 */
	if (F_ISSET(sp->frp, FR_CURSORSET)) {
		sp->lno = sp->frp->lno;
		sp->cno = sp->frp->cno;

		if (file_gline(sp, ep, sp->lno, &len) == NULL) {
			if (file_lline(sp, ep, &sp->lno))
				return (1);
			if (sp->lno == 0)
				sp->lno = 1;
			sp->cno = 0;
		} else if (sp->cno >= len)
			sp->cno = 0;
	} else {
		if (file_lline(sp, ep, &sp->lno))
			return (1);
		if (sp->lno == 0)
			sp->lno = 1;
		sp->cno = 0;
	}

	/* Display the status line. */
	return (status(sp, ep, sp->lno, 0));
}

/*
 * ex_end --
 *	End ex session.
 */
int
ex_end(sp)
	SCR *sp;
{
	/* Save the cursor location. */
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

	return (0);
}

/*
 * ex_optchange --
 *	Handle change of options for vi.
 */
int
ex_optchange(sp, opt)
	SCR *sp;
	int opt;
{
	switch (opt) {
	case O_TAGS:
		return (ex_tagalloc(sp, O_STR(sp, O_TAGS)));
	}
	return (0);
}
