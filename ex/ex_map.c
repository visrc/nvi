/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_map.c,v 5.15 1992/12/05 11:08:40 bostic Exp $ (Berkeley) $Date: 1992/12/05 11:08:40 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "seq.h"

/*
 * ex_map -- :map[!] [key replacement]
 *	Map a key or display mapped keys.
 */
int
ex_map(cmdp)
	EXCMDARG *cmdp;
{
	register int ch;
	register u_char *input, *output;
	enum seqtype stype;
	int key;
	u_char *name;
	char *s, buf[10];

	stype = cmdp->flags & E_FORCE ? INPUT : COMMAND;

	if (cmdp->string == NULL) {
		if (seq_dump(stype, 1) == 0)
			msg("No map entries.");
		return (0);
	}

	/*
	 * Input is the first word, output is everything else, i.e. any space
	 * characters are included.  This is why we can't parse this command
	 * in the main parser.
	 */
	for (input = cmdp->string; isspace(*input); ++input);
	for (output = input; (ch = *output) && !isspace(ch); ++output);
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
		key = UATOI(input + 1);
		(void)snprintf(buf, sizeof(buf), "f%d", key);
#ifdef notdef
		if (FKEY[key]) {		/* CCC */
			input = FKEY[key];
			name = (u_char *)buf;
		} else {
			msg("This terminal has no %s key.", buf);
			return (1);
		}
#else
		name = NULL;
#endif
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
	EXCMDARG *cmdp;
{
	u_char *input;

	input = cmdp->argv[0];
	if (seq_delete(input, cmdp->flags & E_FORCE ? INPUT : COMMAND)) {
		msg("\"%s\" isn't mapped.", input);
		return (1);
	}
	return (0);
}

/*
 * map_save --
 *	Save the mapped sequences to a file.
 */
int
map_save(fp)
	FILE *fp;
{
	
	if (seq_save(fp, (u_char *)"map ", COMMAND));
		return (1);
	return (seq_save(fp, (u_char *)"map! ", INPUT));
}
