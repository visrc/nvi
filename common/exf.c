/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: exf.c,v 5.16 1992/10/01 17:29:46 bostic Exp $ (Berkeley) $Date: 1992/10/01 17:29:46 $";
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
#include "cut.h"
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

#define	EPSET(ep) {							\
	ep->lno = ep->top = 1;						\
	ep->cno = ep->lcno = 0;						\
	ep->uwindow = ep->dwindow = 0;					\
	ep->nlen = strlen(ep->name);					\
	ep->flags = 0;							\
}

/*
 * file_ins --
 *	Insert a new file into the list before or after the specified file.
 */
int
file_ins(ep, name, append)
	EXF *ep;
	char *name;
	int append;
{
	EXF *nep;

	if ((nep = malloc(sizeof(EXF))) == NULL)
		goto err;

	if ((nep->name = strdup(name)) == NULL) {
		free(nep);
err:		msg("Error: %s", strerror(errno));
		return (1);
	}
	EPSET(nep);
	if (append) {
		insexf(nep, ep);
	} else {
		instailexf(nep, ep);
	}
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
		EPSET(ep);
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
	TEXT *tp;
	recno_t cnt;

	/*
	 * The underlying recno stuff handles zero by returning NULL, but have
	 * to have an oob condition for the look-aside into the input buffer
	 * anyway.
	 */
	if (lno == 0)
		return (NULL);

	/*
	 * If we're in input mode, look-aside into the input buffer and
	 * see if the line we want is there.
	 */
	if (ib.stop.lno >= lno && ib.start.lno <= lno) {
		for (cnt = ib.start.lno, tp = ib.head; cnt < lno; ++cnt)
			tp = tp->next;
		if (lenp)
			*lenp = tp->len;
		return (tp->lp);
	}

	/* Otherwise, get the line from the underlying file. */
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
	if (mode == MODE_VI)
		scr_ldelete(lno);
	ep->flags |= F_MODIFIED;
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
	TRACE("append to %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	if ((ep->db->put)(ep->db, &key, &data, R_IAFTER) == -1) {
		msg("%s: line %lu: %s", ep->name, lno, strerror(errno));
		return (1);
	}
	if (mode == MODE_VI)
		scr_linsert(lno + 1, p, len);
	ep->flags |= F_MODIFIED;
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
	TRACE("insert before %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;

	if ((ep->db->put)(ep->db, &key, &data, R_IBEFORE) == -1) {
		msg("%s: line %lu: %s", ep->name, lno, strerror(errno));
		return (1);
	}
	if (mode == MODE_VI)
		scr_linsert(lno, p, len);
	ep->flags |= F_MODIFIED;
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
	TRACE("replace line %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;

	if ((ep->db->put)(ep->db, &key, &data, 0) == -1) {
		msg("%s: line %lu: %s", ep->name, lno, strerror(errno));
		return (1);
	}
	if (mode == MODE_VI)
		scr_lchange(lno, p, len);
	ep->flags |= F_MODIFIED;
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
		lno = 0;
		break;
	default:
		bcopy(key.data, &lno, sizeof(lno));
	}
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
	struct stat sb;
	int fd, flags;
	char tname[sizeof(_PATH_TMPNAME) + 1];

	if (ep->name == NULL) {
		(void)strcpy(tname, _PATH_TMPNAME);
		if ((fd = mkstemp(tname)) == -1) {
			msg("Temporary file: %s", strerror(errno));
			return (1);
		}
		file_ins((EXF *)&exfhdr, tname, 1);
		ep = file_first();
		ep->flags |= F_CREATED | F_NONAME;
	} else if (stat(ep->name, &sb))
		ep->flags | F_CREATED;

	/* Open a db structure. */
	ep->db = dbopen(ep->name, O_CREAT | O_EXLOCK | O_NONBLOCK| O_RDONLY,
	    DEFFILEMODE, DB_RECNO, NULL);
	if (ep->db == NULL && errno == EAGAIN) {
		ep->flags |= F_RDONLY;
		ep->db = dbopen(ep->name, O_CREAT | O_NONBLOCK | O_RDONLY,
		    DEFFILEMODE, DB_RECNO, NULL);
		if (ep->db != NULL)
			msg("%s already locked, session is read-only.",
			    ep->name);
	}
	if (ep->db == NULL) {
		msg("%s: %s", ep->name, strerror(errno));
		return (1);
	}

	/* Set the global state. */
	curf = ep;

	/*
	 * Reset any marks.
	 * XXX
	 * Not sure this should be here, but don't know where else to put
	 * it, either.
	 */
	mark_reset();

	/*
	 * XXX
	 * Kludge for nlines.
	 */
	nlines = file_lline(curf);

	return (0);
}

/*
 * file_stop --
 *	Stop editing a file.
 */
int
file_stop(ep, force)
	EXF *ep;
	int force;
{
	struct stat sb;

	/* If modified, must force. */
	if (ep->flags & F_MODIFIED && !force) {
		msg("Modified since last write; use ! to override.");
		return (1);
	}

	/* Close the db structure. */
	if ((ep->db->close)(ep->db) && !force) {
		msg("%s: close: %s", ep->name, strerror(errno));
		return (1);
	}
	/*
	 * Delete any created, empty file that was never written.
	 *
	 * XXX
	 * This is not quite right; a user writing an empty file explicitly
	 * with the ":w file" command could lose their write when we delete
	 * the file.  Probably not a big deal.
	 */
	if ((ep->flags & F_NONAME || ep->flags & F_CREATED &&
	    !(ep->flags & F_WRITTEN) &&
	    !stat(ep->name, &sb) && sb.st_size == 0) && unlink(ep->name)) {
		msg("Created file %s not removed.", ep->name);
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
		msg("Temporary file; not written.");
		return (1);
	}

	/*
	 * If the name was changed, normal rules apply, i.e. don't overwrite 
	 * unless forced.
	 */
	if (ep->flags & F_NAMECHANGED && !force && !stat(ep->name, &sb)) {
		msg("%s exists, not written; use ! to override.", ep->name);
		return (1);
	}

	/* Make sure the db routines have completely read the file. */
	(void)file_lline(ep);

	if ((fd = open(ep->name, O_WRONLY, 0)) < 0)
		goto err;

	if ((fp = fdopen(fd, "w")) == NULL) {
		(void)close(fd);
err:		msg("%s: %s", ep->name, strerror(errno));
		return (1);
	}

	if (ex_writefp(ep->name, fp, 1, 0, 1))
		return (1);

	ep->flags |= F_WRITTEN;
	ep->flags &= ~F_MODIFIED;
	return (0);
}
