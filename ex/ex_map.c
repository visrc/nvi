/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_map.c,v 5.6 1992/04/05 17:01:03 bostic Exp $ (Berkeley) $Date: 1992/04/05 17:01:03 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "seq.h"
#include "excmd.h"
#include "extern.h"

/*
 * map_init --
 *	Initialize the termcap key maps.
 */
int
map_init()
{
	int rval;

	/*
	 * The KU, KD, KL,and KR variables correspond to the :ku=: etc.
	 * termcap capabilities.  The variables are defined as part of
	 * the curses package.
	 */
	rval = 0;
	if (has_KU)
		rval |= seq_set("up", has_KU, "k", COMMAND, 0);
	if (has_KD)
		rval |= seq_set("down", has_KD, "j", COMMAND, 0);
	if (has_KL)
		rval |= seq_set("left", has_KL, "h", COMMAND, 0);
	if (has_KR)
		rval |= seq_set("right", has_KR, "l", COMMAND, 0);
	if (has_HM)
		rval |= seq_set("home", has_HM, "^", COMMAND, 0);
	if (has_EN)
		rval |= seq_set("end", has_EN, "$", COMMAND, 0);
	if (has_PU)
		rval |= seq_set("page up", has_PU, "\002", COMMAND, 0);
	if (has_PD)
		rval |= seq_set("page down", has_PD, "\006", COMMAND, 0);
	if (has_KI)
		rval |= seq_set("insert", has_KI, "i", COMMAND, 0);
	if (ERASEKEY != '\177')
		rval |= seq_set("delete", "\177", "x", COMMAND, 0);
	return (rval);
}

/*
 * ex_map -- (:map[!] [key replacement])
 *	Map a key or display mapped keys.
 */
int
ex_map(cmdp)
	CMDARG *cmdp;
{
	register char *input, *output;
	enum seqtype stype;
	int key;
	char *name, *s, buf[10];

	stype = cmdp->flags & E_FORCE ? INPUT : COMMAND;

	if (cmdp->string == NULL) {
		seq_dump(stype, 1);
		return (0);
	}

	/*
	 * Input is the first word, output is everything else, i.e. any
	 * space characters are included.  This is why we can't parse
	 * this command in the ex parser itself.
	 */
	for (input = output = cmdp->string;
	    *output && !isspace(*output); ++output);
	if (*output != '\0')
		for (*output++ = '\0'; isspace(*output); ++output);
	if (*output == '\0') {
		msg("Usage: %s.", cmdp->cmd->usage);
		return (1);
	}
	
	/*
	 * If the mapped string is #[0-9], then map to a function
	 * key.
	 */
	if (input[0] == '#' && isdigit(input[1]) && !input[2]) {
		key = atoi(input + 1);
		(void)snprintf(buf, sizeof(buf), "f%d", key);
		if (FKEY[key]) {
			input = FKEY[key];
			name = buf;
		} else {
			msg("This terminal has no %s key.", buf);
			return (1);
		}
	} else {
		name = NULL;

		/* Some single keys may not be remapped in command mode. */
		if (stype == COMMAND && input[1] == '\0')
			switch (input[0]) {
			case '\n':
				s = "\\n";
				goto noremap;
			case '\r':
				s = "\\r";
				goto noremap;
			case '\033':
				s = "^[";
				goto noremap;
			case ':':
				s = ":";
noremap:			msg("The %s character may not be remapped.", s);
				return (1);
			}
	}
	return (seq_set(name, input, output, stype, 1));
}

/*
 * ex_unmap -- (:unmap[!] key)
 *	Unmap a key.
 */
int
ex_unmap(cmdp)
	CMDARG *cmdp;
{
	char *input;

	input = cmdp->argv[0];
	if (seq_delete(input, cmdp->flags & E_FORCE ? INPUT : COMMAND)) {
		msg("\"%s\" was never mapped.", input);
		return (1);
	}
	return (0);
}

/*
 * map_save --
 *	Save the mapped sequences to a file.
 */
map_save(fp)
	FILE *fp;
{
	
	if (seq_save(fp, "map ", COMMAND));
		return (1);
	return (seq_save(fp, "map! ", INPUT));
}
