/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_tag.c,v 5.10 1992/06/07 13:47:38 bostic Exp $ (Berkeley) $Date: 1992/06/07 13:47:38 $";
#endif /* not lint */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
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
	static char *lasttag;
	EXF *ep;
	MARK m, *mp;
	TAG *tag;
	
	DEFMODSYNC;

	switch (cmdp->argc) {
	case 1:
		if ((lasttag = strdup(cmdp->argv[0])) == NULL) {
			msg("Error: %s", strerror(errno));
			return (1);
		}
		break;
	case 0:
		if (lasttag == NULL) {
			msg("No previous tag entered.");
			return (1);
		}
		break;
	}

	if ((tag = tag_push(lasttag)) == NULL)
		return (1);

	/* The tag code can be entered from main, i.e. "vi -t tag". */
	if (curf == NULL) {
		file_ins(file_first(), tag->fname, 0);
		file_start(file_first());
	} else if (strcmp(curf->name, tag->fname)) {
		if (file_stop(curf, 0))
			return (1);
		file_ins(curf, tag->fname, 1);
		file_start(file_next(curf));
	}

	m.lno = 1;
	m.cno = 0;
	if ((mp = f_search(&m, tag->line, NULL, 0)) == NULL) {
		msg("Tag search pattern not found.");
		return (1);
	}
	curf->lno = mp->lno;
	curf->cno = mp->cno;
	return (0);
}
