/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: recover.c,v 8.59 1994/05/19 11:54:47 bostic Exp $ (Berkeley) $Date: 1994/05/19 11:54:47 $";
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

#include <netdb.h>		/* MAXHOSTNAMELEN on some systems. */

#include <bitstring.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
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

/*
 * Recovery code.
 *
 * The basic scheme is there's a btree file, whose name we specify.  The first
 * time a file is modified, and then at RCV_PERIOD intervals after that, the
 * btree file is synced to disk.  Each time a keystroke is requested for a file
 * the terminal routines check to see if the file needs to be synced.  This, of
 * course means that the data structures had better be consistent each time the
 * key routines are called.
 *
 * We don't use timers other than to flag that the file should be synced.  This
 * would require that the SCR and EXF data structures be locked, the dbopen(3)
 * routines lock out the timers for each update, etc.  It's just not worth it.
 * The only way we can lose in the current scheme is if the file is saved, then
 * the user types furiously for RCV_PERIOD - 1 seconds, and types nothing more.
 * Not likely.
 *
 * When a file is first modified, a file which can be handed off to the mailer
 * is created.  The file contains normal headers, with two additions, which
 * occur in THIS order, as the FIRST TWO headers:
 *
 *	Vi-recover-file: file_name
 *	Vi-recover-path: recover_path
 *
 * Since newlines delimit the headers, this means that file names cannot
 * have newlines in them, but that's probably okay.
 *
 * Btree files are named "vi.XXXX" and recovery files are named "recover.XXXX".
 */

#define	VI_FHEADER	"Vi-recover-file: "
#define	VI_PHEADER	"Vi-recover-path: "

static int	rcv_copy __P((SCR *, int, char *));
static int	rcv_mailfile __P((SCR *, EXF *, int, char *));
static int	rcv_mktemp __P((SCR *, char *, char *));

/*
 * rcv_tmp --
 *	Build a file name that will be used as the recovery file.
 */
int
rcv_tmp(sp, ep, name)
	SCR *sp;
	EXF *ep;
	char *name;
{
	struct stat sb;
	int fd;
	char *dp, *p, path[MAXPATHLEN];

	/*
	 * If the recovery directory doesn't exist, try and create it.  As
	 * the recovery files are themselves protected from reading/writing
	 * by other than the owner, the worst that can happen is that a user
	 * would have permission to remove other users recovery files.  If
	 * the sticky bit has the BSD semantics, that too will be impossible.
	 */
	dp = O_STR(sp, O_RECDIR);
	if (stat(dp, &sb)) {
		if (errno != ENOENT || mkdir(dp, 0)) {
			msgq(sp, M_ERR, "Error: %s: %s", dp, strerror(errno));
			goto err;
		}
		(void)chmod(dp, S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX);
	}

	/* Newlines delimit the mail messages. */
	for (p = name; *p; ++p)
		if (*p == '\n') {
			msgq(sp, M_ERR,
		    "Files with newlines in the name are unrecoverable.");
			goto err;
		}

	(void)snprintf(path, sizeof(path), "%s/vi.XXXXXX", dp);
	if ((fd = rcv_mktemp(sp, path, dp)) == -1)
		goto err;
	(void)close(fd);

	if ((ep->rcv_path = strdup(path)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		(void)unlink(path);
err:		msgq(sp, M_ERR,
		    "Modifications not recoverable if the session fails.");
		return (1);
	}

	/* We believe the file is recoverable. */
	F_SET(ep, F_RCV_ON);
	return (0);
}

/*
 * rcv_init --
 *	Force the file to be snapshotted for recovery.
 */
int
rcv_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	recno_t lno;
	int btear;

	/* Only do this once. */
	F_CLR(ep, F_FIRSTMODIFY);

	/* If we already know the file isn't recoverable, we're done. */
	if (!F_ISSET(ep, F_RCV_ON))
		return (0);

	/* Turn off recoverability until we figure out if this will work. */
	F_CLR(ep, F_RCV_ON);

	/* If not recovering a file, build a file to mail to the user. */
	if (ep->rcv_mpath == NULL && rcv_mailfile(sp, ep, 0, NULL))
		goto err;

	/* Force read of entire file. */
	if (file_lline(sp, ep, &lno))
		goto err;

	/* Turn on a busy message, and sync it to backing store. */
	btear = F_ISSET(sp, S_EXSILENT) ? 0 :
	    !busy_on(sp, "Copying file for recovery...");
	if (ep->db->sync(ep->db, R_RECNOSYNC)) {
		msgq(sp, M_ERR, "Preservation failed: %s: %s",
		    ep->rcv_path, strerror(errno));
		if (btear)
			busy_off(sp);
		goto err;
	}
	if (btear)
		busy_off(sp);

	if (!F_ISSET(sp->gp, G_RECOVER_SET) && rcv_on(sp, ep)) {
err:		msgq(sp, M_ERR,
		    "Modifications not recoverable if the session fails.");
		return (1);
	}

	/* We believe the file is recoverable. */
	F_SET(ep, F_RCV_ON);
	return (0);
}

/*
 * rcv_sync --
 *	Sync the file, optionally:
 *		flagging the backup file to be preserved
 *		sending email
 *		ending the file session
 */
int
rcv_sync(sp, ep, flags)
	SCR *sp;
	EXF *ep;
	u_int flags;
{
	int btear, fd, rval;
	char *dp, buf[1024];

	/* Make sure that there's something to recover/sync. */
	if (ep == NULL || !F_ISSET(ep, F_RCV_ON))
		return (0);

	/* Sync the file. */
	if (F_ISSET(ep, F_MODIFIED)) {
		if (ep->db->sync(ep->db, R_RECNOSYNC)) {
			F_CLR(ep, F_RCV_ON | F_RCV_NORM);
			msgq(sp, M_SYSERR,
			    "File backup failed: %s", ep->rcv_path);
			return (1);
		}
		/* Don't remove backing file on exit. */
		if (LF_ISSET(RCV_PRESERVE))
			F_SET(ep, F_RCV_NORM);
	}

	/* Put up a busy message. */
	if (LF_ISSET(RCV_SNAPSHOT | RCV_EMAIL))
		btear = F_ISSET(sp, S_EXSILENT) ? 0 :
		    !busy_on(sp, "Copying file for recovery...");

	/*
	 * !!!
	 * Each time the user exec's :preserve, we have to snapshot all of
	 * the recovery information, i.e. it's like the user re-edited the
	 * file.  We copy the DB(3) backing file, and then create a new mail
	 * recovery file, it's simpler than exiting and reopening all of the
	 * underlying files.
	 */
	rval = 0;
	if (LF_ISSET(RCV_SNAPSHOT)) {
		dp = O_STR(sp, O_RECDIR);
		(void)snprintf(buf, sizeof(buf), "%s/vi.XXXXXX", dp);
		if ((fd = rcv_mktemp(sp, buf, dp)) == -1)
			goto e1;
		if (rcv_copy(sp, fd, ep->rcv_path) || close(fd))
			goto e2;
		if (rcv_mailfile(sp, ep, 1, buf)) {
e2:			(void)unlink(buf);
e1:			if (fd != -1)
				(void)close(fd);
			rval = 1;
		}
	}

	/*
	 * !!!
	 * If you need to port this to a system that doesn't have sendmail,
	 * the -t flag causes sendmail to read the message for the recipients
	 * instead of vi specifying them some other way.
	 */
	if (LF_ISSET(RCV_EMAIL)) {
		(void)snprintf(buf, sizeof(buf),
		    "%s -t < %s", _PATH_SENDMAIL, ep->rcv_mpath);
		(void)system(buf);
	}

	if (btear)
		busy_off(sp);

	if (LF_ISSET(RCV_ENDSESSION) && file_end(sp, ep, 1))
		rval = 1;
	return (rval);
}

/*
 * rcv_mailfile --
 *	Build the file to mail to the user.
 */
static int
rcv_mailfile(sp, ep, iscopy, cp_path)
	SCR *sp;
	EXF *ep;
	int iscopy;
	char *cp_path;
{
	struct passwd *pw;
	uid_t uid;
	FILE *fp;
	time_t now;
	int fd;
	char *dp, *p, *t, host[MAXHOSTNAMELEN], path[MAXPATHLEN];

	if ((pw = getpwuid(uid = getuid())) == NULL) {
		msgq(sp, M_ERR, "Information on user id %u not found.", uid);
		return (1);
	}

	/* Initialize for error. */
	fd = -1;
	fp = NULL;

	dp = O_STR(sp, O_RECDIR);
	(void)snprintf(path, sizeof(path), "%s/recover.XXXXXX", dp);
	if ((fd = rcv_mktemp(sp, path, dp)) == -1)
		return (1);

	/*
	 * We keep an open lock on the file so that the recover option can
	 * distinguish between files that are live and those that need to
	 * be recovered.  There's an obvious window between the mkstemp call
	 * and the lock, but it's pretty small.
	 */
	if (!iscopy) {
		if ((ep->rcv_fd = dup(fd)) == -1)
			goto nolock;
		if (flock(ep->rcv_fd, LOCK_EX | LOCK_NB))
			goto nolock;
	} else {
		if (flock(fd, LOCK_EX | LOCK_NB))
nolock:			msgq(sp, M_SYSERR, "Unable to lock recovery file");
	}

	if (!iscopy) {
		if ((ep->rcv_mpath = strdup(path)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			goto err;
		}
		cp_path = ep->rcv_path;
	}

	/* Use stdio(3). */
	if ((fp = fdopen(fd, "w")) == NULL) {
		msgq(sp, M_SYSERR, "%s", dp);
		goto err;
	}

	t = FILENAME(sp->frp);
	if ((p = strrchr(t, '/')) == NULL)
		p = t;
	else
		++p;
	(void)time(&now);
	(void)gethostname(host, sizeof(host));
	(void)fprintf(fp, "%s%s\n%s%s\n%s\n%s\n%s%s\n%s%s\n%s\n\n",
	    VI_FHEADER, p,			/* Non-standard. */
	    VI_PHEADER, cp_path,		/* Non-standard. */
	    "Reply-To: root",
	    "From: root (Nvi recovery program)",
	    "To: ", pw->pw_name,
	    "Subject: Nvi saved the file ", p,
	    "Precedence: bulk");			/* For vacation(1). */
	(void)fprintf(fp, "%s%.24s%s%s\n%s%s",
	    "On ", ctime(&now),
	    ", the user ", pw->pw_name,
	    "was editing a file named ", p);
	if (p != t)
		(void)fprintf(fp, " (%s)", t);
	(void)fprintf(fp, "\n%s%s%s\n",
	    "on the machine ", host, ", when it was saved for\nrecovery.");
	(void)fprintf(fp, "\n%s\n%s\n\n",
	    "You can recover most, if not all, of the changes",
	    "to this file using the -r option to nex or nvi.");

	if (ferror(fp) || fclose(fp)) {
		msgq(sp, M_SYSERR, NULL);
		goto err;
	}
	return (0);

err:	if (!iscopy && ep->rcv_fd != -1) {
		(void)close(ep->rcv_fd);
		ep->rcv_fd = -1;
	}
	if (fp != NULL)
		(void)fclose(fp);
	else if (fd != -1)
		(void)close(fd);
	return (1);
}

/*
 *	people making love
 *	never exactly the same
 *	just like a snowflake
 *
 * rcv_list --
 *	List the files that can be recovered by this user.
 */
int
rcv_list(sp)
	SCR *sp;
{
	struct dirent *dp;
	struct stat sb;
	DIR *dirp;
	FILE *fp;
	int found;
	char *p, file[1024];

	if (chdir(O_STR(sp, O_RECDIR)) || (dirp = opendir(".")) == NULL) {
		(void)fprintf(stderr,
		    "vi: %s: %s\n", O_STR(sp, O_RECDIR), strerror(errno));
		return (1);
	}

	for (found = 0; (dp = readdir(dirp)) != NULL;) {
		if (strncmp(dp->d_name, "recover.", 8))
			continue;

		/* If it's readable, it's recoverable. */
		if ((fp = fopen(dp->d_name, "r")) == NULL)
			continue;

		/* If it's locked, it's live. */
		if (flock(fileno(fp), LOCK_EX | LOCK_NB)) {
			/* If it's locked, it's live. */
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				(void)fclose(fp);
				continue;
			}
			/*
			 * XXX
			 * Assume that a lock can't be acquired,
			 * but that we should permit recovery anyway.
			 */
		}

		/* Check the header, get the file name. */
		if (fgets(file, sizeof(file), fp) == NULL ||
		    strncmp(file, VI_FHEADER, sizeof(VI_FHEADER) - 1) ||
		    (p = strchr(file, '\n')) == NULL) {
			(void)fprintf(stderr,
			    "vi: %s: malformed recovery file.\n", dp->d_name);
			goto next;
		}
		*p = '\0';

		/* Get the last modification time. */
		if (fstat(fileno(fp), &sb)) {
			(void)fprintf(stderr,
			    "vi: %s: %s\n", dp->d_name, strerror(errno));
			goto next;
		}

		/* Display. */
		(void)printf("%s: %s",
		    file + sizeof(VI_FHEADER) - 1, ctime(&sb.st_mtime));
		found = 1;

next:		(void)fclose(fp);
	}
	if (found == 0)
		(void)printf("vi: no files to recover.\n");
	(void)closedir(dirp);
	return (0);
}

/*
 * rcv_read --
 *	Start a recovered file as the file to edit.
 */
int
rcv_read(sp, frp)
	SCR *sp;
	FREF *frp;
{
	struct dirent *dp;
	struct stat sb;
	DIR *dirp;
	EXF *ep;
	FILE *fp, *sv_fp;
	time_t rec_mtime;
	int found, locked, requested;
	char *name, *p, *t, *recp, *pathp;
	char recpath[MAXPATHLEN], file[MAXPATHLEN], path[MAXPATHLEN];

	if ((dirp = opendir(O_STR(sp, O_RECDIR))) == NULL) {
		msgq(sp, M_ERR,
		    "%s: %s", O_STR(sp, O_RECDIR), strerror(errno));
		return (1);
	}

	name = FILENAME(frp);
	sv_fp = NULL;
	rec_mtime = 0;
	recp = pathp = NULL;
	for (found = requested = 0; (dp = readdir(dirp)) != NULL;) {
		if (strncmp(dp->d_name, "recover.", 8))
			continue;

		/* If it's readable, it's recoverable. */
		(void)snprintf(recpath, sizeof(recpath),
		    "%s/%s", O_STR(sp, O_RECDIR), dp->d_name);
		if ((fp = fopen(recpath, "r")) == NULL)
			continue;

		if (flock(fileno(fp), LOCK_EX | LOCK_NB)) {
			/* If it's locked, it's live. */
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				(void)fclose(fp);
				continue;
			}
			/* Assume that a lock can't be acquired. */
			locked = 0;
		} else
			locked = 1;

		/* Check the headers. */
		if (fgets(file, sizeof(file), fp) == NULL ||
		    strncmp(file, VI_FHEADER, sizeof(VI_FHEADER) - 1) ||
		    (p = strchr(file, '\n')) == NULL ||
		    fgets(path, sizeof(path), fp) == NULL ||
		    strncmp(path, VI_PHEADER, sizeof(VI_PHEADER) - 1) ||
		    (t = strchr(path, '\n')) == NULL) {
			msgq(sp, M_ERR,
			    "%s: malformed recovery file.", recpath);
			goto next;
		}
		++found;
		*t = *p = '\0';

		/* Get the last modification time. */
		if (fstat(fileno(fp), &sb)) {
			msgq(sp, M_ERR,
			    "vi: %s: %s", dp->d_name, strerror(errno));
			goto next;
		}

		/* Check the file name. */
		if (strcmp(file + sizeof(VI_FHEADER) - 1, name))
			goto next;

		++requested;

		/* If we've found more than one, take the most recent. */
		if (recp == NULL || rec_mtime < sb.st_mtime) {
			p = recp;
			t = pathp;
			if ((recp = strdup(recpath)) == NULL) {
				msgq(sp, M_ERR,
				    "vi: Error: %s.\n", strerror(errno));
				recp = p;
				goto next;
			}
			if ((pathp = strdup(path)) == NULL) {
				msgq(sp, M_ERR,
				    "vi: Error: %s.\n", strerror(errno));
				FREE(recp, strlen(recp) + 1);
				recp = p;
				pathp = t;
				goto next;
			}
			if (p != NULL) {
				FREE(p, strlen(p) + 1);
				FREE(t, strlen(t) + 1);
			}
			rec_mtime = sb.st_mtime;
			if (sv_fp != NULL)
				(void)fclose(sv_fp);
			sv_fp = fp;
		} else
next:			(void)fclose(fp);
	}
	(void)closedir(dirp);

	if (recp == NULL) {
		msgq(sp, M_INFO,
		    "No files named %s, readable by you, to recover.", name);
		return (1);
	}
	if (found) {
		if (requested > 1)
			msgq(sp, M_INFO,
		   "There are older versions of this file for you to recover.");
		if (found > requested)
			msgq(sp, M_INFO,
			    "There are other files for you to recover.");
	}

	/* Create the FREF structure, start the btree file. */
	if (file_init(sp, frp, pathp + sizeof(VI_PHEADER) - 1, 0)) {
		FREE(recp, strlen(recp) + 1);
		FREE(pathp, strlen(pathp) + 1);
		(void)fclose(sv_fp);
		return (1);
	}

	/*
	 * We keep an open lock on the file so that the recover option can
	 * distinguish between files that are live and those that need to
	 * be recovered.  The lock is already acquired, so just get a copy.
	 */
	ep = sp->ep;
	if (locked) {
		if ((ep->rcv_fd = dup(fileno(sv_fp))) == -1)
			locked = 0;
	} else
		ep->rcv_fd = -1;
	if (!locked)
		F_SET(frp, FR_UNLOCKED);
	(void)fclose(sv_fp);

	ep->rcv_mpath = recp;

	/* We believe the file is recoverable. */
	F_SET(ep, F_RCV_ON);
	return (0);
}

/*
 * rcv_mktemp --
 *	Paranoid make temporary file routine.
 */
static int
rcv_mktemp(sp, path, dname)
	SCR *sp;
	char *path, *dname;
{
	int fd;

	/*
	 * !!!
	 * We depend on mkstemp(3) setting the permissions correctly.
	 * For GP's, we do it ourselves, to keep the window as small
	 * as possible.
	 */
	if ((fd = mkstemp(path)) == -1)
		msgq(sp, M_SYSERR, "%s", dname);
	else
		(void)chmod(path, S_IRUSR | S_IWUSR);
	return (fd);
}

/*
 * rcv_copy --
 *	Copy a recovery file.
 */
static int
rcv_copy(sp, wfd, fname)
	SCR *sp;
	int wfd;
	char *fname;
{
	int nr, nw, off, rfd;
	char buf[8 * 1024];

	if ((rfd = open(fname, O_RDONLY, 0)) == -1)
		goto err;
	while ((nr = read(rfd, buf, sizeof(buf))) > 0)
		for (off = 0; nr; nr -= nw, off += nw)
			if ((nw = write(wfd, buf + off, nr)) < 0)
				goto err;
	if (nr == 0)
		return (0);

err:	msgq(sp, M_SYSERR, "%s", fname);
	return (1);
}
