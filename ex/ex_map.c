/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_map.c,v 8.5 1993/11/29 14:15:17 bostic Exp $ (Berkeley) $Date: 1993/11/29 14:15:17 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "seq.h"
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
	char *input, *output;
	enum seqtype stype;
	int key;
	char *name, buf[10];

	stype = F_ISSET(cmdp, E_FORCE) ? SEQ_INPUT : SEQ_COMMAND;

	switch (cmdp->argc) {
	case 0:
		if (seq_dump(sp, stype, 1) == 0)
			msgq(sp, M_INFO, "No %s map entries.",
			    stype == SEQ_INPUT ? "input" : "command");
		return (0);
	case 2:
		input = cmdp->argv[0];
		output = cmdp->argv[1];
		break;
	default:
		abort();
	}

	/*
	 * If the mapped string is #[0-9] (and wasn't quoted in any
	 * way, then map to a function key.
	 */
	if (input[0] == '#' && isdigit(input[1]) && !input[2]) {
		key = atoi(input + 1);
		(void)snprintf(buf, sizeof(buf), "f%d", key);
#ifdef notdef
		if (FKEY[key]) {		/* CCC */
			input = FKEY[key];
			name = buf;
		} else {
			msgq(sp, M_ERR, "This terminal has no %s key.", buf);
			return (1);
		}
#else
		name = NULL;
#endif
	} else {
		name = NULL;

		/* Some single keys may not be remapped in command mode. */
		if (stype == SEQ_COMMAND && input[1] == '\0')
			switch (term_key_val(sp, input[0])) {
			case K_COLON:
			case K_CR:
			case K_ESCAPE:
			case K_NL:
				msgq(sp, M_ERR,
				    "The %s character may not be remapped.",
				    charname(sp, input[0]));
				return (1);
			}
	}
	return (seq_set(sp, name, input, output, stype, 1));
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
	char *input;

	input = cmdp->argv[0];
	if (seq_delete(sp,
	    input, F_ISSET(cmdp, E_FORCE) ? SEQ_INPUT : SEQ_COMMAND)) {
		msgq(sp, M_INFO, "\"%s\" isn't mapped.", input);
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
