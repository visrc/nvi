/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: exf.c,v 5.49 1993/03/25 14:59:02 bostic Exp $ (Berkeley) $Date: 1993/03/25 14:59:02 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "log.h"
#include "options.h"
#include "screen.h"
#include "pathnames.h"

static void file_def __P((EXF *));

/*
 * file_ins --
 *	Insert a new file into the list of files before or after the
 *	specified file.
 */
int
file_ins(sp, ep, name, append)
	SCR *sp;
	EXF *ep;
	char *name;
	int append;
{
	EXF *tep;

	if ((tep = malloc(sizeof(EXF))) == NULL)
		goto mem1;
	file_def(tep);

	if ((tep->name = strdup(name)) == NULL)
		goto mem2;
	tep->nlen = strlen(tep->name);
	
	if (append) {
		INSERT_TAIL(tep, ep, next, prev, EXF);
	} else {
		INSERT_HEAD(tep, ep, next, prev, EXF);
	}
	return (0);

mem2:	free(tep);
mem1:	msgq(sp, M_ERROR, "Error: %s", strerror(errno));
	return (1);
}

/*
 * file_set --
 *	Add an argc/argv set of files into the file list.
 */
int
file_set(sp, argc, argv)
	SCR *sp;
	int argc;
	char *argv[];
{
	EXF *tep;

	/* Insert new entries at the tail of the list. */
	for (; *argv; ++argv) {
		if ((tep = malloc(sizeof(EXF))) == NULL)
			goto mem1;
		file_def(tep);
		if ((tep->name = strdup(*argv)) == NULL)
			goto mem2;
		tep->nlen = strlen(tep->name);
		INSERT_TAIL(tep, (EXF *)&sp->gp->exfhdr, next, prev, EXF);
	}
	return (0);

mem2:	free(tep);
mem1:	msgq(sp, M_ERROR, "Error: %s", strerror(errno));
	return (1);
}

/*
 * file_first --
 *	Return the first file.
 */
EXF *
file_first(sp, all)
	SCR *sp;
	int all;
{
	EXF *tep;

	for (tep = sp->gp->exfhdr.next;;) {
		if (tep == (EXF *)&sp->gp->exfhdr)
			return (NULL);
		if (!all && F_ISSET(tep, F_IGNORE))
			continue;
		return (tep);
	}
	/* NOTREACHED */
}

/*
 * file_next --
 *	Return the next file, if any.
 */
EXF *
file_next(sp, ep, all)
	SCR *sp;
	EXF *ep;
	int all;
{
	for (;;) {
		ep = ep->next;
		if (ep == (EXF *)&sp->gp->exfhdr)
			return (NULL);
		if (!all && F_ISSET(ep, F_IGNORE))
			continue;
		return (ep);
	}
	/* NOTREACHED */
}

/*
 * file_prev --
 *	Return the previous file, if any.
 */
EXF *
file_prev(sp, ep, all)
	SCR *sp;
	EXF *ep;
	int all;
{
	for (;;) {
		ep = ep->prev;
		if (ep == (EXF *)&sp->gp->exfhdr)
			return (NULL);
		if (!all && F_ISSET(ep, F_IGNORE))
			continue;
		return (ep);
	}
	/* NOTREACHED */
}

/*
 * file_locate --
 *	Return the appropriate structure if we've seen this file before.
 */
EXF *
file_locate(sp, name)
	SCR *sp;
	char *name;
{
	EXF *tep;

	for (tep = sp->gp->exfhdr.next;
	    tep != (EXF *)&sp->gp->exfhdr; tep = tep->next)
		if (!memcmp(tep->name, name, tep->nlen))
			return (tep);
	return (NULL);
}

/*
 * file_start --
 *	Start editing a file.
 */
EXF *
file_start(sp, ep)
	SCR *sp;
	EXF *ep;
{
	struct stat sb;
	u_int addflags;
	int fd;
	char *openname, tname[sizeof(_PATH_TMPNAME) + 1];

	fd = -1;
	addflags = 0;
	if (ep == NULL || stat(ep->name, &sb)) { 
		(void)strcpy(tname, _PATH_TMPNAME);
		if ((fd = mkstemp(tname)) == -1) {
			msgq(sp, M_ERROR,
			    "Temporary file: %s", strerror(errno));
			return (NULL);
		}

		if (ep == NULL) {
			if (file_ins(sp, (EXF *)&sp->gp->exfhdr, tname, 1))
				return (NULL);
			if ((ep = file_first(sp, 1)) == NULL)
				return (NULL);
			addflags |= F_NONAME;
		}

		/*
		 * The temporary name should appear in the file system, given
		 * that we display it to the user (if they haven't named the
		 * file somehow).  Save off the temporary name so we can later
		 * unlink the file, even if the user has changed the name.  If
		 * that doesn't work, unlink the file now, and the user just
		 * won't be able to access the file outside of vi.
		 */
		if ((ep->tname = strdup(tname)) == NULL)
			(void)unlink(tname);
		openname = tname;
	} else
		openname = ep->name;

	/* Open a db structure. */
	ep->db = dbopen(openname, O_CREAT | O_EXLOCK | O_NONBLOCK| O_RDONLY,
	    DEFFILEMODE, DB_RECNO, NULL);
	if (ep->db == NULL && errno == EAGAIN) {
		addflags |= F_RDONLY;
		ep->db = dbopen(openname, O_CREAT | O_NONBLOCK | O_RDONLY,
		    DEFFILEMODE, DB_RECNO, NULL);
		if (ep->db != NULL)
			msgq(sp, M_ERROR,
			    "%s already locked, session is read-only",
			    ep->name);
	}
	if (ep->db == NULL) {
		msgq(sp, M_ERROR, "%s: %s", ep->name, strerror(errno));
		return (NULL);
	}
	if (fd != -1)
		(void)close(fd);

	/* Only a few bits are retained between edit instances. */
	ep->flags &= F_RETAINMASK;
	F_SET(ep, addflags);

	/* Flush the line caches. */
	ep->c_lno = ep->c_nlines = OOBLNO;

	/* Start logging. */
	log_init(sp, ep);

	/*
	 * Reset any marks.
	 *
	 * XXX
	 * This shouldn't be here, but don't know where else to put it.
	 */
	mark_reset(sp, ep);

	return (ep);
}

/*
 * file_stop --
 *	Stop editing a file.
 */
int
file_stop(sp, ep, force)
	SCR *sp;
	EXF *ep;
	int force;
{
	/* Close the db structure. */
	if ((ep->db->close)(ep->db) && !force) {
		msgq(sp, M_ERROR, "%s: close: %s", ep->name, strerror(errno));
		return (1);
	}

	/*
	 * Committed to the close.
	 *
	 * Stop logging.
	 */
	(void)log_end(sp, ep);

	/* Unlink any temporary file. */
	if (ep->tname != NULL && unlink(ep->tname)) {
		msgq(sp, M_ERROR, "%s: remove: %s", ep->tname, strerror(errno));
		free(ep->tname);
	}

	return (0);
}

/*
 * file_sync --
 *	Sync the file to disk.
 */
int
file_sync(sp, ep, force)
	SCR *sp;
	EXF *ep;
	int force;
{
	struct stat sb;
	FILE *fp;
	MARK from, to;
	int fd;

	/* If file not modified, we're done. */
	if (!F_ISSET(ep, F_MODIFIED))
		return (0);

	/*
	 * Don't permit writing to temporary files.  The problem is that
	 * if it's a temp file and the user does ":wq", then we write and
	 * quit, unlinking the temporary file.  Not what the user had in
	 * mind at all.
	 */
	if (F_ISSET(ep, F_NONAME)) {
		msgq(sp, M_ERROR, "No filename to which to write.");
		return (1);
	}

	/* Can't write if read-only. */
	if ((ISSET(O_READONLY) || F_ISSET(ep, F_RDONLY)) && !force) {
		msgq(sp, M_ERROR,
		    "Read-only file, not written; use ! to override.");
		return (1);
	}

	/*
	 * If the name was changed, normal rules apply, i.e. don't overwrite 
	 * unless forced.
	 */
	if (F_ISSET(ep, F_NAMECHANGED) && !force && !stat(ep->name, &sb)) {
		msgq(sp, M_ERROR,
		    "%s exists, not written; use ! to override.", ep->name);
		return (1);
	}

	/* Open the file, truncating its contents. */
	if ((fd = open(ep->name,
	    O_CREAT | O_TRUNC | O_WRONLY, DEFFILEMODE)) < 0)
		goto err;

	/* Use stdio for buffering. */
	if ((fp = fdopen(fd, "w")) == NULL) {
		(void)close(fd);
err:		msgq(sp, M_ERROR, "%s: %s", ep->name, strerror(errno));
		return (1);
	}

	/* Build fake addresses. */
	from.lno = 1;
	from.cno = 0;
	to.lno = file_lline(sp, ep);
	to.cno = 0;

	/* Use the underlying ex write routines. */
	if (ex_writefp(sp, ep, ep->name, fp, &from, &to, 1))
		return (1);

	F_CLR(ep, F_MODIFIED);
	return (0);
}

/*
 * file_def --
 *	Fill in a default EXF structure.
 */
static void
file_def(ep)
	EXF *ep;
{
	memset(ep, 0, sizeof(EXF));

	ep->c_lno = OOBLNO;
	ep->olno = OOBLNO;
	ep->lno = 1;
	ep->cno = ep->ocno = 0;
	ep->l_ltype = LOG_NOTYPE;

	F_SET(ep, F_NEWSESSION);
}
