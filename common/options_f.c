/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: options_f.c,v 5.1 1993/03/25 14:16:03 bostic Exp $ (Berkeley) $Date: 1993/03/25 14:16:03 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "vi.h"
#include "options.h"
#include "screen.h"

int
f_columns(ep, valp, eqp)
	EXF *ep; 
	void *valp;
	char *eqp;
{
	u_long val;
	char buf[25];

	val = *(u_long *)valp;

	if (val < MINIMUM_SCREEN_COLS) {
		ep->msg(ep, M_ERROR, "Screen columns too small, less than %d.",
		    MINIMUM_SCREEN_COLS);
		return (1);
	}
	if (val < LVAL(O_SHIFTWIDTH)) {
		ep->msg(ep, M_ERROR,
		    "Screen columns too small, less than shiftwidth.");
		return (1);
	}
	if (val < LVAL(O_SIDESCROLL)) {
		ep->msg(ep, M_ERROR,
		    "Screen columns too small, less than sidescroll.");
		return (1);
	}
	if (val < LVAL(O_TABSTOP)) {
		ep->msg(ep, M_ERROR,
		    "Screen columns too small, less than tabstop.");
		return (1);
	}
	if (val < LVAL(O_WRAPMARGIN)) {
		ep->msg(ep, M_ERROR,
		    "Screen columns too small, less than wrapmargin.");
		return (1);
	}
	if (val < O_NUMBER_LENGTH) {
		ep->msg(ep, M_ERROR,
		    "Screen columns too small, less than number option.");
		return (1);
	}
	(void)snprintf(buf, sizeof(buf), "COLUMNS=%lu", val);
	(void)putenv(buf);

	/* Set resize bit. */
	SF_SET(ep, S_RESIZE);
	return (0);
}

int
f_flash(ep, valp, eqp)
	EXF *ep;
	void *valp;
	char *eqp;
{
	size_t len;
	char *s, b1[1024], b2[1024];
	
	if ((s = getenv("TERM")) == NULL) {
		ep->msg(ep, M_ERROR,
		    "No \"TERM\" value set in the environment.");
		return (1);
	}

	/* Get the termcap information. */
	if (tgetent(b1, s) != 1) {
		ep->msg(ep, M_ERROR, "No termcap entry for %s.", s);
		return (1);
	}

	/*
	 * Get the visual bell string.  If one doesn't exist, then
	 * set O_ERRORBELLS.
	 */
	if (tgetstr("vb", &s) == NULL) {
		SET(O_ERRORBELLS);
		UNSET(O_FLASH);
		return (1);
	}

	len = s - b2;
	if ((VB = malloc(len)) == NULL) {
		ep->msg(ep, M_ERROR, "Error: %s", strerror(errno));
		return (1);
	}

	memmove(s, b2, len);

	if (VB != NULL)
		free(VB);
	VB = s;

	return (0);
}

int
f_mesg(ep, valp, eqp)
	EXF *ep;
	void *valp;
	char *eqp;
{
	extern mode_t __orig_mode;		/* GLOBAL */
	extern int __set_orig_mode;		/* GLOBAL */
	struct stat sb;
	char *tty;

	if ((tty = ttyname(STDERR_FILENO)) == NULL) {
		ep->msg(ep, M_ERROR, "ttyname: %s", strerror(errno));
		return (1);
	}
	if (stat(tty, &sb) < 0) {
		ep->msg(ep, M_ERROR, "%s: %s", strerror(errno));
		return (1);
	}

	__set_orig_mode = 1;
	__orig_mode = sb.st_mode;

	if (ISSET(O_MESG)) {
		if (chmod(tty, sb.st_mode | S_IWGRP) < 0) {
			ep->msg(ep, M_ERROR, "%s: %s", strerror(errno));
			return (1);
		}
	} else
		if (chmod(tty, sb.st_mode & ~S_IWGRP) < 0) {
			ep->msg(ep, M_ERROR, "%s: %s", strerror(errno));
			return (1);
		}
	return (0);
}

int
f_keytime(ep, valp, eqp)
	EXF *ep; 
	void *valp;
	char *eqp;
{
	u_long val;

	val = *(u_long *)valp;

#define	MAXKEYTIME	20
	if (val > MAXKEYTIME) {
		ep->msg(ep, M_ERROR,
		    "Keytime too large, more than %d.", MAXKEYTIME);
		return (1);
	}
	return (0);
}

int
f_leftright(ep, valp, eqp)
	EXF *ep;
	void *valp;
	char *eqp;
{
	return (scr_end(ep) || scr_begin(ep));
}

int
f_lines(ep, valp, eqp)
	EXF *ep; 
	void *valp;
	char *eqp;
{
	u_long val;
	char buf[25];

	val = *(u_long *)valp;

	if (val < MINIMUM_SCREEN_ROWS) {
		ep->msg(ep, M_ERROR, "Screen lines too small, less than %d.",
		    MINIMUM_SCREEN_ROWS);
		return (1);
	}
	(void)snprintf(buf, sizeof(buf), "ROWS=%lu", val);
	(void)putenv(buf);

	/* Set resize bit. */
	SF_SET(ep, S_RESIZE);
	return (0);
}

int
f_list(ep, valp, eqp)
	EXF *ep; 
	void *valp;
	char *eqp;
{
	SF_SET(ep, S_REFORMAT);
	return (0);
}

int
f_modelines(ep, valp, eqp)
	EXF *ep;
	void *valp;
	char *eqp;
{
	if (ISSET(O_MODELINES)) {
		ep->msg(ep, M_ERROR, "The modelines option may not be set.");
		UNSET(O_MODELINES);
	}
	return (0);
}

int
f_ruler(ep, valp, eqp)
	EXF *ep; 
	void *valp;
	char *eqp;
{
	scr_modeline(ep, 0);
	return (0);
}

int
f_shiftwidth(ep, valp, eqp)
	EXF *ep; 
	void *valp;
	char *eqp;
{
	u_long val;

	val = *(u_long *)valp;

	if (val > LVAL(O_COLUMNS)) {
		ep->msg(ep, M_ERROR,
		    "Shiftwidth can't be larger than screen size.");
		return (1);
	}
	return (0);
}

int
f_sidescroll(ep, valp, eqp)
	EXF *ep; 
	void *valp;
	char *eqp;
{
	u_long val;

	val = *(u_long *)valp;

	if (val > LVAL(O_COLUMNS)) {
		ep->msg(ep, M_ERROR,
		    "Sidescroll can't be larger than screen size.");
		return (1);
	}
	return (0);
}

int
f_tabstop(ep, valp, eqp)
	EXF *ep; 
	void *valp;
	char *eqp;
{
	u_long val;

	val = *(u_long *)valp;

	if (val == 0) {
		ep->msg(ep, M_ERROR, "Tab stops can't be set to 0.");
		return (1);
	}
#define	MAXTABSTOP	20
	if (val > MAXTABSTOP) {
		ep->msg(ep, M_ERROR,
		    "Tab stops can't be larger than %d.", MAXTABSTOP);
		return (1);
	}
	if (val > LVAL(O_COLUMNS)) {
		ep->msg(ep, M_ERROR,
		    "Tab stops can't be larger than screen size.",
		    MAXTABSTOP);
		return (1);
	}
	return (0);
}

/*
 * F_tags --
 *	Build an array of pathnames for the tags routines.  It's not
 *	a fast build as we walk the string twice, but it's unclear we
 *	care.
 */
int
f_tags(ep, valp, eqp)
	EXF *ep;
	void *valp;
	char *eqp;
{
	TAGF *tfp;
	size_t len;
	int cnt;
	char *p, *t;

	if (ep->tfhead != NULL) {		/* Free up previous array. */
		for (cnt = 0; ep->tfhead[cnt] != NULL; ++cnt)
			free(ep->tfhead[cnt]->fname);
		free(ep->tfhead);
	}
	for (p = t = eqp, cnt = 0;; ++p) {	/* Count new entries. */
		if (*p == '\0' || isspace(*p)) {
			if ((len = p - t) > 1)
				++cnt;
			t = p + 1;
		}
		if (*p == '\0')
			break;
	}
						/* Allocate new array. */
	if ((ep->tfhead = malloc((cnt + 1) * sizeof(TAGF))) == NULL)
		goto mem2;
	ep->tfhead[cnt] = NULL;
	for (p = t = eqp, cnt = 0;; ++p) {	/* Fill in new array. */
		if (*p == '\0' || isspace(*p)) {
			if ((len = p - t) > 1) {
				if ((ep->tfhead[cnt] =
				    malloc(sizeof(TAGF))) == NULL)
					goto mem1;
				if ((ep->tfhead[cnt]->fname =
				    malloc(len + 1)) == NULL) {
mem1:					ep->tfhead[cnt] = NULL;
mem2:					ep->msg(ep, M_ERROR,
					    "Error: %s", strerror(errno));
					return (1);
				}
				memmove(ep->tfhead[cnt]->fname, t, len);
				ep->tfhead[cnt]->fname[len] = '\0';
				ep->tfhead[cnt]->flags = 0;
				++cnt;
			}
			t = p + 1;
		}
		if (*p == '\0')
			 break;
	}
	return (0);
}

int
f_term(ep, valp, eqp)
	EXF *ep;
	void *valp;
	char *eqp;
{
	return (f_flash(ep, NULL, NULL));
}

int
f_wrapmargin(ep, valp, eqp)
	EXF *ep; 
	void *valp;
	char *eqp;
{
	u_long val;

	val = *(u_long *)valp;

	if (val > LVAL(O_COLUMNS)) {
		ep->msg(ep, M_ERROR,
		    "Wrapmargin value can't be larger than screen size.");
		return (1);
	}
	return (0);
}
