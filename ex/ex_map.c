/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_map.c,v 8.16 1994/07/16 09:22:32 bostic Exp $ (Berkeley) $Date: 1994/07/16 09:22:32 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <curses.h>
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_map -- :map[!] [input] [replacement]
 *	Map a key/string or display mapped keys.
 *
 * Historical note:
 *	Historic vi maps were fairly bizarre, and likely to differ in
 *	very subtle and strange ways from this implementation.  Two
 *	things worth noting are that vi would often hang or drop core
 *	if the map was strange enough (ex: map X "xy$@x^V), or, simply
 *	not work.  One trick worth remembering is that if you put a
 *	mark at the start of the map, e.g. map X mx"xy ...), or if you
 *	put the map in a .exrc file, things would often work much better.
 *	No clue why.
 */
int
ex_map(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	enum seqtype stype;
	CHAR_T *input, *p;

	stype = F_ISSET(cmdp, E_FORCE) ? SEQ_INPUT : SEQ_COMMAND;

	switch (cmdp->argc) {
	case 0:
		if (seq_dump(sp, stype, 1) == 0)
			msgq(sp, M_INFO, "No %s map entries",
			    stype == SEQ_INPUT ? "input" : "command");
		return (0);
	case 2:
		input = cmdp->argv[0]->bp;
		break;
	default:
		abort();
	}

	/*
	 * If the mapped string is #[0-9]* (and wasn't quoted) then store
	 * the function key mapping, and call the screen specific routine.
	 * Note, if the screen specific routine is able to create the
	 * mapping, the SEQ_FUNCMAP type stays around, maybe the next screen
	 * type can get it right.
	 */
	if (input[0] == '#') {
		for (p = input + 1; isdigit(*p); ++p);
		if (p[0] != '\0')
			goto nofunc;

		if (seq_set(sp, NULL, 0, input, cmdp->argv[0]->len,
		    cmdp->argv[1]->bp, cmdp->argv[1]->len, stype, SEQ_FUNCMAP))
			return (1);
		return (sp->s_fmap(sp, stype, input, cmdp->argv[0]->len,
		    cmdp->argv[1]->bp, cmdp->argv[1]->len));
	}

	/* Some single keys may not be remapped in command mode. */
nofunc:	if (stype == SEQ_COMMAND && input[1] == '\0')
		switch (KEY_VAL(sp, input[0])) {
		case K_COLON:
		case K_CR:
		case K_ESCAPE:
		case K_NL:
			msgq(sp, M_ERR, "The %s character may not be remapped",
			    KEY_NAME(sp, input[0]));
			return (1);
		}
	return (seq_set(sp, NULL, 0, input, cmdp->argv[0]->len,
	    cmdp->argv[1]->bp, cmdp->argv[1]->len, stype, SEQ_USERDEF));
}

/*
 * ex_unmap -- (:unmap[!] key)
 *	Unmap a key.
 */
int
ex_unmap(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	if (seq_delete(sp, cmdp->argv[0]->bp, cmdp->argv[0]->len,
	    F_ISSET(cmdp, E_FORCE) ? SEQ_INPUT : SEQ_COMMAND)) {
		msgq(sp, M_INFO, "\"%s\" isn't mapped", cmdp->argv[0]->bp);
		return (1);
	}
	return (0);
}

/*
 * map_save --
 *	Save the mapped sequences to a file.
 */
int
map_save(sp, fp)
	SCR *sp;
	FILE *fp;
{
	if (seq_save(sp, fp, "map ", SEQ_COMMAND))
		return (1);
	return (seq_save(sp, fp, "map! ", SEQ_INPUT));
}
