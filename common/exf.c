/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: exf.c,v 8.82 1994/06/28 12:07:50 bostic Exp $ (Berkeley) $Date: 1994/06/28 12:07:50 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

/*
 * We include <sys/file.h>, because the flock(2) #defines were
 * found there on historical systems.  We also include <fcntl.h>
 * because the open(2) #defines are found there on newer systems.
 */
#include <sys/file.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>
#include <pathnames.h>

#include "vi.h"
#include "excmd.h"

/*
 * file_add --
 *	Insert a file name into the FREF list, if it doesn't already
 *	appear in it.
 *
 * !!!
 * The "if it doesn't already appear" changes vi's semantics slightly.  If
 * you do a "vi foo bar", and then execute "next bar baz", the edit of bar
 * will reflect the line/column of the previous edit session.  Historic nvi
 * did not do this.  The change is a logical extension of the change where
 * vi now remembers the last location in any file that it has ever edited,
 * not just the previously edited file.
 */
FREF *
file_add(sp, name)
	SCR *sp;
	CHAR_T *name;
{
	FREF *frp;

	/*
	 * Return it if it already exists.  Note that we test against the
	 * user's name, whatever that happens to be, including if it's a
	 * temporary file.
	 */
	if (name != NULL)
		for (frp = sp->frefq.cqh_first;
		    frp != (FREF *)&sp->frefq; frp = frp->q.cqe_next)
			if (!strcmp(frp->name, name))
				return (frp);

	/* Allocate and initialize the FREF structure. */
	CALLOC(sp, frp, FREF *, 1, sizeof(FREF));
	if (frp == NULL)
		return (NULL);

	/*
	 * If no file name specified, or if the file name is a request
	 * for something temporary, file_init() will allocate the file
	 * name.  Temporary files are always ignored.
	 */
	if (name != NULL && strcmp(name, TEMPORARY_FILE_STRING) &&
	    (frp->name = strdup(name)) == NULL) {
		FREE(frp, sizeof(FREF));
		msgq(sp, M_SYSERR, NULL);
		return (NULL);
	}

	/* Append into the chain of file names. */
	CIRCLEQ_INSERT_TAIL(&sp->frefq, frp, q);

	return (frp);
}

/*
 * file_init --
 *	Start editing a file, based on the FREF structure.  If successsful,
 *	let go of any previous file.  Don't release the previous file until
 *	absolutely sure we have the new one.
 */
int
file_init(sp, frp, rcv_name, force)
	SCR *sp;
	FREF *frp;
	char *rcv_name;
	int force;
{
	EXF *ep;
	RECNOINFO oinfo;
	struct stat sb;
	size_t psize;
	int fd;
	char *oname, tname[MAXPATHLEN];

	/*
	 * If the file is a recovery file, let the recovery code handle it.
	 * Clear the FR_RECOVER flag first -- the recovery code does set up,
	 * and then calls us!  If the recovery call fails, it's probably
	 * because the named file doesn't exist.  So, move boldly forward,
	 * presuming that there's an error message the user will get to see.
	 */
	if (F_ISSET(frp, FR_RECOVER)) {
		F_CLR(frp, FR_RECOVER);
		return (rcv_read(sp, frp));
	}

	/*
	 * Required EXF initialization:
	 *	Flush the line caches.
	 *	Default recover mail file fd to -1.
	 *	Set initial EXF flag bits.
	 */
	CALLOC_RET(sp, ep, EXF *, 1, sizeof(EXF));
	ep->c_lno = ep->c_nlines = OOBLNO;
	ep->rcv_fd = -1;
	LIST_INIT(&ep->marks);
	F_SET(ep, F_FIRSTMODIFY);

	/*
	 * If no name or backing file, create a backing temporary file, saving
	 * the temp file name so we can later unlink it.  If the user never
	 * named this file, copy the temporary file name to the real name (we
	 * display that until the user renames it).
	 */
	if ((oname = frp->name) == NULL || stat(oname, &sb)) {
		(void)snprintf(tname,
		    sizeof(tname), "%s/vi.XXXXXX", O_STR(sp, O_DIRECTORY));
		if ((fd = mkstemp(tname)) == -1) {
			msgq(sp, M_SYSERR, "Temporary file");
			goto err;
		}
		(void)close(fd);

		if (frp->name == NULL)
			F_SET(frp, FR_TMPFILE);
		if ((frp->tname = strdup(tname)) == NULL ||
		    frp->name == NULL && (frp->name = strdup(tname)) == NULL) {
			if (frp->tname != NULL)
				free(frp->tname);
			msgq(sp, M_SYSERR, NULL);
			(void)unlink(tname);
			goto err;
		}
		oname = frp->tname;
		psize = 4 * 1024;
		F_SET(frp, FR_NEWFILE);
	} else {
		/*
		 * Try to keep it at 10 pages or less per file.  This
		 * isn't friendly on a loaded machine, btw.
		 */
		if (sb.st_size < 40 * 1024)
			psize = 4 * 1024;
		else if (sb.st_size < 320 * 1024)
			psize = 32 * 1024;
		else
			psize = 64 * 1024;

		frp->mtime = sb.st_mtime;

		if (!S_ISREG(sb.st_mode))
			msgq(sp, M_ERR,
			    "Warning: %s is not a regular file", oname);
	}

	/* Set up recovery. */
	memset(&oinfo, 0, sizeof(RECNOINFO));
	oinfo.bval = '\n';			/* Always set. */
	oinfo.psize = psize;
	oinfo.flags = F_ISSET(sp->gp, G_SNAPSHOT) ? R_SNAPSHOT : 0;
	if (rcv_name == NULL) {
		if (!rcv_tmp(sp, ep, frp->name))
			oinfo.bfname = ep->rcv_path;
	} else {
		if ((ep->rcv_path = strdup(rcv_name)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			goto err;
		}
		oinfo.bfname = ep->rcv_path;
		F_SET(ep, F_MODIFIED);
	}

	/* Open a db structure. */
	if ((ep->db = dbopen(rcv_name == NULL ? oname : NULL,
	    O_NONBLOCK | O_RDONLY, DEFFILEMODE, DB_RECNO, &oinfo)) == NULL) {
		msgq(sp, M_SYSERR, rcv_name == NULL ? oname : rcv_name);
		goto err;
	}

	/* Init file marks. */
	if (mark_init(sp, ep))
		goto err;

	/* Start logging. */
	if (log_init(sp, ep))
		goto err;

	/*
	 * The -R flag, or doing a "set readonly" during a session causes
	 * all files edited during the session (using an edit command, or
	 * even using tags) to be marked read-only.  Changing the file name
	 * (see ex/ex_file.c), clears this flag.
	 *
	 * Otherwise, try and figure out if a file is readonly.  This is a
	 * dangerous thing to do.  The kernel is the only arbiter of whether
	 * or not a file is writeable, and the best that a user program can
	 * do is guess.  Obvious loopholes are files that are on a file system
	 * mounted readonly (access catches this one on a few systems), or
	 * alternate protection mechanisms, ACL's for example, that we can't
	 * portably check.  Lots of fun, and only here because users whined.
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
	if (O_ISSET(sp, O_READONLY) || !F_ISSET(frp, FR_NEWFILE) &&
	    (!(sb.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) ||
	    access(frp->name, W_OK)))
		F_SET(frp, FR_RDONLY);
	else
		F_CLR(frp, FR_RDONLY);

	/*
	 * Close the previous file; if that fails, close the new one and
	 * run for the border.
	 *
	 * !!!
	 * There's a nasty special case.  If the user edits a temporary file,
	 * and then does an ":e! %", we need to re-initialize the backing
	 * file, but we can't change the name.  (It's worse -- we're dealing
	 * with *names* here, we can't even detect that it happened.)  Set a
	 * flag so that the file_end routine ignores the backing information
	 * of the old file if it happens to be the same as the new one.
	 *
	 * !!!
	 * Side-effect: after the call to file_end(), sp->frp may be NULL.
	 */
	F_SET(frp, FR_DONTDELETE);
	if (sp->ep != NULL && file_end(sp, sp->ep, force)) {
		(void)file_end(sp, ep, 1);
		goto err;
	}
	F_CLR(frp, FR_DONTDELETE);

	/*
	 * 4.4BSD supports locking in the open call, other systems don't.
	 * Since the user can't interrupt us between the open and here,
	 * we Don't Care.
	 *
	 * !!!
	 * We need to distinguish a lock not being available for the file
	 * from the file system not supporting locking.  Assume that EAGAIN
	 * or EWOULDBLOCK is the former.  There isn't a portable way to do
	 * this.
	 *
	 * If it's a recovery file, it should already be locked.
	 *
	 * XXX
	 * The locking is flock(2) style, not fcntl(2).  The latter is known
	 * to fail badly on some systems, and its only advantage is that it
	 * occasionally works over NFS.
	 *
	 * XXX
	 * We don't make a big deal of not being able to lock the file.  As
	 * locking rarely works over NFS, it's far too common to do anything
	 * like print an error message, let alone make the file readonly.  At
	 * some future time, this should be changed to be an error.
	 */
	if (rcv_name == NULL && flock(ep->db->fd(ep->db), LOCK_EX | LOCK_NB))
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			msgq(sp, M_INFO,
			    "%s already locked, session is read-only", oname);
			F_SET(frp, FR_RDONLY);
		} else
			F_SET(frp, FR_UNLOCKED);

	/*
	 * Set the alternate file name to be the file we've just discarded.
	 *
	 * !!!
	 * If the current file was a temporary file, the call to file_end()
	 * unlinked it and free'd the name.  So, there is no previous file,
	 * and there is no alternate file name.  This matches historical
	 * practice, although in historical vi it could only happen as the
	 * result of the initial command, i.e. if vi was executed without a
	 * file name.
	 */
	set_alt_name(sp, sp->frp == NULL ? NULL : sp->frp->name);

	/* Switch... */
	++ep->refcnt;
	sp->ep = ep;
	sp->frp = frp;
	return (0);

err:	if (frp->name != NULL) {
		free(frp->name);
		frp->name = NULL;
	}
	if (frp->tname != NULL) {
		(void)unlink(frp->tname);
		free(frp->tname);
		frp->tname = NULL;
	}
	if (ep->rcv_path != NULL) {
		free(ep->rcv_path);
		ep->rcv_path = NULL;
	}
	if (ep->db != NULL)
		(void)ep->db->close(ep->db);
	FREE(ep, sizeof(EXF));
	return (1);
}

/*
 * file_end --
 *	Stop editing a file.
 */
int
file_end(sp, ep, force)
	SCR *sp;
	EXF *ep;
	int force;
{
	FREF *frp;

	/*
	 * Clean up the FREF structure.
	 *
	 * Save the cursor location.
	 *
	 * XXX
	 * It would be cleaner to do this somewhere else, but by the time
	 * ex or vi knows that we're changing files it's already happened.
	 */
	frp = sp->frp;
	frp->lno = sp->lno;
	frp->cno = sp->cno;
	F_SET(frp, FR_CURSORSET);

	/*
	 * We may no longer need the temporary backing file, so clean it
	 * up.  We don't need the FREF structure either, if the file was
	 * never named, so lose it.
	 *
	 * !!!
	 * Re: FR_DONTDELETE, see the comment above in file_init().
	 */
	if (!F_ISSET(frp, FR_DONTDELETE) && frp->tname != NULL) {
		if (unlink(frp->tname))
			msgq(sp, M_SYSERR, "%s: remove", frp->tname);
		free(frp->tname);
		if (F_ISSET(frp, FR_TMPFILE)) {
			CIRCLEQ_REMOVE(&sp->frefq, frp, q);
			free(frp->name);
			free(frp);
		}
		sp->frp = NULL;
	}

	/*
	 * Clean up the EXF structure.
	 *
	 * sp->ep MAY NOT BE THE SAME AS THE ARGUMENT ep, SO DON'T USE IT!
	 *
	 * If multiply referenced, just decrement the count and return.
	 */
	if (--ep->refcnt != 0)
		return (0);

	/* Close the db structure. */
	if (ep->db->close != NULL && ep->db->close(ep->db) && !force) {
		msgq(sp, M_ERR, "%s: close: %s", frp->name, strerror(errno));
		++ep->refcnt;
		return (1);
	}

	/* COMMITTED TO THE CLOSE.  THERE'S NO GOING BACK... */

	/* Stop logging. */
	(void)log_end(sp, ep);

	/* Free up any marks. */
	(void)mark_end(sp, ep);

	/*
	 * Delete recovery files, close the open descriptor, free recovery
	 * memory.  See recover.c for a description of the protocol.
	 *
	 * XXX
	 * Unlink backup file first, we can detect that the recovery file
	 * doesn't reference anything when the user tries to recover it.
	 * There's a race, here, obviously, but it's fairly small.
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
		free(ep->rcv_path);
	if (ep->rcv_mpath != NULL)
		free(ep->rcv_mpath);

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
file_write(sp, ep, fm, tm, name, flags)
	SCR *sp;
	EXF *ep;
	MARK *fm, *tm;
	char *name;
	int flags;
{
	struct stat sb;
	FILE *fp;
	FREF *frp;
	MARK from, to;
	u_long nlno, nch;
	int btear, fd, oflags, rval;
	char *msg;

	/*
	 * Don't permit writing to temporary files if the command is going to
	 * exit.  The problem is that if it's a temp file, and the user does
	 * ":wq", we write and quit, unlinking the temporary file.  Not what
	 * the user had in mind at all.  This test cannot be forced.
	 */
	frp = sp->frp;
	if (name == NULL &&
	    F_ISSET(frp, FR_TMPFILE) && LF_ISSET(FS_WILLEXIT)) {
		msgq(sp, M_ERR,
		    "File is a temporary; write/exit not permitted");
		return (1);
	}

	/* Can't write files marked read-only, unless forced. */
	if (!LF_ISSET(FS_FORCE) &&
	    name == NULL && F_ISSET(frp, FR_RDONLY)) {
		if (LF_ISSET(FS_POSSIBLE))
			msgq(sp, M_ERR,
			    "Read-only file, not written; use ! to override");
		else
			msgq(sp, M_ERR,
			    "Read-only file, not written");
		return (1);
	}

	/* If not forced, not appending, and "writeany" not set ... */
	if (!LF_ISSET(FS_FORCE | FS_APPEND) && !O_ISSET(sp, O_WRITEANY)) {
		/* Don't overwrite anything but the original file. */
		if (name != NULL) {
			if (!stat(name, &sb))
				goto exists;
		} else if (F_ISSET(frp,
		    FR_NAMECHANGE) && !stat(frp->name, &sb)) {
			name = frp->name;
exists:			if (LF_ISSET(FS_POSSIBLE))
				msgq(sp, M_ERR,
		"%s exists, not written; use ! to override", name);
			else
				msgq(sp, M_ERR,
				    "%s exists, not written", name);
			return (1);
		}

		/*
		 * Don't write part of any existing file.  Only test for the
		 * original file, the previous test catches anything else.
		 */
		if (!LF_ISSET(FS_ALL) &&
		    name == NULL && !stat(frp->name, &sb)) {
			if (LF_ISSET(FS_POSSIBLE))
				msgq(sp, M_ERR,
				    "Use ! to write a partial file");
			else
				msgq(sp, M_ERR, "Partial file, not written");
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
	 * display the "existing file" messsage.  Since the FR_NAMECHANGE flag
	 * is cleared on a successful write, the message only appears once when
	 * the user changes a file name.  This is historic practice.
	 *
	 * One final test.  If we're not forcing or appending, and we have a
	 * saved modification time, stop the user if it's been written since
	 * we last edited or wrote it, and make them force it.
	 */
	if (stat(name == NULL ? frp->name : name, &sb))
		msg = ": new file";
	else {
		msg = "";
		if (!LF_ISSET(FS_FORCE | FS_APPEND)) {
			if (frp->mtime && sb.st_mtime > frp->mtime) {
				msgq(sp, M_ERR,
			"%s: file modified more recently than this copy%s",
				    name == NULL ? frp->name : name,
				    LF_ISSET(FS_POSSIBLE) ?
				    "; use ! to override" : "");
				return (1);
			}
			if (name != NULL || F_ISSET(frp, FR_NAMECHANGE))
				msg = ": existing file";
		}
	}

	/* We no longer care where the name came from. */
	if (name == NULL)
		name = frp->name;

	/* Set flags to either append or truncate. */
	oflags = O_CREAT | O_WRONLY;
	if (LF_ISSET(FS_APPEND))
		oflags |= O_APPEND;
	else
		oflags |= O_TRUNC;

	/* Open the file. */
	if ((fd = open(name, oflags, DEFFILEMODE)) < 0) {
		msgq(sp, M_SYSERR, name);
		return (1);
	}

	/* Use stdio for buffering. */
	if ((fp = fdopen(fd, "w")) == NULL) {
		(void)close(fd);
		msgq(sp, M_SYSERR, name);
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

	/* Turn on the busy message. */
	btear = F_ISSET(sp, S_EXSILENT) ? 0 : !busy_on(sp, "Writing...");
	rval = ex_writefp(sp, ep, name, fp, fm, tm, &nlno, &nch);
	if (btear)
		busy_off(sp);

	/*
	 * Save the new last modification time -- even if the write fails
	 * we re-init the time.  That way the user can clean up the disk
	 * and rewrite without having to force it.
	 */
	frp->mtime = stat(name, &sb) ? 0 : sb.st_mtime;

	/* If the write failed, complain loudly. */
	if (rval) {
		if (!LF_ISSET(FS_APPEND))
			msgq(sp, M_ERR, "%s: WARNING: file truncated!", name);
		return (1);
	}

	/*
	 * Once we've actually written the file, it doesn't matter that the
	 * file name was changed -- if it was, we've already whacked it.
	 */
	F_CLR(frp, FR_NAMECHANGE);

	/* If wrote the entire file, clear the modified bit. */
	if (LF_ISSET(FS_ALL))
		F_CLR(ep, F_MODIFIED);

	msgq(sp, M_INFO, "%s%s%s: %lu line%s, %lu characters",
	    INTERRUPTED(sp) ? "Interrupted write: " : "",
	    name, msg, nlno, nlno == 1 ? "" : "s", nch);

	return (0);
}
