/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_tag.c,v 5.23 1993/02/28 14:00:45 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:00:45 $";
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
#include "screen.h"
#include "tag.h"

static int tagchange __P((EXF *, TAG *, int));

/*
 * ex_tagpush -- :tag [file]
 *	Display a tag.
 */
int
ex_tagpush(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	static char *lasttag;
	TAG *tag;
	
	switch (cmdp->argc) {
	case 1:
		if (lasttag)
			free(lasttag);
		if ((lasttag = strdup((char *)cmdp->argv[0])) == NULL) {
			ep->msg(ep, M_ERROR, "Error: %s", strerror(errno));
			return (1);
		}
		break;
	case 0:
		if (lasttag == NULL) {
			ep->msg(ep, M_ERROR, "No previous tag entered.");
			return (1);
		}
		break;
	default:
		abort();
	}

	if ((tag = tag_push(ep, lasttag)) == NULL)
		return (1);
	if (tagchange(ep, tag, cmdp->flags & E_FORCE)) {
		(void)tag_pop(ep);
		return (1);
	}
	return (0);
}

/*
 * ex_tagpop -- :tagp[op][!]
 *	Pop the tag stack.
 */
int
ex_tagpop(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	TAG *tag;
	int force;

	if ((tag = tag_head()) == NULL) {
		ep->msg(ep, M_ERROR, "No tags on the stack.");
		return (1);
	}

	force = cmdp->flags & E_FORCE;
	if (strcmp(ep->name, tag->fname))
		MODIFY_CHECK(ep, force);

	if ((tag = tag_pop(ep)) == NULL)
		return (1);
	return (tagchange(ep, tag, force));
}

/*
 * ex_tagtop -- :tagt[op][!]
 *	Clear the tag stack.
 */	
int
ex_tagtop(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	TAG *tag;

	for (tag = NULL; tag_head() != NULL;)
		tag = tag_pop(ep);
	if (tag == NULL) {
		ep->msg(ep, M_ERROR, "No tags on the stack.");
		return (1);
	}
	return (tagchange(ep, tag, cmdp->flags & E_FORCE));
}

static int
tagchange(ep, tag, force)
	EXF *ep;
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
	} else if (strcmp(ep->name, tag->fname)) {
		MODIFY_CHECK(ep, force);
		if ((tep = file_locate(tag->fname)) == NULL) {
			if (file_ins(ep, tag->fname, 1))
				return (1);
			if ((tep = file_next(ep, 0)) == NULL)
				return (1);
			FF_SET(tep, F_IGNORE);
		}
	} else
		tep = ep;

	m.lno = 1;
	m.cno = 0;
	if ((mp = f_search(tep, &m,
	    (u_char *)tag->line, NULL, SEARCH_PARSE | SEARCH_TERM)) == NULL) {
		ep->msg(ep, M_ERROR,
		    "%s: search pattern not found.", tag->tag);
		return (1);
	}
	SCRLNO(tep) = mp->lno;
	SCRCNO(tep) = mp->cno;

	FF_SET(ep, F_SWITCH);
	ep->eprev = tep;
	return (0);
}
