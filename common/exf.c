/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: exf.c,v 5.47 1993/02/28 14:19:35 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:19:35 $";
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

static EXFLIST exfhdr;				/* List of files. */

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
		goto mem1;
	exf_def(nep);

	if ((SCRP(nep) = malloc(sizeof(SCR))) == NULL)
		goto mem2;
	scr_def(SCRP(nep));

	if ((nep->name = strdup(name)) == NULL)
		goto mem3;
	nep->nlen = strlen(nep->name);
	
	if (append) {
		insexf(nep, ep);
	} else {
		instailexf(nep, ep);
	}
	return (0);

mem3:	free(SCRP(nep));
mem2:	free(nep);
mem1:	ep->msg(ep, M_ERROR, "Error: %s", strerror(errno));
	return (1);
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

	/* Insert new entries at the tail of the list. */
	for (; *argv; ++argv) {
		if ((ep = malloc(sizeof(EXF))) == NULL)
			goto mem1;
		exf_def(ep);
		if ((SCRP(ep) = malloc(sizeof(SCR))) == NULL)
			goto mem2;
		scr_def(SCRP(ep));
		if ((ep->name = strdup(*argv)) == NULL)
			goto mem3;
		ep->nlen = strlen(ep->name);
		instailexf(ep, &exfhdr);
	}
	return (0);

mem3:	free(SCRP(ep));
mem2:	free(ep);
mem1:	ep->msg(ep, M_ERROR, "Error: %s", strerror(errno));
	return (1);
}

/*
 * file_first --
 *	Return the first file.
 */
EXF *
file_first(all)
	int all;
{
	EXF *ep;

	for (ep = exfhdr.next;;) {
		if (ep == (EXF *)&exfhdr)
			return (NULL);
		if (!all && FF_ISSET(ep, F_IGNORE))
			continue;
		return (ep);
	}
	/* NOTREACHED */
}

/*
 * file_next --
 *	Return the next file, if any.
 */
EXF *
file_next(ep, all)
	EXF *ep;
	int all;
{
	for (;;) {
		ep = ep->next;
		if (ep == (EXF *)&exfhdr)
			return (NULL);
		if (!all && FF_ISSET(ep, F_IGNORE))
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
file_prev(ep, all)
	EXF *ep;
	int all;
{
	for (;;) {
		ep = ep->prev;
		if (ep == (EXF *)&exfhdr)
			return (NULL);
		if (!all && FF_ISSET(ep, F_IGNORE))
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
file_locate(name)
	char *name;
{
	EXF *p;

	for (p = exfhdr.next; p != (EXF *)&exfhdr; p = p->next)
		if (!memcmp(p->name, name, p->nlen))
			return (p);
	return (NULL);
}

/*
 * file_start --
 *	Start editing a file.
 */
EXF *
file_start(ep)
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
			ep->msg(ep, M_ERROR,
			    "Temporary file: %s", strerror(errno));
			return (NULL);
		}

		if (ep == NULL) {
			if (file_ins((EXF *)&exfhdr, tname, 1))
				return (NULL);
			if ((ep = file_first(1)) == NULL)
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
			ep->msg(ep, M_ERROR,
			    "%s already locked, session is read-only",
			    ep->name);
	}
	if (ep->db == NULL) {
		ep->msg(ep, M_ERROR, "%s: %s", ep->name, strerror(errno));
		return (NULL);
	}
	if (fd != -1)
		(void)close(fd);

	/* Only a few bits are retained between edit instances. */
	ep->flags &= F_RETAINMASK;
	FF_SET(ep, addflags);

	/* Flush the line caches. */
	ep->c_lno = ep->c_nlines = OOBLNO;

	/* Start logging. */
	log_init(ep);

	/*
	 * Reset any marks.
	 * XXX
	 * Not sure this should be here, but don't know where else to put
	 * it, either.
	 */
	mark_reset(ep);

	return (ep);
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
	/* Clean up the session, if necessary. */
	if (ep->end && ep->end(ep))
		return (1);

	/* Close the db structure. */
	if ((ep->db->close)(ep->db) && !force) {
		ep->msg(ep, M_ERROR,
		    "%s: close: %s", ep->name, strerror(errno));
		return (1);
	}

	/* Stop logging. */
	log_end(ep);

	/* Unlink any temporary file, ignore any error. */
	if (ep->tname != NULL && unlink(ep->tname)) {
		ep->msg(ep, M_ERROR,
		    "%s: remove: %s", ep->tname, strerror(errno));
		free(ep->tname);
	}

	/* Delete temporary space. */
	if (ep->tmp_bp) {
		free(ep->tmp_bp);
		ep->tmp_bp = NULL;
		ep->tmp_blen = 0;
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
	MARK from, to;
	int fd;

	/* If not modified, we're done. */
	if (!FF_ISSET(ep, F_MODIFIED))
		return (0);

	/* Can't write the temporary file. */
	if (FF_ISSET(ep, F_NONAME)) {
		ep->msg(ep, M_ERROR, "No filename to which to write.");
		return (1);
	}

	/* Can't write if read-only. */
	if ((ISSET(O_READONLY) || FF_ISSET(ep, F_RDONLY)) && !force) {
		ep->msg(ep, M_ERROR,
		    "Read-only file, not written; use ! to override.");
		return (1);
	}

	/*
	 * If the name was changed, normal rules apply, i.e. don't overwrite 
	 * unless forced.
	 */
	if (FF_ISSET(ep, F_NAMECHANGED) && !force && !stat(ep->name, &sb)) {
		ep->msg(ep, M_ERROR,
		    "%s exists, not written; use ! to override.", ep->name);
		return (1);
	}

	if ((fd = open(ep->name,
	    O_CREAT | O_TRUNC | O_WRONLY, DEFFILEMODE)) < 0)
		goto err;

	if ((fp = fdopen(fd, "w")) == NULL) {
		(void)close(fd);
err:		ep->msg(ep, M_ERROR, "%s: %s", ep->name, strerror(errno));
		return (1);
	}

	from.lno = 1;
	from.cno = 0;
	to.lno = file_lline(ep);
	to.cno = 0;
	if (ex_writefp(ep, ep->name, fp, &from, &to, 1))
		return (1);

	FF_CLR(ep, F_MODIFIED);
	return (0);
}

/*
 * file_switch --
 *	Switch files.
 */
EXF *
file_switch(ep, force)
	EXF *ep;
	int force;
{
	EXF *sp, tmp;
	sigset_t bmask, omask;

	/*
	 * This is (theoretically) the only time in which the ep structure
	 * can be inconsistent, or, we have more than a single ep structure
	 * open.  Block the standard user generated signals.
	 */
	sigemptyset(&bmask);
	sigaddset(&bmask, SIGHUP);
	sigaddset(&bmask, SIGINT);
	(void)sigprocmask(SIG_BLOCK, &bmask, &omask);

	/*
	 * There's this nasty little problem in that we might be editing the
	 * same file again.  Tags, "e!", and "rew" are all possible paths into
	 * that morass.  To avoid having to figure it out, we copy the "to"
	 * EXF structure into a temporary space, and open up an instance of
	 * the file, then close the from file, and copy the temporary space
	 * back into the "to" file.  There's always a valid EXF structure,
	 * and we don't have to figure out if we're reinitializing anything.
	 */
	if (ep->enext == NULL)
		sp = file_start(ep->enext);
	else {
		tmp = *ep->enext;
		sp = file_start(&tmp);
	}
	if (sp == NULL)
		return (NULL);

	/* Copy what we need from the last file. */
	sp->scrp = ep->scrp;
	sp->msgp = ep->msgp;
	sp->flags |= ep->flags & F_COPYMASK;

	/* Link the edit chain. */
	sp->eprev = FF_ISSET(ep, F_DUMMY) ? NULL : ep;

	if (!FF_ISSET(ep, F_DUMMY) && file_stop(ep, force)) {
		FF_SET(sp, F_IGNORE);
		(void)file_stop(sp, 1);
		sp = NULL;
	}

	if (ep->enext == NULL)
		ep->enext = sp;
	else {
		*ep->enext = *sp;
		sp = ep->enext;
	}

	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	return (sp);
}

/*
 * exf_def --
 *	Fill in a default EXF structure.  It's a function because I
 *	just got tired of getting burned 'cause the structure changed.
 */
void
exf_def(ep)
	EXF *ep;
{
	memset(ep, 0, sizeof(EXF));
	ep->c_lno = OOBLNO;
	ep->stdfp = stdout;
	ep->searchdir = NOTSET;
	FF_SET(ep, F_NEWSESSION);
}
