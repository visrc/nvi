/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: util.c,v 5.3 1992/04/05 15:47:10 bostic Exp $ (Berkeley) $Date: 1992/04/05 15:47:10 $";
#endif /* not lint */

#include <unistd.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"

/*
 * bell --
 *	Ring the terminal's bell.
 */
void
bell()
{
	if (ISSET(O_VBELL)) {
		do_VB();
		refresh();
	}
	else if (ISSET(O_ERRORBELLS))
		(void)write(STDOUT_FILENO, "\007", 1);	/* '\a' */
}

/*
 * parseptrn --
 */
char *
parseptrn(ptrn)
	register char *ptrn;
{
	register char sep;

	/*
	 * Return a pointer to the end of the string or the first character
	 * after the second occurrence of the first character.  In the latter
	 * case, replace the second occurrence with a '\0'.  Used to parse
	 * search strings.
	 */
	for (sep = *ptrn; *++ptrn && *ptrn != sep;)
		/* Backslash escapes the next character. */
		if (ptrn[0] == '\\' && ptrn[1] != '\0')
			++ptrn;
	if (*ptrn)
		*ptrn++ = '\0';
	return (ptrn);
}
