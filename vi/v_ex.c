/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 5.2 1992/04/22 08:10:21 bostic Exp $ (Berkeley) $Date: 1992/04/22 08:10:21 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "curses.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_ex --
 *	Executes a string of ex commands, and wait for a user keystroke
 *	before returning to the vi screen.  If that keystroke is another
 *	':', it continues with ex commands.
 */
MARK
v_ex(m, text)
	MARK	m;	/* the current line */
	char	*text;	/* the first command to execute */
{
	/* run the command.  be careful about modes & output */
	exwrote = (mode == MODE_COLON);
	ex_cstring(text, strlen(text), 0);
	ex_refresh();

	/* if mode is no longer MODE_VI, then we should quit right away! */
	if (mode != MODE_VI && mode != MODE_COLON)
		return cursor;

	/* The command did some output.  Wait for a keystoke. */
	if (exwrote)
	{
		mode = MODE_VI;	
		msg("[Hit <RETURN> to continue]");
		if (getkey(0) == ':')
		{	mode = MODE_COLON;
			addch('\n');
		}
		else
			iredraw();
	}

	return cursor;
}
