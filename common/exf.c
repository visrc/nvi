/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: exf.c,v 5.22 1992/10/26 17:43:19 bostic Exp $ (Berkeley) $Date: 1992/10/26 17:43:19 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
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

static EXF defexf = {
	NULL, NULL,				/* next, prev */
	1, 1, 1, 1,				/* top, otop, lno, olno */
	0, 0, 0, 0,				/* cno, ocno, scno, shift */
	0, 0, 0,				/* rcm, lines, cols */
	0, 0,					/* cwidth, rcmflags */
	NULL,					/* db */
	NULL, 0, OOBLNO, OOBLNO,		/* c_lp, c_len, c_lno, nlines */
	NULL,					/* sdb */
	NULL, 0, 0,				/* name, nlen, flags */
};

static void file_copyright __P((EXF *));

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
	*nep = defexf;

	if ((nep->name = strdup(name)) == NULL) {
		free(nep);
err:		msg("Error: %s", strerror(errno));
		return (1);
	}
	nep->nlen = strlen(nep->name);
	
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
		*ep = defexf;
		if ((ep->name = strdup(*argv)) == NULL) {
			free(ep);
err:			msg("Error: %s", strerror(errno));
			return (1);
		}
		instailexf(ep, &exfhdr);
	}
	return (0);
}

/*
 * file_first --
 *	Return the first file.
 */
EXF *
file_first()
{
	EXF *ep;

	for (ep = exfhdr.next;;) {
		if (ep == (EXF *)&exfhdr)
			return (NULL);
		if (!(ep->flags & F_IGNORE))
			return (ep);
	}
	/* NOTREACHED */
}

/*
 * file_next --
 *	Return the next file, if any.
 */
EXF *
file_next(ep)
	EXF *ep;
{
	for (;;) {
		ep = ep->next;
		if (ep == (EXF *)&exfhdr)
			return (NULL);
		if (!(ep->flags & F_IGNORE))
			return (ep);
	}
	/* NOTREACHED */
}

/*
 * file_prev --
 *	Return the previous file, if any.
 */
EXF *
file_prev(ep)
	EXF *ep;
{
	for (;;) {
		ep = ep->prev;
		if (ep == (EXF *)&exfhdr)
			return (NULL);
		if (!(ep->flags & F_IGNORE))
			return (ep);
	}
	/* NOTREACHED */
}

/*
 * file_locate --
 *	Return the appropriate structure if we've seen this file before.
 */
EXF *
file_locate(name)
	char *name;
{
	EXF *p;

	for (p = exfhdr.next; p != (EXF *)&exfhdr; p = p->next)
		if (!bcmp(p->name, name, p->nlen))
			return (p);
	return (NULL);
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

	if (ep == NULL) {
		(void)strcpy(tname, _PATH_TMPNAME);
		if ((fd = mkstemp(tname)) == -1) {
			msg("Temporary file: %s", strerror(errno));
			return (1);
		}
		file_ins((EXF *)&exfhdr, tname, 1);
		ep = file_first();
		ep->flags |= F_CREATED | F_NONAME;
	} else if (stat(ep->name, &sb))
		ep->flags |= F_CREATED;

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
	if (ep->flags & F_NONAME)
		(void)close(fd);

	/* Flush the line caches. */
	ep->c_lno = ep->c_nlines = OOBLNO;

	/* Open a shadow db structure. */
	(void)strcpy(tname, _PATH_TMPNAME);
	if ((fd = mkstemp(tname)) == -1) {
		msg("Temporary file: %s", strerror(errno));
		return (1);
	}
	ep->sdb = dbopen(tname, O_CREAT | O_EXLOCK | O_NONBLOCK | O_RDWR,
	    DEFFILEMODE, DB_RECNO, NULL);
	(void)close(fd);
	if (ep->sdb == NULL) {
		msg("shadow db: %s", strerror(errno));
		return (1);
	}

	if (ISSET(O_COPYRIGHT))			/* Skip leading comment. */
		file_copyright(ep);

	if (mode == MODE_VI) {			/* Vi special things. */
		scr_init(ep);			/* Screen state. */
		status(ep, ep->lno);		/* Intro message. */
	}

	/*
	 * Reset any marks.
	 * XXX
	 * Not sure this should be here, but don't know where else to put
	 * it, either.
	 */
	mark_reset();

	/* Set the global state. */
	curf = ep;

	return (0);
}

/*
 * file_modify --
 *	Check for modification.
 */
int
file_modify(ep, force)
	EXF *ep;
	int force;
{
	/* If modified, must force. */
	if (ep->flags & F_MODIFIED && !force) {
		msg("Modified since last write; use ! to override.");
		return (1);
	}
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

	/* Close the db structure. */
	if ((ep->db->close)(ep->db) && !force) {
		msg("%s: close: %s", ep->name, strerror(errno));
		return (1);
	}

	/* Close the shadow structure. */
	if ((ep->sdb->close)(ep->sdb))
		msg("%s: shadow close: %s", ep->name, strerror(errno));

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

static void
file_copyright(ep)
	EXF *ep;
{
	recno_t lno;
	size_t len;
	u_char *p;

	for (lno = 1;
	    (p = file_gline(ep, lno, &len)) != NULL && len == 0; ++lno);
	if (p == NULL || len <= 1 || bcmp(p, "/*", 2))
		return;
	do {
		for (; len; --len, ++p)
			if (p[0] == '*' && len > 1 && p[1] == '/') {
				ep->top = ep->lno = lno;
				return;
			}
	} while ((p = file_gline(ep, ++lno, &len)) != NULL);
	return;
}
