/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_init.c,v 10.4 1995/06/09 12:51:38 bostic Exp $ (Berkeley) $Date: 1995/06/09 12:51:38 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/stat.h>

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

#include "common.h"
#include "tag.h"
#include "pathnames.h"

enum rc { NOEXIST, NOPERM, RCOK };
static enum rc	exrc_isok __P((SCR *, struct stat *, char *, int, int));

/*
 * ex_screen_copy --
 *	Copy ex screen.
 *
 * PUBLIC: int ex_screen_copy __P((SCR *, SCR *));
 */
int
ex_screen_copy(orig, sp)
	SCR *orig, *sp;
{
	EX_PRIVATE *oexp, *nexp;

	/* Create the private ex structure. */
	CALLOC_RET(orig, nexp, EX_PRIVATE *, 1, sizeof(EX_PRIVATE));
	sp->ex_private = nexp;

	/* Initialize queues. */
	TAILQ_INIT(&nexp->tagq);
	TAILQ_INIT(&nexp->tagfq);
	TAILQ_INIT(&nexp->cdq);
	CIRCLEQ_INIT(&nexp->im_tiq);

	if (orig == NULL) {
	} else {
		oexp = EXP(orig);

		if (oexp->lastbcomm != NULL &&
		    (nexp->lastbcomm = strdup(oexp->lastbcomm)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return(1);
		}

		if (ex_tagcopy(orig, sp))
			return (1);
	}
	return (0);
}

/*
 * ex_screen_end --
 *	End a vi screen.
 *
 * PUBLIC: int ex_screen_end __P((SCR *));
 */
int
ex_screen_end(sp)
	SCR *sp;
{
	EX_PRIVATE *exp;
	int rval;

	if ((exp = EXP(sp)) == NULL)
		return (0);

	rval = 0;
	if (argv_free(sp))
		rval = 1;

	if (exp->ibp != NULL)
		free(exp->ibp);

	if (exp->lastbcomm != NULL)
		free(exp->lastbcomm);

	if (ex_tagfree(sp))
		rval = 1;

	if (ex_cdfree(sp))
		rval = 1;

	text_lfree(&exp->im_tiq);

	/* Free private memory. */
	FREE(exp, sizeof(EX_PRIVATE));
	sp->ex_private = NULL;

	return (rval);
}

/*
 * ex_optchange --
 *	Handle change of options for ex.
 *
 * PUBLIC: int ex_optchange __P((SCR *, int));
 */
int
ex_optchange(sp, opt)
	SCR *sp;
	int opt;
{
	switch (opt) {
	case O_CDPATH:
		return (ex_cdalloc(sp, O_STR(sp, O_CDPATH)));
	case O_TAGS:
		return (ex_tagalloc(sp, O_STR(sp, O_TAGS)));
	}
	return (0);
}


#define	RUN_EXRC(p) {							\
	(void)ex_cfile(sp, p,						\
	    E_BLIGNORE | E_NOAUTO | E_NOPRDEF | E_VLITONLY);		\
	if (F_ISSET(sp, S_EXIT | S_EXIT_FORCE))				\
		return (0);						\
}

#define	RUN_ICMD(sp, s, len, flags) {					\
	(sp)->gp->excmd.cp = s;						\
	(sp)->gp->excmd.clen = len;					\
	F_INIT(&(sp)->gp->excmd, flags);				\
	if (ex_cmd(sp))							\
		return (1);						\
}

/*
 * ex_icmd --
 *	Execute the command-line ex commands.
 *
 * PUBLIC: int ex_icmd __P((SCR *, char *));
 */
int
ex_icmd(sp, s)
	SCR *sp;
	char *s;
{
	RUN_ICMD(sp, s, strlen(s),
	    E_BLIGNORE | E_NOAUTO | E_NOPRDEF | E_VLITONLY);
	return (0);
}

/*
 * ex_exrc --
 *	Read the EXINIT environment variable and the startup exrc files,
 *	and execute their commands.
 *
 * PUBLIC: int ex_exrc __P((SCR *));
 */
int
ex_exrc(sp)
	SCR *sp;
{
	struct stat hsb, lsb;
	char *p, path[MAXPATHLEN];

	/*
	 * Source the system, environment, $HOME and local .exrc values.
	 * Vi historically didn't check $HOME/.exrc if the environment
	 * variable EXINIT was set.  This is all done before the file is
	 * read in, because things in the .exrc information can set, for
	 * example, the recovery directory.
	 *
	 * !!!
	 * While nvi can handle any of the options settings of historic vi,
	 * the converse is not true.  Since users are going to have to have
	 * files and environmental variables that work with both, we use nvi
	 * versions of both the $HOME and local startup files if they exist,
	 * otherwise the historic ones.
	 *
	 * !!!
	 * For a discussion of permissions and when what .exrc files are
	 * read, see the comment above the exrc_isok() function below.
	 *
	 * !!!
	 * If the user started the historic of vi in $HOME, vi read the user's
	 * .exrc file twice, as $HOME/.exrc and as ./.exrc.  We avoid this, as
	 * it's going to make some commands behave oddly, and I can't imagine
	 * anyone depending on it.
	 */
	switch (exrc_isok(sp, &hsb, _PATH_SYSEXRC, 1, 0)) {
	case NOEXIST:
	case NOPERM:
		break;
	case RCOK:
		RUN_EXRC(_PATH_SYSEXRC);
		break;
	}

	if ((p = getenv("NEXINIT")) != NULL || (p = getenv("EXINIT")) != NULL)
		if ((p = strdup(p)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		} else {
			RUN_ICMD(sp, p, strlen(p), E_BLIGNORE |
			    E_NOAUTO | E_NOPRDEF | E_VLITONLY);
			free(p);
			if (F_ISSET(sp, S_EXIT | S_EXIT_FORCE))
				return (0);
		}
	else if ((p = getenv("HOME")) != NULL && *p) {
		(void)snprintf(path, sizeof(path), "%s/%s", p, _PATH_NEXRC);
		switch (exrc_isok(sp, &hsb, path, 0, 1)) {
		case NOEXIST:
			(void)snprintf(path,
			    sizeof(path), "%s/%s", p, _PATH_EXRC);
			if (exrc_isok(sp, &hsb, path, 0, 1) == RCOK)
				RUN_EXRC(path);
			break;
		case NOPERM:
			break;
		case RCOK:
			RUN_EXRC(path);
			break;
		}
	}

	if (O_ISSET(sp, O_EXRC))
		switch (exrc_isok(sp, &lsb, _PATH_NEXRC, 0, 0)) {
		case NOEXIST:
			if (exrc_isok(sp, &lsb, _PATH_EXRC, 0, 0) == RCOK &&
			    (lsb.st_dev != hsb.st_dev ||
			    lsb.st_ino != hsb.st_ino))
				RUN_EXRC(_PATH_EXRC);
			break;
		case NOPERM:
			break;
		case RCOK:
			if (lsb.st_dev != hsb.st_dev ||
			    lsb.st_ino != hsb.st_ino)
				RUN_EXRC(_PATH_NEXRC);
			break;
		}
	return (0);
}

/*
 * ex_cfile --
 *	Execute ex commands from a file.
 *
 * PUBLIC: int ex_cfile __P((SCR *, char *, u_int32_t));
 */
int
ex_cfile(sp, filename, flags)
	SCR *sp;
	char *filename;
	u_int32_t flags;
{
	struct stat sb;
	int fd, len, nf;
	char *bp, *p;

	bp = NULL;
	if ((fd = open(filename, O_RDONLY, 0)) < 0 || fstat(fd, &sb))
		goto err;

	/*
	 * XXX
	 * We'd like to test if the file is too big to malloc.  Since we don't
	 * know what size or type off_t's or size_t's are, what the largest
	 * unsigned integral type is, or what random insanity the local C
	 * compiler will perpetrate, doing the comparison in a portable way
	 * is flatly impossible.  Hope that malloc fails if the file is too
	 * large.
	 */
	MALLOC(sp, bp, char *, (size_t)sb.st_size);
	if (bp == NULL)
		return (1);

	len = read(fd, bp, (int)sb.st_size);
	if (len == -1 || len != sb.st_size) {
		if (len != sb.st_size)
			errno = EIO;

err:		p = msg_print(sp, filename, &nf);
		msgq(sp, M_SYSERR, "%s", p);
		if (nf)
			FREE_SPACE(sp, p, 0);
		if (bp != NULL)
			free(bp);
		(void)close(fd);
		return (1);
	}

	/*
	 * Messages include file/line information, but we don't
	 * care if we can't get space.
	 */
	sp->if_lno = 1;
	sp->if_name = strdup(filename);

	RUN_ICMD(sp, bp, sb.st_size,
	    E_BLIGNORE | E_NOAUTO | E_NOPRDEF | E_VLITONLY);

	if (sp->if_name != NULL) {
		free(sp->if_name);
		sp->if_name = NULL;
	}
	free(bp);
	(void)close(fd);
	return (0);
}

/*
 * exrc_isok --
 *	Check a .exrc file for source-ability.
 *
 * !!!
 * Historically, vi read the $HOME and local .exrc files if they were owned
 * by the user's real ID, or the "sourceany" option was set, regardless of
 * any other considerations.  We no longer support the sourceany option as
 * it's a security problem of mammoth proportions.  We require the system
 * .exrc file to be owned by root, the $HOME .exrc file to be owned by the
 * user's effective ID (or that the user's effective ID be root) and the
 * local .exrc files to be owned by the user's effective ID.  In all cases,
 * the file cannot be writeable by anyone other than its owner.
 *
 * In O'Reilly ("Learning the VI Editor", Fifth Ed., May 1992, page 106),
 * it notes that System V release 3.2 and later has an option "[no]exrc".
 * The behavior is that local .exrc files are read only if the exrc option
 * is set.  The default for the exrc option was off, so, by default, local
 * .exrc files were not read.  The problem this was intended to solve was
 * that System V permitted users to give away files, so there's no possible
 * ownership or writeability test to ensure that the file is safe.
 *
 * POSIX 1003.2-1992 standardized exrc as an option.  It required the exrc
 * option to be off by default, thus local .exrc files are not to be read
 * by default.  The Rationale noted (incorrectly) that this was a change
 * to historic practice, but correctly noted that a default of off improves
 * system security.  POSIX also required that vi check the effective user
 * ID instead of the real user ID, which is why we've switched from historic
 * practice.
 *
 * We initialize the exrc variable to off.  If it's turned on by the system
 * or $HOME .exrc files, and the local .exrc file passes the ownership and
 * writeability tests, then we read it.  This breaks historic 4BSD practice,
 * but it gives us a measure of security on systems where users can give away
 * files.
 */
static enum rc
exrc_isok(sp, sbp, path, rootown, rootid)
	SCR *sp;
	struct stat *sbp;
	char *path;
	int rootown, rootid;
{
	enum { ROOTOWN, OWN, WRITER } etype;
	uid_t euid;
	int nf1, nf2;
	char *a, *b, buf[MAXPATHLEN];

	/* Check for the file's existence. */
	if (stat(path, sbp))
		return (NOEXIST);

	/* Check ownership permissions. */
	euid = geteuid();
	if (!(rootown && sbp->st_uid == 0) &&
	    !(rootid && euid == 0) && sbp->st_uid != euid) {
		etype = rootown ? ROOTOWN : OWN;
		goto denied;
	}

	/* Check writeability. */
	if (sbp->st_mode & (S_IWGRP | S_IWOTH)) {
		etype = WRITER;
		goto denied;
	}
	return (RCOK);

denied:	a = msg_print(sp, path, &nf1);
	if (strchr(path, '/') == NULL && getcwd(buf, sizeof(buf)) != NULL) {
		b = msg_print(sp, buf, &nf2);
		switch (etype) {
		case ROOTOWN:
			msgq(sp, M_ERR,
			    "125|%s/%s: not sourced: not owned by you or root",
			    b, a);
			break;
		case OWN:
			msgq(sp, M_ERR,
			    "126|%s/%s: not sourced: not owned by you", b, a);
			break;
		case WRITER:
			msgq(sp, M_ERR,
    "127|%s/%s: not sourced: writeable by a user other than the owner", b, a);
			break;
		}
		if (nf2)
			FREE_SPACE(sp, b, 0);
	} else
		switch (etype) {
		case ROOTOWN:
			msgq(sp, M_ERR,
			    "128|%s: not sourced: not owned by you or root", a);
			break;
		case OWN:
			msgq(sp, M_ERR,
			    "129|%s: not sourced: not owned by you", a);
			break;
		case WRITER:
			msgq(sp, M_ERR,
	    "130|%s: not sourced: writeable by a user other than the owner", a);
			break;
		}

	if (nf1)
		FREE_SPACE(sp, a, 0);
	return (NOPERM);
}
