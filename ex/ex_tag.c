/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_tag.c,v 5.8 1992/05/07 12:47:16 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:47:16 $";
#endif /* not lint */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "exf.h"
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
	MARK m, *mp;
	EXF *ep;
	TAG *tag;
	
	DEFMODSYNC;

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

	if (strcmp(curf->name, tag->fname)) {
		if (!file_stop(curf, 0))
			return (1);
		file_ins(curf, tag->fname);
		file_start(file_next(curf));
	}

	m.lno = 1;
	m.cno = 1;
	if ((mp = f_search(&m, tag->line, NULL, 0)) == NULL) {
		msg("Tag search pattern not found.");
		return (1);
	}
	cursor = *mp;
	return (0);
}
