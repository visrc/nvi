/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_tag.c,v 5.6 1992/05/01 18:43:34 bostic Exp $ (Berkeley) $Date: 1992/05/01 18:43:34 $";
#endif /* not lint */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "tag.h"
#include "extern.h"

/*
 * ex_tag -- :tag [file]
 *	Display a tag.
 */
int
ex_tag(cmdp)
	EXCMDARG *cmdp;
{
	TAG *tag;
	static char *lasttag;
	
	/*
	 * If the file has been modified, write a warning and fail, unless
	 * overridden by the '!' flag.
	 */
	if (!(cmdp->flags & E_FORCE) && tstflag(file, MODIFIED)) {
		msg("%s has been modified but not written.", origname);
		return (1);
	}

	switch (cmdp->argc) {
	case 1:
		if ((lasttag = strdup(cmdp->argv[0])) == NULL) {
			msg("Error: %s", strerror(errno));
			return (1);
		}
		/* FALLTHROUGH */
	case 0:
		if ((tag = tag_push(lasttag)) == NULL)
			return (1);
		break;
	}

TRACE("tag %s, fname %s, line %s\n", tag->tag, tag->fname, tag->line);

	if (strcmp(origname, tag->fname)) {
		if (!tmpabort(cmdp->flags & E_FORCE)) {
		msg("Use :tag! to abort changes, or :w to save changes");
			return (1);
		}
		tmpstart(tag->fname);
	}

	if ((cursor = f_search(MARK_FIRST, tag->line)) == MARK_UNSET) {
		msg("Tag search pattern not found.");
		cursor = MARK_FIRST;
		return (1);
	}
	return (0);
}
