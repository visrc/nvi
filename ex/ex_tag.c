/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_tag.c,v 5.17 1992/12/05 11:08:57 bostic Exp $ (Berkeley) $Date: 1992/12/05 11:08:57 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "search.h"
#include "tag.h"

static int tagchange __P((TAG *, int));

/*
 * ex_tagpush -- :tag [file]
 *	Display a tag.
 */
int
ex_tagpush(cmdp)
	EXCMDARG *cmdp;
{
	static char *lasttag;
	TAG *tag;
	
	switch (cmdp->argc) {
	case 1:
		if (lasttag)
			free(lasttag);
		if ((lasttag = strdup((char *)cmdp->argv[0])) == NULL) {
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
	if (tagchange(tag, cmdp->flags & E_FORCE)) {
		(void)tag_pop();
		return (1);
	}
	return (0);
}

/*
 * ex_tagpop -- :tagp[op][!]
 *	Pop the tag stack.
 */
int
ex_tagpop(cmdp)
	EXCMDARG *cmdp;
{
	TAG *tag;
	int force;

	if ((tag = tag_head()) == NULL) {
		msg("No tags on the stack.");
		return (1);
	}

	force = cmdp->flags & E_FORCE;
	if (strcmp(curf->name, tag->fname))
		MODIFY_CHECK(curf, force);

	if ((tag = tag_pop()) == NULL)
		return (1);
	return (tagchange(tag, force));
}

/*
 * ex_tagtop -- :tagt[op][!]
 *	Clear the tag stack.
 */	
int
ex_tagtop(cmdp)
	EXCMDARG *cmdp;
{
	TAG *tag;

	for (tag = NULL; tag_head() != NULL;)
		tag = tag_pop();
	if (tag == NULL) {
		msg("No tags on the stack.");
		return (1);
	}
	return (tagchange(tag, cmdp->flags & E_FORCE));
}

static int
tagchange(tag, force)
	TAG *tag;
	int force;
{
	EXF *tep;
	MARK m, *mp;
	char *argv[2];

	/*
	 * The tag code can be entered from main, i.e. "vi -t tag".
	 * If the file list is empty, then the tag file is the file
	 * we're editing, otherwise, it's a side trip.
	 */
	if (file_first(1) == NULL) {
		argv[0] = tag->fname;
		argv[1] = NULL;
		if (file_set(1, argv))
			return (1);
		if ((tep = file_first(0)) == NULL)
			return (1);
		if (file_start(tep))
			return (1);
	} else if (strcmp(curf->name, tag->fname)) {
		MODIFY_CHECK(curf, force);
		if ((tep = file_locate(tag->fname)) == NULL) {
			if (file_ins(curf, tag->fname, 1))
				return (1);
			if ((tep = file_next(curf, 0)) == NULL)
				return (1);
			FF_SET(tep, F_IGNORE);
		}
		if (file_stop(curf, force))
			return (1);
		if (file_start(tep))
			PANIC;
	}

	m.lno = 1;
	m.cno = 0;
	if ((mp = f_search(curf,
	    &m, (u_char *)tag->line, NULL, SEARCH_PARSE)) == NULL) {
		msg("%s: search pattern not found.", tag->tag);
		return (1);
	}
	curf->lno = mp->lno;
	curf->cno = mp->cno;
	return (0);
}
