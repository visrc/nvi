/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: exf.c,v 5.54 1993/04/17 11:44:18 bostic Exp $ (Berkeley) $Date: 1993/04/17 11:44:18 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

static void file_def __P((EXF *));

/*
 * file_get --
 *	Return the appropriate structure if we've seen this file before,
 *	otherwise insert a new file into the list of files before or after
 *	the specified file.  If the file name is NULL, create a temporary
 *	file for editing.
 */
EXF *
file_get(sp, ep, name, append)
	SCR *sp;
	EXF *ep;
	char *name;
	int append;
{
	EXF *tep;
	struct stat sb;
	int fd;
	char tname[sizeof(_PATH_TMPNAME) + 1];

	/*
	 * Check for the file.  Ignore files without names, but check
	 * F_IGNORE files, in case of re-editing.  If the file is in
	 * play, up the reference count and return.
	 */
	if (name != NULL)
		for (tep = sp->gp->exfhdr.next;
		    tep != (EXF *)&sp->gp->exfhdr; tep = tep->next)
			if (!memcmp(tep->name, name, tep->nlen)) {
				if (tep->refcnt != 0)
					return (tep);
				break;
			}

	/* If not found, build an entry for it. */
	if (name == NULL || tep == (EXF *)&sp->gp->exfhdr) {
		if ((tep = malloc(sizeof(EXF))) == NULL)
			goto e1;
		file_def(tep);

		/* Insert into the chain of files. */
		if (append) {
			HDR_APPEND(tep, ep, next, prev, EXF);
		} else {
			HDR_INSERT(tep, ep, next, prev, EXF);
		}

		/* Ignore all files, by default. */
		F_SET(tep, F_IGNORE);
	}

	/*
	 * If no backing file, create a backing temporary file.
	 * Save the temp name so can later unlink it.
	 */
	if (name == NULL || stat(name, &sb)) {
		(void)strcpy(tname, _PATH_TMPNAME);
		if ((fd = mkstemp(tname)) == -1) {
			msgq(sp, M_ERR,
			    "Temporary file: %s", strerror(errno));
			goto e2;
		}
		(void)close(fd);
		if ((tep->tname = strdup(tname)) == NULL) {
			(void)unlink(tname);
			goto e2;
		}
	}

	/*
	 * If no name, point the name at the temporary name (we display
	 * it to the user until they rename it.
	 */
	if (name == NULL) {
		F_SET(tep, F_NONAME);
		tep->name = tep->tname;
	} else if ((tep->name = strdup(name)) == NULL)
		goto e2;
	tep->nlen = strlen(tep->name);
	
	return (tep);

e2:	HDR_DELETE(tep, next, prev, EXF);
	free(tep);
e1:	msgq(sp, M_ERR, "Error: %s", strerror(errno));
	return (NULL);
}

/*
 * file_set --
 *	Append an argc/argv set of files to the file list.
 */
int
file_set(sp, argc, argv)
	SCR *sp;
	int argc;
	char *argv[];
{
	EXF *ep;

	for (; *argv; ++argv) {
		if ((ep =
		    file_get(sp, (EXF *)&sp->gp->exfhdr, *argv, 1)) == NULL)
			return (1);
		F_CLR(ep, F_IGNORE);
	}
	return (0);
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
	while ((ep = ep->next) != (EXF *)&sp->gp->exfhdr)
		if (all || !F_ISSET(ep, F_IGNORE))
			return (ep);
	return (NULL);
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
	while ((ep = ep->prev) != (EXF *)&sp->gp->exfhdr)
		if (all || !F_ISSET(ep, F_IGNORE))
			return (ep);
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
	char *oname;

	if (ep == NULL &&
	    (ep = file_get(sp, (EXF *)&sp->gp->exfhdr, NULL, 1)) == NULL)
		return (NULL);

	oname = ep->tname == NULL ? ep->name : ep->tname;

	/* Open a db structure. */
	ep->db = dbopen(oname, O_EXLOCK | O_NONBLOCK| O_RDONLY,
	    DEFFILEMODE, DB_RECNO, NULL);
	if (ep->db == NULL && errno == EAGAIN) {
		F_SET(ep, F_RDONLY);
		ep->db = dbopen(oname, O_NONBLOCK | O_RDONLY,
		    DEFFILEMODE, DB_RECNO, NULL);
		if (ep->db != NULL)
			msgq(sp, M_INFO,
			    "%s already locked, session is read-only", oname);
	}
	if (ep->db == NULL) {
		msgq(sp, M_ERR, "%s: %s", oname, strerror(errno));
		return (NULL);
	}

	if (ep->refcnt == 0) {
		/* Flush the line caches. */
		ep->c_lno = ep->c_nlines = OOBLNO;

		/* Start logging. */
		log_init(sp, ep);

		/*
		 * Reset any marks.
		 *
		 * XXX
		 * This shouldn't be here.
		 */
		mark_reset(sp, ep);
	}
	++ep->refcnt;
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
	if (--ep->refcnt != 0)
		return (0);

	/* Close the db structure. */
	if ((ep->db->close)(ep->db) && !force) {
		msgq(sp, M_ERR, "%s: close: %s", ep->name, strerror(errno));
		return (1);
	}

	/*
	 * Committed to the close.
	 *
	 * Stop logging.
	 */
	(void)log_end(sp, ep);

	/* Unlink any temporary file. */
	if (ep->tname != NULL) {
		if (unlink(ep->tname))
			msgq(sp, M_ERR,
			    "%s: remove: %s", ep->tname, strerror(errno));
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

	/*
	 * Don't permit writing to temporary files.  The problem is that
	 * if it's a temp file and the user does ":wq", we write and quit,
	 * unlinking the temporary file.  Not what the user had in mind
	 * at all.
	 */
	if (F_ISSET(ep, F_NONAME)) {
		msgq(sp, M_ERR, "No filename to which to write.");
		return (1);
	}

	/* Can't write if read-only. */
	if ((O_ISSET(sp, O_READONLY) || F_ISSET(ep, F_RDONLY)) && !force) {
		msgq(sp, M_ERR,
		    "Read-only file, not written; use ! to override.");
		return (1);
	}

	/*
	 * If the name was changed, normal rules apply, i.e. don't overwrite 
	 * unless forced.
	 */
	if (F_ISSET(ep, F_NAMECHANGED) && !force && !stat(ep->name, &sb)) {
		msgq(sp, M_ERR,
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
err:		msgq(sp, M_ERR, "%s: %s", ep->name, strerror(errno));
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
	ep->l_ltype = LOG_NOTYPE;
	F_SET(ep, F_NOSETPOS);
}
