/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: recover.c,v 8.3 1993/07/06 14:26:43 bostic Exp $ (Berkeley) $Date: 1993/07/06 14:26:43 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "pathnames.h"
#include "recover.h"

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
 * would require that the SCR and HDR data structures be locked, the dbopen(3)
 * routines lock out the timers for each update, etc.  It's just not worth it.
 * The only way we can lose in the current scheme is if the file is saved, then
 * the user types furiously for RCV_PERIOD - 1 seconds, and types nothing more.
 * Not likely.
 *
 * When a file is first modified, a file which can be handed off to the mailer
 * is created.  The file contains normal headers, with one addition:
 *
 *	Vi-recover-line: file_name/recover_path
 *
 * Btree files are named "vi.XXXX" and recovery files are named "recover.XXXX".
 */

#define	VIHEADER	"Vi-recover-line: "

static rcv_mailfile __P((SCR *, EXF *));

/*
 * rcv_tmp --
 *	Build a file name that will be used as the recovery file.
 */
int
rcv_tmp(sp, ep)
	SCR *sp;
	EXF *ep;
{
	int fd;
	char path[MAXPATHLEN];

	(void)snprintf(path, sizeof(path), "%s/vi.XXXXXX", _PATH_PRESERVE);
	if ((fd = mkstemp(path)) == -1) {
		msgq(sp, M_ERR,
		    "Error: %s: %s", _PATH_PRESERVE, strerror(errno));
		return (1);
	}
	if ((ep->rcv_path = strdup(path)) == NULL) {
		(void)close(fd);
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		return (1);
	}
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
	struct itimerval value;
	recno_t lno;

	F_CLR(ep, F_FIRSTMODIFY | F_RCV_ON);

	/* Build file to mail to the user. */
	if (rcv_mailfile(sp, ep))
		goto err1;

	/* Force read of entire file. */
	if (file_lline(sp, ep, &lno))
		goto err2;

	(void)sp->s_busy_cursor(sp,
	    lno > 500 ? "Copying file for recovery..." : NULL);

	/* Sync it to backing store. */
	if (ep->db->sync(ep->db, R_RECNOSYNC)) {
		msgq(sp, M_ERR, "Preservation failed: %s: %s",
		    ep->rcv_path, strerror(errno));
		goto err2;
	}

	if (!F_ISSET(sp->gp, G_RECOVER_SET)) {
		/* Start the recovery timer. */
		(void)signal(SIGALRM, SIG_IGN);
		value.it_interval.tv_sec = value.it_interval.tv_usec = 0;
		value.it_value.tv_sec = RCV_PERIOD;
		value.it_value.tv_usec = 0;
		if (setitimer(ITIMER_REAL, &value, NULL)) {
			msgq(sp, M_ERR,
			    "Error: setitimer: %s", strerror(errno));
			goto err2;
		}
		(void)signal(SIGALRM, rcv_alrm);

		/* Initialize the about-to-die handler. */
		(void)signal(SIGHUP, rcv_hup);
	}

	F_SET(ep, F_RCV_ON);
	return (0);

err2:	FREE(ep->rcv_mpath, strlen(ep->rcv_path));
	ep->rcv_mpath = NULL;
err1:	msgq(sp, M_ERR, "Recovery after system crash not possible.");
	return (1);
}

/*
 * rcv_mailfile --
 *	Build the file to mail to the user.
 */
static int
rcv_mailfile(sp, ep)
	SCR *sp;
	EXF *ep;
{
	struct passwd *pw;
	FILE *fp;
	time_t now;
	int fd;
	char *p, host[MAXHOSTNAMELEN], path[MAXPATHLEN];

	if ((pw = getpwuid(getuid())) == NULL) {
		msgq(sp, M_ERR, "Error: getpwuid: %s", strerror(errno));
		return (1);
	}

	(void)snprintf(path, sizeof(path), "%s/recover.XXXXXX", _PATH_PRESERVE);
	if ((fd = mkstemp(path)) == -1 || (fp = fdopen(fd, "w")) == NULL) {
		if (fd != -1)
			(void)close(fd);
		msgq(sp, M_ERR,
		    "Error: %s: %s", _PATH_PRESERVE, strerror(errno));
		return (1);
	}
	if ((ep->rcv_mpath = strdup(path)) == NULL) {
		(void)fclose(fp);
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		return (1);
	}
	
	if ((p = strrchr(ep->name, '/')) == NULL)
		p = ep->name;
	else
		++p;
	(void)time(&now);
	(void)gethostname(host, sizeof(host));
	(void)fprintf(fp, "%s%s/%s\n%s\n%s\n%s%s\n%s%s\n%s\n\n",
	    VIHEADER, p, ep->rcv_path,			/* Non-standard. */
	    "Reply-To: root",
	    "From: root (Nvi recovery program)",
	    "To: ", pw->pw_name,
	    "Subject: Nvi saved the file ", p,
	    "Precedence: bulk");			/* For vacation(1). */
	(void)fprintf(fp, "%s%.24s%s%s\n%s%s%s\n",
	    "On ", ctime(&now),
	    ", the user ", pw->pw_name,
	    "was editing a file named ", ep->name,
	    " on the machine ");
	(void)fprintf(fp, "%s%s\n",
	    host, ", when it was saved for\nrecovery.");
	(void)fprintf(fp, "\n%s\n%s\n%s\n\n",
	    "You can recover most, if not all, of the changes",
	    "to this file using the -l and -r options to nvi(1)",
	    "or nex(1).");

	if (fclose(fp)) {
		(void)unlink(ep->rcv_mpath);
		FREE(ep->rcv_mpath, strlen(ep->rcv_path));
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		return (1);
	}
	return (0);
}

/*
 * rcv_end --
 *	Turn off the timer, handlers.
 */
void
rcv_end()
{
	struct itimerval value;

	(void)signal(SIGALRM, SIG_IGN);
	(void)signal(SIGHUP, SIG_IGN);

	value.it_interval.tv_sec = value.it_interval.tv_usec = 0;
	value.it_value.tv_sec = value.it_value.tv_usec = 0;
	(void)setitimer(ITIMER_REAL, &value, NULL);
}

/*
 * rcv_sync --
 *	Sync the backing file.
 */
int
rcv_sync(sp, ep)
	SCR *sp;
	EXF *ep;
{
	if (ep->db->sync(ep->db, R_RECNOSYNC)) {
		msgq(sp, M_ERR, "Preservation failed: %s: %s",
		    ep->rcv_path, strerror(errno));
		F_CLR(ep, F_RCV_ON);
		return (1);
	}
	return (0);
}

/*
 * rcv_alrm --
 *	Recovery timer interrupt handler.
 *
 *	The only race should be with linking and unlinking the SCR
 *	chain, and using the underlying EXF * from the SCR structure.
 */
void
rcv_alrm(signo)
	int signo;
{
	SCR *sp;

	/* Walk the list of screens, noting that the file should be synced. */
	for (sp = __global_list->scrhdr.next;
	    sp != (SCR *)&__global_list->scrhdr; sp = sp->next)
		if (sp->ep != NULL && F_ISSET(sp->ep, F_RCV_ON))
			F_SET(sp->ep, F_RCV_ALRM);
}

/*
 * rcv_hup --
 *	Recovery death interrupt handler.
 *
 *	The only race should be with linking and unlinking the SCR
 *	chain, and using the underlying EXF * from the SCR structure.
 *
 *	DON'T USE MSG ROUTINES, THEY'RE NOT PROTECTED AGAINST US!
 */
void
rcv_hup(signo)
	int signo;
{
	SCR *sp;

	/* Walk the list of screens, sync'ing the files. */
	for (sp = __global_list->scrhdr.next;
	    sp != (SCR *)&__global_list->scrhdr; sp = sp->next)
		if (sp->ep != NULL)
			if (F_ISSET(sp->ep, F_RCV_ON))
				(void)sp->ep->db->sync(sp->ep->db, R_RECNOSYNC);
			else
				(void)unlink(sp->ep->rcv_path);

	/* Die with the proper exit status. */
	(void)signal(SIGHUP, SIG_DFL);
	(void)kill(0, SIGHUP);

	/* NOTREACHED */
	exit (1);
}

/*
 * rcv_list --
 *	List the files that can be recovered by this user.
 */
int
rcv_list()
{
	struct dirent *dp;
	struct stat sb;
	DIR *dirp;
	FILE *fp;
	int found;
	char *p, buf[1024];

	if (chdir(_PATH_PRESERVE) || (dirp = opendir(".")) == NULL) {
		(void)fprintf(stderr,
		    "vi: %s: %s\n", _PATH_PRESERVE, strerror(errno));
		return (1);
	}

	for (found = 0; (dp = readdir(dirp)) != NULL;) {
		if (strncmp(dp->d_name, "recover.", 8))
			continue;

		/* If it's readable, it's recoverable. */
		if ((fp = fopen(dp->d_name, "r")) == NULL)
			continue;

		/* Check the header. */
		if (fgets(buf, sizeof(buf), fp) == NULL ||
		    strncmp(buf, VIHEADER, sizeof(VIHEADER) - 1)) {
			(void)fprintf(stderr,
			    "vi: %s: malformed recovery file.", dp->d_name);
			goto next;
		}

		/* Get the last modification time. */
		if (fstat(fileno(fp), &sb)) {
			(void)fprintf(stderr,
			    "vi: %s: %s", dp->d_name, strerror(errno));
			goto next;
		}

		/* Get the file name. */
		if ((p = strchr(buf, '/')) == NULL)
			goto next;
		*p = '\0';

		/* Display. */
		(void)printf("%s: %s",
		    buf + sizeof(VIHEADER) - 1, ctime(&sb.st_mtime));
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
EXF *
rcv_read(sp, name)
	SCR *sp;
	char *name;
{
	struct dirent *dp;
	DIR *dirp;
	EXF *ep;
	FILE *fp;
	int found;
	char *p, *t, buf[1024], path[MAXPATHLEN];
		
	if ((dirp = opendir(_PATH_PRESERVE)) == NULL) {
		msgq(sp, M_ERR, "%s: %s", _PATH_PRESERVE, strerror(errno));
		return (NULL);
	}

	for (found = 0; (dp = readdir(dirp)) != NULL;) {
		if (strncmp(dp->d_name, "recover.", 8))
			continue;

		/* If it's readable, it's recoverable. */
		(void)snprintf(path, sizeof(path),
		    "%s/%s", _PATH_PRESERVE, dp->d_name);
		if ((fp = fopen(path, "r")) == NULL)
			continue;

		/* Check the header. */
		if (fgets(buf, sizeof(buf), fp) == NULL ||
		    strncmp(buf, VIHEADER, sizeof(VIHEADER) - 1) ||
		    (p = strchr(buf, '/')) == NULL) {
			msgq(sp, M_ERR, "%s: malformed recovery file.", path);
			(void)fclose(fp);
			continue;
		}
		*p = '\0';
		(void)fclose(fp);

		/* Check the file name. */
		if (!strcmp(buf + sizeof(VIHEADER) - 1, name)) {
			found = 1;
			break;
		}
	}
	(void)closedir(dirp);

	if (!found) {
		msgq(sp, M_INFO,
		    "No files named %s, owned by you, to edit.", name);
		return (NULL);
	}

	/*
	 * Get the btree file name.
	 *
	 * 'p' was left pointing to the '/' slot.
	 */
	t = p + 1;
	if ((p = strchr(t, '\n')) == NULL) {
		msgq(sp, M_ERR, "%s: malformed recovery file.", path);
		return (NULL);
	}
	*p = '\0';
	if ((p = strdup(path)) == NULL) {
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		return (NULL);
	}
	if ((ep = file_start(sp, NULL, t)) == NULL)
		return (NULL);
	ep->rcv_mpath = p;
	return (ep);
}
