/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: exf.c,v 5.7 1992/05/21 13:03:28 bostic Exp $ (Berkeley) $Date: 1992/05/21 13:03:28 $";
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
#include "mark.h"
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

	file_ins((EXF *)&exfhdr, "no.name");
	ep->flags |= F_NONAME;
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
	ep->lno = 1;
	ep->cno = 0;
	ep->nlen = strlen(ep->name);
	ep->flags = 0;
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
		ep->lno = 1;
		ep->cno = 0;
		ep->nlen = strlen(ep->name);
		ep->flags = 0;
		instailexf(ep, &exfhdr);
	}
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

/*
 * file_gline --
 *	Retrieve a line from the file.
 */
char *
file_gline(ep, lno, lenp)
	EXF *ep;
	recno_t lno;				/* Line number. */
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
 * file_dline --
 *	Delete a line from the file.
 */
int
file_dline(ep, lno)
	EXF *ep;
	recno_t lno;
{
	DBT key;

#if DEBUG && 1
	TRACE("delete line %lu\n", lno);
#endif
	key.data = &lno;
	key.size = sizeof(lno);
	if ((ep->db->del)(ep->db, &key, 0) == 1) {
		msg("%s: line %lu: not found", ep->name, lno);
		return (1);
	}
	return (0);
}

/*
 * file_aline --
 *	Append a line into the file.
 */
int
file_aline(ep, lno, p, len)
	EXF *ep;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;

	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;

#if DEBUG && 1
	TRACE("append to line %lu {%.*s}\n", lno, MIN(len, 20), p);
#endif
	if ((ep->db->put)(ep->db, &key, &data, R_IAFTER) == -1) {
		msg("%s: line %lu: %s", ep->name, lno, strerror(errno));
		return (1);
	}
	return (0);
}

/*
 * file_iline --
 *	Insert a line into the file.
 */
int
file_iline(ep, lno, p, len)
	EXF *ep;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;

#if DEBUG && 1
	TRACE("insert before line %lu {%.*s}\n", lno, MIN(len, 20), p);
#endif
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;

	if ((ep->db->put)(ep->db, &key, &data, R_IBEFORE) == -1) {
		msg("%s: line %lu: %s", ep->name, lno, strerror(errno));
		return (1);
	}
	return (0);
}

/*
 * file_sline --
 *	Store a line in the file.
 */
int
file_sline(ep, lno, p, len)
	EXF *ep;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;

#if DEBUG && 1
	TRACE("replace line %lu {%.*s}\n", lno, MIN(len, 20), p);
#endif
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;

	if ((ep->db->put)(ep->db, &key, &data, 0) == -1) {
		msg("%s: line %lu: %s", ep->name, lno, strerror(errno));
		return (1);
	}
	return (0);
}

/*
 * file_lline --
 *	Figure out the line number of the last line in the file.
 */
u_long
file_lline(ep)
	EXF *ep;
{
	DBT data, key;
	recno_t lno;

	key.data = &lno;
	key.size = sizeof(lno);

	switch((ep->db->seq)(ep->db, &key, &data, R_LAST)) {
        case -1:
		msg("%s: line %lu: %s", ep->name, lno, strerror(errno));
		/* FALLTHROUGH */
        case 1:
		return (0);
		/* NOTREACHED */
	}

	bcopy(key.data, &lno, sizeof(lno));
	return (lno);
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

	/* Change the global state. */
	curf = ep;
	cursor.lno = ep->lno;
	cursor.cno = ep->cno;

	/*
	 * XXX
	 * Major kludge, for now, so that nlines is right.
	 */
	nlines = file_lline(curf);

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
	if ((ep->db->close)(ep->db) && !force) {
		msg("%s: close: %s", ep->name, strerror(errno));
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
	if ((ISSET(O_READONLY) || ep->flags & F_RDONLY) && !force) {
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

		if (ex_writefp(ep->name, fp, 1, 0, 1))
			return (1);
	} else if ((ep->db->sync)(ep->db)) {
		msg("%s: sync: %s", ep->name, strerror(errno));
		return (1);
	}

	ep->flags &= ~F_MODIFIED;
	return (0);
}
