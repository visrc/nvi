/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: exf.c,v 5.1 1992/05/03 08:17:05 bostic Exp $ (Berkeley) $Date: 1992/05/03 08:17:05 $";
#endif /* not lint */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "options.h"
#include "exf.h"

static EXFLIST exfhdr;				/* List of files. */
EXF *curf;					/* Current file. */

#ifdef NOTDEF
void file_show __P((char *));
#endif

/*
 * file_init --
 *	Initialize the file list.
 */
void
file_init()
{
	exfhdr.next = exfhdr.prev = (EXF *)&exfhdr;
}

/*
 * file_default --
 *	Add the default "no-name" structure to the file list.
 */
int
file_default()
{
	EXF *ep;

	if ((ep = malloc(sizeof(EXF))) == NULL) {
		msg("Error: %s", strerror(errno));
		return (1);
	}
	
	ep->name = NULL;
	ep->lastpos = MARK_FIRST;
	ep->flags = F_NONAME;
	insexf(ep, &exfhdr);

	curf = ep;
	return (0);
}

/*
 * file_ins --
 *	Insert a new file into the list, after the current file.
 */
int
file_ins(name)
	char *name;
{
	EXF *ep;

	if ((ep = malloc(sizeof(EXF))) == NULL)
		goto err;
	if ((ep->name = strdup(name)) == NULL) {
		free(ep);
err:		msg("Error: %s", strerror(errno));
		return (1);
	}

	ep->lastpos = MARK_FIRST;
	/* ep->flags = ISSET(O_READONLY) ? F_RDONLY : 0; */
	insexf(ep, curf);
	return (0);
}

/*
 * file_set --
 *	Set the file list from an argc/argv.
 */
int
file_set(argc, argv)
	int argc;
	char *argv[];
{
	EXF *ep;

	/* Free up any previous list. */
	while (exfhdr.next != (EXF *)&exfhdr) {
		ep = exfhdr.next;
		rmexf(ep);
		if (ep->name)
			free(ep->name);
		free(ep);
		file_show("remove: ");
	}

	/* Insert new entries at the tail of the list. */
	for (; *argv; ++argv) {
		if ((ep = malloc(sizeof(EXF))) == NULL)
			goto err;
		if ((ep->name = strdup(*argv)) == NULL) {
			free(ep);
err:			msg("Error: %s", strerror(errno));
			return (1);
		}
		ep->lastpos = MARK_FIRST;
		/* ep->flags = ISSET(O_READONLY) ? F_RDONLY : 0; */
		instailexf(ep, &exfhdr);
	}

	curf = file_first();
	return (0);
}

/*
 * file_iscurrent --
 *	Return if the filename is the same as the current one.
 */
int
file_iscurrent(name)
	char *name;
{
	char *p;

	/*
	 * XXX
	 * Compare the dev/ino , not just the name; then, quit looking at
	 * just the file name (see parse_err in ex_errlist.c).
	 */
	if ((p = rindex(curf->name, '/')) == NULL)
		p = curf->name;
	else
		++p;
	return (!strcmp(p, name));
}


/*
 * file_first --
 *	Return the first file.
 */
EXF *
file_first()
{
	return (exfhdr.next);
}

/*
 * file_next --
 *	Return the next file, if any.
 */
EXF *
file_next(ep)
	EXF *ep;
{
	return (ep->next != (EXF *)&exfhdr ? ep->next : NULL);
}

/*
 * file_prev --
 *	Return the previous file, if any.
 */
EXF *
file_prev(ep)
	EXF *ep;
{
	return (ep->prev != (EXF *)&exfhdr ? ep->prev : NULL);
}

#ifdef NOTDEF
void
file_show(s)
	char *s;
{
	register EXF *ep;

	(void)printf("%s", s);
	for (ep = exfhdr.next; ep != (EXF *)&exfhdr; ep = ep->next)
		if (ep == curf)
			(void)printf("[%s] ", ep->name ? ep->name : "NONAME");
		else
			(void)printf("{%s} ", ep->name ? ep->name : "NONAME");
	(void)printf("\n");
}
#endif
