/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil. All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: api.c,v 8.5 1995/11/18 12:59:09 bostic Exp $ (Berkeley) $Date: 1995/11/18 12:59:09 $";
#endif /* not lint */

#ifdef TCL_INTERP
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "tcl_extern.h"

extern GS *__global_list;			/* XXX */

/*
 * api_fscreen --
 *	Return a pointer to the screen specified by the screen id
 *	or a file name.
 *
 * PUBLIC: SCR *api_fscreen __P((int, char *));
 */
SCR *
api_fscreen(id, name)
	int id;
	char *name;
{
	GS *gp;
	SCR *tsp;

	gp = __global_list;

	/* Search the displayed list. */
	for (tsp = gp->dq.cqh_first;
	    tsp != (void *)&gp->dq; tsp = tsp->q.cqe_next)
		if (name == NULL &&
		    id == tsp->id || !strcmp(name, tsp->frp->name))
			return (tsp);

	/* Search the hidden list. */
	for (tsp = gp->hq.cqh_first;
	    tsp != (void *)&gp->hq; tsp = tsp->q.cqe_next)
		if (name == NULL &&
		    id == tsp->id || !strcmp(name, tsp->frp->name))
			return (tsp);
	return (NULL);
}

/*
 * api_aline --
 *	Append a line.
 *
 * PUBLIC: int api_aline __P((SCR *, recno_t, char *, size_t));
 */
int
api_aline(sp, lno, line, len)
	SCR *sp;
	recno_t lno;
	char *line;
	size_t len;
{
	return (db_append(sp, 1, lno, line, len));
}

/*
 * api_dline --
 *	Delete a line.
 *
 * PUBLIC: int api_dline __P((SCR *, recno_t));
 */
int
api_dline(sp, lno)
	SCR *sp;
	recno_t lno;
{
	return (db_delete(sp, lno));
}

/*
 * api_gline --
 *	Get a line.
 *
 * PUBLIC: int api_gline __P((SCR *, recno_t, char **, size_t *));
 */
int
api_gline(sp, lno, linepp, lenp)
	SCR *sp;
	recno_t lno;
	char **linepp;
	size_t *lenp;
{
	return (db_get(sp, lno, DBG_FATAL, linepp, lenp));
}

/*
 * api_iline --
 *	Insert a line.
 *
 * PUBLIC: int api_iline __P((SCR *, recno_t, char *, size_t));
 */
int
api_iline(sp, lno, line, len)
	SCR *sp;
	recno_t lno;
	char *line;
	size_t len;
{
	return (db_insert(sp, lno, line, len));
}

/*
 * api_lline --
 *	Return the line number of the last line in the file.
 *
 * PUBLIC: int api_lline __P((SCR *, recno_t *));
 */
int
api_lline(sp, lnop)
	SCR *sp;
	recno_t *lnop;
{
	return (db_last(sp, lnop));
}

/*
 * api_sline --
 *	Set a line.
 *
 * PUBLIC: int api_sline __P((SCR *, recno_t, char *, size_t));
 */
int
api_sline(sp, lno, line, len)
	SCR *sp;
	recno_t lno;
	char *line;
	size_t len;
{
	return (db_set(sp, lno, line, len));
}

/*
 * api_getcursor --
 *	Get the cursor.
 *
 * PUBLIC: int api_getcursor __P((SCR *, MARK *));
 */
int
api_getcursor(sp, mp)
	SCR *sp;
	MARK *mp;
{
	mp->lno = sp->lno;
	mp->cno = sp->cno;
	return (0);
}

/*
 * api_setcursor --
 *	Set the cursor.
 *
 * PUBLIC: int api_setcursor __P((SCR *, MARK *));
 */
int
api_setcursor(sp, mp)
	SCR *sp;
	MARK *mp;
{
	size_t len;

	if (db_get(sp, mp->lno, DBG_FATAL, NULL, &len))
		return (1);
	if (mp->cno < 0 || mp->cno > len) {
		msgq(sp, M_ERR, "Cursor set to nonexistent column");
		return (1);
	}

	/* Set the cursor. */
	sp->lno = mp->lno;
	sp->cno = mp->cno;
	return (0);
}

/*
 * api_emessage --
 *	Print an error message.
 *
 * PUBLIC: void api_emessage __P((SCR *, char *));
 */
void
api_emessage(sp, text)
	SCR *sp;
	char *text;
{
	msgq(sp, M_ERR, "%s", text);
}

/*
 * api_imessage --
 *	Print an informational message.
 *
 * PUBLIC: void api_imessage __P((SCR *, char *));
 */
void
api_imessage(sp, text)
	SCR *sp;
	char *text;
{
	msgq(sp, M_INFO, "%s", text);
}

/*
 * api_iscreen
 *	Create a new screen and return its id.
 *
 * PUBLIC: int api_iscreen __P((SCR *, char *, int *));
 */
int
api_iscreen(sp, file, idp)
	SCR *sp;
	char *file;
	int *idp;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	ex_cbuild(&cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0, ap, &a, file);
	cmd.flags |= E_NEWSCREEN;			/* XXX */
	if (cmd.cmd->fn(sp, &cmd))
		return (1);
	*idp = sp->id;
	return (0);
}

/*
 * api_escreen
 *	End a screen.
 *
 * PUBLIC: int api_escreen __P((SCR *));
 */
int
api_escreen(sp)
	SCR *sp;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	/*
	 * XXX
	 * If the interpreter exits anything other than the current
	 * screen, vi isn't going to update everything correctly.
	 */
	ex_cbuild(&cmd, C_QUIT, 0, OOBLNO, OOBLNO, 0, ap, &a, NULL);
	return (cmd.cmd->fn(sp, &cmd));
}

/*
 * api_swscreen --
 *    Switch to a new screen.
 *
 * PUBLIC: int api_swscreen __P((SCR *, SCR *));
 */
int
api_swscreen(sp, new)
      SCR *sp, *new;
{
	/*
	 * XXX
	 * If the interpreter switches from anything other than the
	 * current screen, vi isn't going to update everything correctly.
	 */
	sp->nextdisp = new;
	F_SET(sp, S_SSWITCH);

	return (0);
}

/*
 * api_map --
 *	Map a key.
 *
 * PUBLIC: int api_map __P((SCR *, char *, char *, size_t));
 */
int
api_map(sp, name, map, len)
	SCR *sp;
	char *name, *map;
	size_t len;
{
	ARGS *ap[3], a, b;
	EXCMD cmd;

	ex_cbuild(&cmd, C_MAP, 0, OOBLNO, OOBLNO, 0, ap, &a, name);
	b.bp = (CHAR_T *)map;
	b.len = len;
	ap[1] = &b;
	return (cmd.cmd->fn(sp, &cmd));
}

/*
 * api_unmap --
 *	Unmap a key.
 *
 * PUBLIC: int api_unmap __P((SCR *, char *));
 */
int 
api_unmap(sp, name)
	SCR *sp;
	char *name;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	ex_cbuild(&cmd, C_UNMAP, 0, OOBLNO, OOBLNO, 0, ap, &a, name);
	return (cmd.cmd->fn(sp, &cmd));
}

/*
 * api_opts_get --
 *	Return a option value as a string, in allocated memory.
 *
 * PUBLIC: int api_opts_get __P((SCR *, char *, char **));
 */
int
api_opts_get(sp, name, value)
	SCR *sp;
	char *name, **value;
{
	int offset;
	OPTLIST const *op;

	if ((op = opts_search(name)) == NULL)
		return (1);

	offset = op - optlist;
	switch (op->type) {
	case OPT_0BOOL:
	case OPT_1BOOL:
		MALLOC_RET(sp, *value, char *, strlen(op->name) + 2);
		(void)sprintf(*value,
		    "%s%s", O_ISSET(sp, offset) ? "" : "no", op->name);
		break;
	case OPT_NUM:
		MALLOC_RET(sp, *value, char *, 20);
		(void)sprintf(*value, "%ld", (u_long)O_VAL(sp, offset));
		break;
	case OPT_STR:
		if (O_STR(sp, offset) == NULL) {
			MALLOC_RET(sp, *value, char *, 2);
			value[0] = '\0';
		} else {
			MALLOC_RET(sp,
			    *value, char *, strlen(O_STR(sp, offset)));
			(void)sprintf(*value, "%s", O_STR(sp, offset));
		}
		break;
	}
	return (0);
}

/*
 * api_opts_set --
 *	Set options.
 *
 * PUBLIC: int api_opts_set __P((SCR *, char *));
 */
int 
api_opts_set(sp, name)
	SCR *sp;
	char *name;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	ex_cbuild(&cmd, C_SET, 0, OOBLNO, OOBLNO, 0, ap, &a, name);
	return (cmd.cmd->fn(sp, &cmd));
}
#endif /* TCL_INTERP */
