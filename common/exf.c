/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: exf.c,v 8.42 1993/11/18 08:17:00 bostic Exp $ (Berkeley) $Date: 1993/11/18 08:17:00 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

/*
 * We include <sys/file.h>, because the flock(2) #defines were
 * found there on historical systems.  We also include <fcntl.h>
 * because the open(2) #defines are found there on newer systems.
 */
#include <sys/file.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "pathnames.h"

/*
 * file_add --
 *	Insert a file name into the FREF list, if it doesn't already
 *	appear in it.
 *
 * XXX
 * The "if they don't already appear" changes vi's semantics slightly.
 * If you do a "vi foo bar", and then execute "next bar baz", the edit
 * of bar will reflect the line/column of the previous edit session.
 * This seems reasonable to me, and is a logical extension of the change
 * where vi now remembers the last location in any file that it has ever
 * edited, not just the previously edited file.
 */
FREF *
file_add(sp, frp_append, fname, ignore)
	SCR *sp;
	FREF *frp_append;
	char *fname;
	int ignore;
{
	FREF *frp;

	/*
	 * Ignore if it already exists, with the exception that we turn off
	 * the ignore bit.  (The sequence is that the file was already part
	 * of an argument list, and had the ignore bit set as part of adding
	 * a new argument list.
	 */
	if (fname != NULL)
		for (frp = sp->frefq.tqh_first;
		    frp != NULL; frp = frp->q.tqe_next)
			if (!strcmp(frp->fname, fname)) {
				F_CLR(frp, FR_IGNORE);
				return (frp);
			}

	/* Allocate and initialize the FREF structure. */
	if ((frp = malloc(sizeof(FREF))) == NULL)
		goto mem;
	memset(frp, 0, sizeof(FREF));

	/*
	 * If no file name specified, set the appropriate flag.
	 *
	 * XXX
	 * This had better be *closely* followed by a file_init
	 * so something gets filled in!
	 */
	if (fname == NULL)
		F_SET(frp, FR_NONAME);
	else if ((frp->fname = strdup(fname)) == NULL) {
		FREE(frp, sizeof(FREF));
mem:		msgq(sp, M_SYSERR, NULL);
		return (NULL);
	} else
		frp->nlen = strlen(fname);

	/* Only the initial argument list is "remembered". */
	if (ignore)
		F_SET(frp, FR_IGNORE);

	/* Append into the chain of file names. */
	if (frp_append != NULL) {
		TAILQ_INSERT_AFTER(&sp->frefq, frp_append, frp, q);
	} else
		TAILQ_INSERT_TAIL(&sp->frefq, frp, q);

	return (frp);
}

/*
 * file_first --
 *	Return the first file name for editing, if any.
 */
FREF *
file_first(sp, all)
	SCR *sp;
	int all;
{
	FREF *frp;

	/* Return the first file name. */
	for (frp = sp->frefq.tqh_first; frp != NULL; frp = frp->q.tqe_next)
		if (all || !F_ISSET(frp, FR_IGNORE))
			return (frp);
	return (NULL);
}

/*
 * file_next --
 *	Return the next file name, if any.
 */
FREF *
file_next(sp, all)
	SCR *sp;
	int all;
{
	FREF *frp;

	/* Return the next file name. */
	for (frp = sp->frefq.tqh_first; frp != NULL; frp = frp->q.tqe_next)
		if (all || !F_ISSET(frp, FR_EDITED | FR_IGNORE))
			return (frp);
	return (NULL);
}

/*
 * file_init --
 *	Start editing a file, based on the FREF structure.  If successsful,
 *	let go of any previous file.  Don't release the previous file until
 *	absolutely sure we have the new one.
 */
int
file_init(sp, frp, rcv_fname, force)
	SCR *sp;
	FREF *frp;
	char *rcv_fname;
	int force;
{
	EXF *ep;
	RECNOINFO oinfo;
	struct stat sb;
	size_t psize;
	int fd;
	char *oname, tname[sizeof(_PATH_TMPNAME) + 1];

	/* Create the EXF. */
	if ((ep = malloc(sizeof(EXF))) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}

	/*
	 * Required ep initialization:
	 *	Flush the line caches.
	 *	Default recover mail file fd to -1.
	 *	Set initial EXF flag bits.
	 */
	memset(ep, 0, sizeof(EXF));
	ep->c_lno = ep->c_nlines = OOBLNO;
	ep->rcv_fd = -1;
	F_SET(ep, F_FIRSTMODIFY);

	/*
	 * If no name or backing file, create a backing temporary file, saving
	 * the temp file name so can later unlink it.  Repoint the name to the
	 * temporary name (we display it to the user until they rename it).
	 * There are some games we play with the FR_FREE_TNAME and FR_NONAME
	 * flags (see ex/ex_file.c) to make sure that the temporary memory gets
	 * free'd up.
	 */
	if (frp->fname == NULL || stat(frp->fname, &sb)) {
		(void)strcpy(tname, _PATH_TMPNAME);
		if ((fd = mkstemp(tname)) == -1) {
			msgq(sp, M_SYSERR, "Temporary file");
			goto err;
		}
		(void)close(fd);
		F_SET(frp, FR_UNLINK_TFILE);

		if ((frp->tname = strdup(tname)) == NULL) {
			(void)unlink(tname);
			msgq(sp, M_SYSERR, NULL);
			goto err;
		}
		if (frp->fname == NULL) {
			F_SET(frp, FR_NONAME);
			frp->fname = frp->tname;
			frp->nlen = strlen(frp->fname);
		} else
			F_SET(frp, FR_FREE_TNAME);
		oname = frp->tname;
		psize = 4 * 1024;

		F_SET(frp, FR_NEWFILE);
	} else {
		oname = frp->fname;

		/* Try to keep it at 10 pages or less per file. */
		if (sb.st_size < 40 * 1024)
			psize = 4 * 1024;
		else if (sb.st_size < 320 * 1024)
			psize = 32 * 1024;
		else
			psize = 64 * 1024;

		frp->mtime = sb.st_mtime;
	}
	
	/* Set up recovery. */
	memset(&oinfo, 0, sizeof(RECNOINFO));
	oinfo.bval = '\n';			/* Always set. */
	oinfo.psize = psize;
	oinfo.flags = F_ISSET(sp->gp, G_SNAPSHOT) ? R_SNAPSHOT : 0;
	if (rcv_fname == NULL) {
		if (rcv_tmp(sp, ep, frp->fname))
			msgq(sp, M_ERR,
		    "Modifications not recoverable if the system crashes.");
		else
			oinfo.bfname = ep->rcv_path;
	} else if ((ep->rcv_path = strdup(rcv_fname)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		goto err;
	} else {
		oinfo.bfname = ep->rcv_path;
		F_SET(ep, F_MODIFIED);
	}

	/* Open a db structure. */
	if ((ep->db = dbopen(rcv_fname == NULL ? oname : NULL,
	    O_NONBLOCK | O_RDONLY, DEFFILEMODE, DB_RECNO, &oinfo)) == NULL) {
		msgq(sp, M_SYSERR, oname);
		goto err;
	}

	/* Init file marks. */
	if (mark_init(sp, ep)) {
		msgq(sp, M_SYSERR, NULL);
		goto err;
	}

	/*
	 * The -R flag, or doing a "set readonly" during a session causes
	 * all files edited during the session (using an edit command, or
	 * even using tags) to be marked read-only.  Note that changing
	 * the file name (see ex/ex_file.c), clears this flag.
	 *
	 * Otherwise, try and figure out if a file is readonly.  This is
	 * a dangerous thing to do.  The kernel is the only arbiter of
	 * whether or not a file is writeable, and the best that a user
	 * program can do is guess.  Obvious loopholes are files that are
	 * on a file system mounted readonly (access catches this one some
	 * systems), or other protection mechanisms, ACL's for example,
	 * that we can't portably check.
	 *
	 * !!!
	 * Historic vi displayed the readonly message if none of the file
	 * write bits were set, or if an an access(2) call on the path
	 * failed.  This seems reasonable.  If the file is mode 444, root
	 * users may want to know that the owner of the file did not expect
	 * it to be written.
	 *
	 * Historic vi set the readonly bit if no write bits were set for
	 * a file, even if the access call would have succeeded.  This makes
	 * the superuser force the write even when vi expects that it will
	 * succeed.  I'm less supportive of this semantic, but it's historic
	 * practice and the conservative approach to vi'ing files as root.
	 *
	 * It would be nice if there was some way to update this when the user
	 * does a "^Z; chmod ...".  The problem is that we'd first have to
	 * distinguish between readonly bits set because of file permissions
	 * and those set for other reasons.  That's not too hard, but deciding
	 * when to reevaluate the permissions is trickier.  An alternative
	 * might be to turn off the readonly bit if the user forces a write
	 * and it succeeds.
	 *
	 * XXX
	 * Access(2) doesn't consider the effective uid/gid values.  This
	 * probably isn't a problem for vi when it's running standalone.
	 */
	F_CLR(frp, FR_RDONLY);
	if (O_ISSET(sp, O_READONLY) || !F_ISSET(frp, FR_NEWFILE) &&
	    (!(sb.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) ||
	    access(frp->fname, W_OK)))
		F_SET(frp, FR_RDONLY);

	/* Start logging. */
	log_init(sp, ep);

	++ep->refcnt;

	/* Close the previous file; if that fails, close the new one. */
	if (sp->ep != NULL && file_end(sp, sp->ep, force)) {
		(void)file_end(sp, ep, 1);
		return (1);
	}

	/*
	 * 4.4BSD supports locking in the open call, other systems don't.
	 * Since the user can't interrupt us between the open and here,
	 * it's a don't care.
	 *
	 * !!!
	 * We need to distinguish a lock not being available for the file
	 * from the file system not supporting locking.  Assume that EAGAIN
	 * is the former.  There isn't a portable way to do this.
	 *
	 * XXX
	 * The locking is flock(2) style, not fcntl(2).  The latter is known
	 * to fail badly on various systems, and its only advantage is that
	 * it sometimes works over NFS.
	 */
	if (flock(ep->db->fd(ep->db), LOCK_EX | LOCK_NB))
		if (errno == EAGAIN) {
			msgq(sp, M_INFO,
			    "%s already locked, session is read-only", oname);
			F_SET(frp, FR_RDONLY);
		} else
			msgq(sp, M_VINFO, "%s cannot be locked", oname);

	/* Switch... */
	sp->ep = ep;
	if ((sp->p_frp = sp->frp) != NULL)
		set_alt_fname(sp, sp->p_frp->fname);
	sp->frp = frp;
	return (0);

err:	if (ep->rcv_path != NULL) {
		FREE(ep->rcv_path, strlen(ep->rcv_path));
		ep->rcv_path = NULL;
	}
	if (F_ISSET(frp, FR_FREE_TNAME))
		FREE(frp->tname, strlen(frp->tname));
	FREE(ep, sizeof(EXF));
	return (1);
}

/*
 * file_end --
 *	Stop editing a file.
 *
 *	NB: sp->ep MAY NOT BE THE SAME AS THE ARGUMENT ep, SO DON'T
 *	    USE IT!
 */
int
file_end(sp, ep, force)
	SCR *sp;
	EXF *ep;
	int force;
{
	FREF *frp;

	/*
	 * Save the cursor location.
	 *
	 * XXX
	 * It would be cleaner to do this somewhere else, but by the time
	 * ex or vi know that we're changing files it's already happened.
	 */
	frp = sp->frp;
	frp->lno = sp->lno;
	frp->cno = sp->cno;
	F_SET(frp, FR_CURSORSET);

	/* If multiply referenced, decrement count and return. */
	if (--ep->refcnt != 0)
		return (0);

	/* Close the db structure. */
	if (ep->db->close != NULL && ep->db->close(ep->db) && !force) {
		msgq(sp, M_ERR, "%s: close: %s", frp->fname, strerror(errno));
		return (1);
	}

	/* COMMITTED TO THE CLOSE.  THERE'S NO GOING BACK... */

	/*
	 * Delete the recovery files, close the open descriptor,
	 * free recovery memory.
	 */
	if (!F_ISSET(ep, F_RCV_NORM)) {
		if (ep->rcv_path != NULL && unlink(ep->rcv_path))
			msgq(sp, M_ERR,
			    "%s: remove: %s", ep->rcv_path, strerror(errno));
		if (ep->rcv_mpath != NULL && unlink(ep->rcv_mpath))
			msgq(sp, M_ERR,
			    "%s: remove: %s", ep->rcv_mpath, strerror(errno));
	}
	if (ep->rcv_fd != -1)
		(void)close(ep->rcv_fd);
	if (ep->rcv_path != NULL)
		FREE(ep->rcv_path, strlen(ep->rcv_path));
	if (ep->rcv_mpath != NULL)
		FREE(ep->rcv_mpath, strlen(ep->rcv_mpath));

	/* Stop logging. */
	(void)log_end(sp, ep);

	/*
	 * Unlink any temporary file, and, if FR_FREE_TNAME set, free
	 * the memory referenced by tname.
	 */
	if (F_ISSET(frp, FR_UNLINK_TFILE)) {
		F_CLR(frp, FR_UNLINK_TFILE);
		if (unlink(frp->tname))
			msgq(sp, M_ERR,
			    "%s: remove: %s", frp->tname, strerror(errno));
		if (F_ISSET(frp, FR_FREE_TNAME)) {
			F_CLR(frp, FR_FREE_TNAME);
			FREE(frp->tname, strlen(frp->tname));
		}
	}

	/* Free up any marks. */
	mark_end(sp, ep);

	/* Free the EXF structure. */
	FREE(ep, sizeof(EXF));
	return (0);
}

/*
 * file_write --
 *	Write the file to disk.  Historic vi had fairly convoluted
 *	semantics for whether or not writes would happen.  That's
 *	why all the flags.
 */
int
file_write(sp, ep, fm, tm, fname, flags)
	SCR *sp;
	EXF *ep;
	MARK *fm, *tm;
	char *fname;
	int flags;
{
	struct stat sb;
	FILE *fp;
	MARK from, to;
	u_long nlno, nch;
	int fd, oflags;
	char *msg;

	/*
	 * Don't permit writing to temporary files.  The problem is that
	 * if it's a temp file, and the user does ":wq", we write and quit,
	 * unlinking the temporary file.  Not what the user had in mind
	 * at all.  This test cannot be forced.
	 */
	if (fname == NULL && F_ISSET(sp->frp, FR_NONAME)) {
		msgq(sp, M_ERR, "No filename to which to write.");
		return (1);
	}

	/* Can't write files marked read-only, unless forced. */
	if (!LF_ISSET(FS_FORCE) &&
	    fname == NULL && F_ISSET(sp->frp, FR_RDONLY)) {
		if (LF_ISSET(FS_POSSIBLE))
			msgq(sp, M_ERR,
			    "Read-only file, not written; use ! to override.");
		else
			msgq(sp, M_ERR,
			    "Read-only file, not written.");
		return (1);
	}

	/* If not forced, not appending, and "writeany" not set ... */
	if (!LF_ISSET(FS_FORCE | FS_APPEND) && !O_ISSET(sp, O_WRITEANY)) {
		/* Don't overwrite anything but the original file. */
		if (fname != NULL && !stat(fname, &sb) ||
		    F_ISSET(sp->frp, FR_NAMECHANGED) &&
		    !stat(sp->frp->fname, &sb)) {
			if (fname == NULL)
				fname = sp->frp->fname;
			if (LF_ISSET(FS_POSSIBLE))
				msgq(sp, M_ERR,
		"%s exists, not written; use ! to override.", fname);
			else
				msgq(sp, M_ERR,
				    "%s exists, not written.", fname);
			return (1);
		}

		/*
		 * Don't write part of any existing file.  Only test for
		 * the original file, the previous test catches anything
		 * else.
		 */
		if (!LF_ISSET(FS_ALL) && fname == NULL &&
		    !F_ISSET(sp->frp, FR_NAMECHANGED) &&
		    !stat(sp->frp->fname, &sb)) {
			if (LF_ISSET(FS_POSSIBLE))
				msgq(sp, M_ERR,
				    "Use ! to write a partial file.");
			else
				msgq(sp, M_ERR, "Partial file, not written.");
			return (1);
		}
	}

	/*
	 * Figure out if the file already exists -- if it doesn't, we display
	 * the "new file" message.  The stat might not be necessary, but we
	 * just repeat it because it's easier than hacking the previous tests.
	 * The information is only used for the user message and modification
	 * time test, so we can ignore the obvious race condition.
	 *
	 * If the user is overwriting a file other than the original file, and
	 * O_WRITEANY was what got us here (neither force nor append was set),
	 * display the "existing file" messsage.  Since the FR_NAMECHANGED flag
	 * is turned off on a successful write, the latter only appears once.
	 * This is historic practice.
	 *
	 * One final test.  If we're not forcing or appending, and we have a
	 * saved modification time, stop the user if it's been written since
	 * we last edited or wrote it, and make them force it.
	 */
	if (stat(fname == NULL ? sp->frp->fname : fname, &sb))
		msg = ": new file";
	else {
		msg = "";
		if (!LF_ISSET(FS_FORCE | FS_APPEND)) {
			if (sp->frp->mtime && sb.st_mtime > sp->frp->mtime) {
				msgq(sp, M_ERR,
			"%s: file modified more recently than this copy%s.",
				    fname == NULL ? sp->frp->fname : fname,
				    LF_ISSET(FS_POSSIBLE) ?
				    "; use ! to override" : "");
				return (1);
			}
			if ((fname != NULL || F_ISSET(sp->frp, FR_NAMECHANGED)))
				msg = ": existing file";
		}
	}

	/* We no longer care where the name came from. */
	if (fname == NULL)
		fname = sp->frp->fname;

	/* Set flags to either append or truncate. */
	oflags = O_CREAT | O_WRONLY;
	if (LF_ISSET(FS_APPEND))
		oflags |= O_APPEND;
	else
		oflags |= O_TRUNC;

	/* Open the file. */
	if ((fd = open(fname, oflags, DEFFILEMODE)) < 0) {
		msgq(sp, M_SYSERR, fname);
		return (1);
	}

	/*
	 * Once we've decided that we can actually write the file, it
	 * doesn't matter that the file name was changed -- if it was,
	 * we created the file.
	 */
	F_CLR(sp->frp, FR_NAMECHANGED);

	/* Use stdio for buffering. */
	if ((fp = fdopen(fd, "w")) == NULL) {
		(void)close(fd);
		msgq(sp, M_SYSERR, fname);
		return (1);
	}

	/* Build fake addresses, if necessary. */
	if (fm == NULL) {
		from.lno = 1;
		from.cno = 0;
		fm = &from;
		if (file_lline(sp, ep, &to.lno))
			return (1);
		to.cno = 0;
		tm = &to;
	}

	/* Write the file. */
	if (ex_writefp(sp, ep, fname, fp, fm, tm, &nlno, &nch)) {
		if (!LF_ISSET(FS_APPEND))
			msgq(sp, M_ERR, "%s: WARNING: file truncated!", fname);
		return (1);
	}

	msgq(sp, M_INFO, "%s%s: %lu line%s, %lu characters.",
	    fname, msg, nlno, nlno == 1 ? "" : "s", nch);

	/* Save the new last modification time. */
	sp->frp->mtime = stat(fname, &sb) ? 0 : sb.st_mtime;
	
	/* If wrote the entire file, clear the modified bit. */
	if (LF_ISSET(FS_ALL))
		F_CLR(ep, F_MODIFIED);

	return (0);
}
