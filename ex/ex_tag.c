/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_tag.c,v 5.27 1993/04/05 07:11:51 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:11:51 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "tag.h"
#include "excmd.h"

static int tagchange __P((SCR *, EXF *, TAG *, int));

/*
 * ex_tagpush -- :tag [file]
 *	Display a tag.
 */
int
ex_tagpush(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	TAG *tag;
	
	switch (cmdp->argc) {
	case 1:
		if (sp->tlast)
			free(sp->tlast);
		if ((sp->tlast = strdup((char *)cmdp->argv[0])) == NULL) {
			msgq(sp, M_ERROR, "Error: %s", strerror(errno));
			return (1);
		}
		break;
	case 0:
		if (sp->tlast == NULL) {
			msgq(sp, M_ERROR, "No previous tag entered.");
			return (1);
		}
		break;
	default:
		abort();
	}

	if ((tag = tag_push(sp, sp->tlast)) == NULL)
		return (1);
	if (tagchange(sp, ep, tag, cmdp->flags & E_FORCE)) {
		(void)tag_pop(sp);
		return (1);
	}
	return (0);
}

/*
 * ex_tagpop -- :tagp[op][!]
 *	Pop the tag stack.
 */
int
ex_tagpop(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	TAG *tag;
	int force;

	if ((tag = tag_head(sp)) == NULL) {
		msgq(sp, M_ERROR, "No tags on the stack.");
		return (1);
	}

	force = cmdp->flags & E_FORCE;
	if (strcmp(ep->name, tag->fname))
		MODIFY_CHECK(sp, ep, force);

	if ((tag = tag_pop(sp)) == NULL)
		return (1);
	return (tagchange(sp, ep, tag, force));
}

/*
 * ex_tagtop -- :tagt[op][!]
 *	Clear the tag stack.
 */	
int
ex_tagtop(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	TAG *tag;

	for (tag = NULL; tag_head(sp) != NULL;)
		tag = tag_pop(sp);
	if (tag == NULL) {
		msgq(sp, M_ERROR, "No tags on the stack.");
		return (1);
	}
	return (tagchange(sp, ep, tag, cmdp->flags & E_FORCE));
}

static int
tagchange(sp, ep, tag, force)
	SCR *sp;
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
	if (file_first(sp, 1) == NULL) {
		argv[0] = tag->fname;
		argv[1] = NULL;
		if (file_set(sp, 1, argv))
			return (1);
		if ((tep = file_first(sp, 0)) == NULL)
			return (1);
	} else if (strcmp(ep->name, tag->fname)) {
		MODIFY_CHECK(sp, ep, force);
		if ((tep = file_get(sp, ep, tag->fname, 1)) == NULL)
			return (1);
	} else
		tep = ep;

	m.lno = 1;
	m.cno = 0;
	if ((mp = f_search(sp, tep, &m,
	    tag->line, NULL, SEARCH_PARSE | SEARCH_TERM)) == NULL) {
		msgq(sp, M_ERROR, "%s: search pattern not found.", tag->tag);
		return (1);
	}
	tep->start_lno = mp->lno;
	tep->start_cno = mp->cno;

	sp->enext = tep;
	F_SET(sp, S_FSWITCH);
	return (0);
}
