/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_init.c,v 10.7 1995/11/05 10:29:38 bostic Exp $ (Berkeley) $Date: 1995/11/05 10:29:38 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <bitstring.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "../common/pathnames.h"
#include "tag.h"

enum rc { NOEXIST, NOPERM, RCOK };
static enum rc	exrc_isok __P((SCR *, struct stat *, char *, int, int));

static int ex_run_file __P((SCR *, char *));

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
		if (ex_run_file(sp, _PATH_SYSEXRC))
			return (1);
		if (F_ISSET(sp, S_EXIT | S_EXIT_FORCE))
			return (0);
		break;
	}

	if ((p = getenv("NEXINIT")) != NULL) {
		if (ex_run_str(sp, "NEXINIT", p, strlen(p), 1))
			return (1);
	} else if ((p = getenv("EXINIT")) != NULL) {
		if (ex_run_str(sp, "EXINIT", p, strlen(p), 1))
			return (1);
	} else if ((p = getenv("HOME")) != NULL && *p) {
		(void)snprintf(path, sizeof(path), "%s/%s", p, _PATH_NEXRC);
		switch (exrc_isok(sp, &hsb, path, 0, 1)) {
		case NOEXIST:
			(void)snprintf(path,
			    sizeof(path), "%s/%s", p, _PATH_EXRC);
			if (exrc_isok(sp,
			    &hsb, path, 0, 1) == RCOK && ex_run_file(sp, path))
				return (1);
			break;
		case NOPERM:
			break;
		case RCOK:
			if (ex_run_file(sp, path))
				return (1);
			break;
		}
	}

	if (O_ISSET(sp, O_EXRC))
		switch (exrc_isok(sp, &lsb, _PATH_NEXRC, 0, 0)) {
		case NOEXIST:
			if (exrc_isok(sp, &lsb, _PATH_EXRC, 0, 0) == RCOK &&
			    (lsb.st_dev != hsb.st_dev ||
			    lsb.st_ino != hsb.st_ino) &&
			    ex_run_file(sp, _PATH_EXRC))
				return (1);
			break;
		case NOPERM:
			break;
		case RCOK:
			if ((lsb.st_dev != hsb.st_dev ||
			    lsb.st_ino != hsb.st_ino) &&
			    ex_run_file(sp, _PATH_NEXRC))
				return (1);
			break;
		}
	return (0);
}

/*
 * ex_run_file --
 *	Set up a file of ex commands to run.
 */
static int
ex_run_file(sp, name)
	SCR *sp;
	char *name;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	ex_cbuild(&cmd, C_SOURCE, 0, OOBLNO, OOBLNO, 0, ap, &a, name);
	return (ex_source(sp, &cmd));
}

/*
 * ex_run_str --
 *	Set up a string of ex commands to run.
 *
 * PUBLIC: int ex_run_str __P((SCR *, char *, char *, size_t, int));
 */
int
ex_run_str(sp, name, str, len, nocopy)
	SCR *sp;
	char *name, *str;
	size_t len;
	int nocopy;
{
	EXCMD *ecp;

	/* Build an EXCMD structure and put it on the command queue. */
	CALLOC_RET(sp, ecp, EXCMD *, 1, sizeof(EXCMD));

	if (nocopy)
		ecp->cp = str;
	else if ((ecp->cp = v_strdup(sp, str, len)) == NULL)
		return (1);
	ecp->clen = len;

	if (name == NULL)
		ecp->if_name = NULL;
	else {
		if ((ecp->if_name = v_strdup(sp, name, strlen(name))) == NULL)
			return (1);
		ecp->if_lno = 1;
	}

	F_INIT(ecp, E_BLIGNORE | E_NOAUTO | E_NOPRDEF | E_VLITONLY);

	LIST_INSERT_HEAD(&sp->gp->ecq, ecp, q);
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
