/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: exf.c,v 5.4 1992/05/04 11:49:58 bostic Exp $ (Berkeley) $Date: 1992/05/04 11:49:58 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <db.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "exf.h"
#include "options.h"
#include "pathnames.h"
#include "extern.h"

static EXFLIST exfhdr;				/* List of files. */
EXF *curf;					/* Current file. */

#ifdef FILE_LIST_DEBUG
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
	
	ep->flags = F_NONAME;
	ep->name = "no.name";
	ep->nlen = strlen(ep->name);
	ep->pos = MARK_FIRST;
	insexf(ep, &exfhdr);

	curf = ep;
	return (0);
}

/*
 * file_ins --
 *	Insert a new file into the list after the specified file.
 */
int
file_ins(before, name)
	EXF *before;
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

	ep->nlen = strlen(ep->name);
	ep->pos = MARK_FIRST;
	ep->flags = ISSET(O_READONLY) ? F_RDONLY : 0;
	insexf(ep, before);
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
		ep->nlen = strlen(ep->name);
		ep->pos = MARK_FIRST;
		ep->flags = ISSET(O_READONLY) ? F_RDONLY : 0;
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

#ifdef FILE_LIST_DEBUG
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

/*
 * fetchline --
 *	Find a line and return a pointer to a copy of its text.
 * XXX
 * Old interface.
 */
char *
fetchline(lno, lenp)
	long lno;
	size_t *lenp;
{
	return (file_line(curf, lno, lenp));
}

/*
 * file_line --
 *	Retrieve a line from the file.
 */
char *
file_line(ep, lno, lenp)
	EXF *ep;
	u_long lno;				/* Line number. */
	size_t *lenp;				/* Length store. */
{
	DBT data, key;

	key.data = &lno;
	key.size = sizeof(lno);

	switch((ep->db->get)(ep->db, &key, &data, 0)) {
        case -1:
		msg("%s: line %lu: %s", ep->name, lno, strerror(errno));
		/* FALLTHROUGH */
        case 1:
		return (NULL);
		/* NOTREACHED */
	}
	if (lenp)
		*lenp = data.size;
	return (data.data);
}

/*
 * file_start --
 *	Start editing a file.
 */
int
file_start(ep)
	EXF *ep;
{
	int flags;

	/* Open a db structure. */
	if ((ep->db = dbopen(ep->flags & F_NONAME ? NULL : ep->name,
	    O_CREAT | O_EXLOCK | O_RDONLY, 0, DB_RECNO, NULL)) == NULL) {
		msg("%s: %s", ep->name, strerror(errno));
		return (1);
	}

	/* Init the file structure. */
	ep->lno = 1;
	ep->cno = 1;
	ep->nlines = 0;

	/*
	 * Set up the cursor.
	 * XXX
	 * Old stuff.
	 */
	cursor = ep->pos = MARK_FIRST;

	/*
	 * XXX
	 * Right now, db doesn't return enough info to set the read-only
	 * flag, nor do we have a stat structure to set the dev/ino bits.
	 */

	/* Set the current file pointer. */
	curf = ep;

	/*
	 * XXX
	 * Major kludge, for now, so that nlines is right.
	 */
	for (nlines = 1; fetchline(nlines, NULL); ++nlines);
	--nlines;

	return (0);
}

/*
 * file_stop --
 *	Close the underlying db.
 */
int
file_stop(ep, force)
	EXF *ep;
	int force;
{
	/* Close the db structure. */
	if ((ep->db->close)(ep->db)) {
		msg("%s: close: %s", ep->name, strerror(errno));
		if (!force)
			return (1);
	}
	return (0);
}

/*
 * file_sync --
 *	Sync the file to disk.
 */
int
file_sync(ep, force)
	EXF *ep;
	int force;
{
	struct stat sb;
	FILE *fp;
	int fd;

	/* If unmodified, nothing to sync. */
	if (!(ep->flags & F_MODIFIED))
		return (0);

	/* Can't write if read-only. */
	if (ep->flags & F_RDONLY && !force) {
		msg("Read-only file, not written; use ! to override.");
		return (1);
	}

	/* Can't write if no name ever specified. */
	if (ep->flags & F_NONAME) {
		msg("No name for this file; not written.");
		return (1);
	}

	/*
	 * If the name was changed, can't use the db write routines, and
	 * normal rules apply, i.e. don't overwrite unless forced.
	 */
	if (ep->flags & F_NAMECHANGED) {
		if (!force && !stat(ep->name, &sb)) {
			msg("%s exists, not written; use ! to override.",
			    ep->name);
			return (1);
		}

		if ((fd = open(ep->name,
		    O_CREAT | O_EXLOCK | O_WRONLY, DEFFILEMODE)) < 0)
			goto err;

		if ((fp = fdopen(fd, "w")) == NULL) {
			(void)close(fd);
err:			msg("%s: %s", ep->name, strerror(errno));
			return (1);
		}

		if (ex_writefp(ep->name, fp, MARK_FIRST, MARK_LAST, 1))
			return (1);
	} else if ((ep->db->sync)(ep->db)) {
		msg("%s: sync: %s", ep->name, strerror(errno));
		return (1);
	}

	ep->flags &= ~F_MODIFIED;
	return (0);
}
