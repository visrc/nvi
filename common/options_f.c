/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: options_f.c,v 5.4 1993/03/28 19:05:24 bostic Exp $ (Berkeley) $Date: 1993/03/28 19:05:24 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "tag.h"

int
f_columns(sp, valp, eqp)
	SCR *sp; 
	void *valp;
	char *eqp;
{
	u_long val;
	char buf[25];

	val = *(u_long *)valp;

	if (val < MINIMUM_SCREEN_COLS) {
		msgq(sp, M_ERROR, "Screen columns too small, less than %d.",
		    MINIMUM_SCREEN_COLS);
		return (1);
	}
	if (val < LVAL(O_SHIFTWIDTH)) {
		msgq(sp, M_ERROR,
		    "Screen columns too small, less than shiftwidth.");
		return (1);
	}
	if (val < LVAL(O_SIDESCROLL)) {
		msgq(sp, M_ERROR,
		    "Screen columns too small, less than sidescroll.");
		return (1);
	}
	if (val < LVAL(O_TABSTOP)) {
		msgq(sp, M_ERROR,
		    "Screen columns too small, less than tabstop.");
		return (1);
	}
	if (val < LVAL(O_WRAPMARGIN)) {
		msgq(sp, M_ERROR,
		    "Screen columns too small, less than wrapmargin.");
		return (1);
	}
#ifdef notdef
	/*
	 * This has to be checked by reaching down into the screen code.
	 */
	if (val < O_NUMBER_LENGTH) {
		msgq(sp, M_ERROR,
		    "Screen columns too small, less than number option.");
		return (1);
	}
#endif
	(void)snprintf(buf, sizeof(buf), "COLUMNS=%lu", val);
	(void)putenv(buf);

	/* Set resize bit. */
	F_SET(sp, S_RESIZE);
	return (0);
}

int
f_flash(sp, valp, eqp)
	SCR *sp;
	void *valp;
	char *eqp;
{
	size_t len;
	char *s, *t, b1[2048], b2[2048];
	
	if ((s = getenv("TERM")) == NULL) {
		msgq(sp, M_ERROR,
		    "No \"TERM\" value set in the environment.");
		return (1);
	}

	/* Get the termcap information. */
	if (tgetent(b1, s) != 1) {
		msgq(sp, M_ERROR, "No termcap entry for %s.", s);
		return (1);
	}

	/*
	 * Get the visual bell string.  If one doesn't exist, then
	 * set O_ERRORBELLS.
	 */
	s = b2;
	if (tgetstr("vb", &s) == NULL) {
		SET(O_ERRORBELLS);
		UNSET(O_FLASH);
		return (1);
	}

	len = s - b2;
	if ((t = malloc(len)) == NULL) {
		msgq(sp, M_ERROR, "Error: %s", strerror(errno));
		return (1);
	}

	memmove(t, b2, len);

	if (sp->VB != NULL)
		free(sp->VB);
	sp->VB = t;

	return (0);
}

int
f_mesg(sp, valp, eqp)
	SCR *sp;
	void *valp;
	char *eqp;
{
	struct stat sb;
	char *tty;

	if ((tty = ttyname(STDERR_FILENO)) == NULL) {
		msgq(sp, M_ERROR, "ttyname: %s", strerror(errno));
		return (1);
	}
	if (stat(tty, &sb) < 0) {
		msgq(sp, M_ERROR, "%s: %s", strerror(errno));
		return (1);
	}

	F_SET(sp->gp, G_SETMODE);
	sp->gp->origmode = sb.st_mode;

	if (ISSET(O_MESG)) {
		if (chmod(tty, sb.st_mode | S_IWGRP) < 0) {
			msgq(sp, M_ERROR, "%s: %s", strerror(errno));
			return (1);
		}
	} else
		if (chmod(tty, sb.st_mode & ~S_IWGRP) < 0) {
			msgq(sp, M_ERROR, "%s: %s", strerror(errno));
			return (1);
		}
	return (0);
}

int
f_keytime(sp, valp, eqp)
	SCR *sp; 
	void *valp;
	char *eqp;
{
	u_long val;

	val = *(u_long *)valp;

#define	MAXKEYTIME	20
	if (val > MAXKEYTIME) {
		msgq(sp, M_ERROR,
		    "Keytime too large, more than %d.", MAXKEYTIME);
		return (1);
	}
	return (0);
}

int
f_leftright(sp, valp, eqp)
	SCR *sp;
	void *valp;
	char *eqp;
{
	F_SET(sp, S_REDRAW);
	return (0);
}

int
f_lines(sp, valp, eqp)
	SCR *sp; 
	void *valp;
	char *eqp;
{
	u_long val;
	char buf[25];

	val = *(u_long *)valp;

	if (val < MINIMUM_SCREEN_ROWS) {
		msgq(sp, M_ERROR, "Screen lines too small, less than %d.",
		    MINIMUM_SCREEN_ROWS);
		return (1);
	}
	(void)snprintf(buf, sizeof(buf), "ROWS=%lu", val);
	(void)putenv(buf);

	/* Set resize bit. */
	F_SET(sp, S_RESIZE);
	return (0);
}

int
f_list(sp, valp, eqp)
	SCR *sp; 
	void *valp;
	char *eqp;
{
	F_SET(sp, S_REFORMAT);
	return (0);
}

int
f_modelines(sp, valp, eqp)
	SCR *sp;
	void *valp;
	char *eqp;
{
	if (ISSET(O_MODELINES)) {
		msgq(sp, M_ERROR, "The modelines option may not be set.");
		UNSET(O_MODELINES);
	}
	return (0);
}

int
f_ruler(sp, valp, eqp)
	SCR *sp; 
	void *valp;
	char *eqp;
{
	/*
	 * XXX
	 * Fix this when the options code gets reworked.
	 * scr_modeline(sp, 0);
	 */
	return (0);
}

int
f_shiftwidth(sp, valp, eqp)
	SCR *sp; 
	void *valp;
	char *eqp;
{
	u_long val;

	val = *(u_long *)valp;

	if (val > LVAL(O_COLUMNS)) {
		msgq(sp, M_ERROR,
		    "Shiftwidth can't be larger than screen size.");
		return (1);
	}
	return (0);
}

int
f_sidescroll(sp, valp, eqp)
	SCR *sp; 
	void *valp;
	char *eqp;
{
	u_long val;

	val = *(u_long *)valp;

	if (val > LVAL(O_COLUMNS)) {
		msgq(sp, M_ERROR,
		    "Sidescroll can't be larger than screen size.");
		return (1);
	}
	return (0);
}

int
f_tabstop(sp, valp, eqp)
	SCR *sp; 
	void *valp;
	char *eqp;
{
	u_long val;

	val = *(u_long *)valp;

	if (val == 0) {
		msgq(sp, M_ERROR, "Tab stops can't be set to 0.");
		return (1);
	}
#define	MAXTABSTOP	20
	if (val > MAXTABSTOP) {
		msgq(sp, M_ERROR,
		    "Tab stops can't be larger than %d.", MAXTABSTOP);
		return (1);
	}
	if (val > LVAL(O_COLUMNS)) {
		msgq(sp, M_ERROR,
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
f_tags(sp, valp, eqp)
	SCR *sp;
	void *valp;
	char *eqp;
{
	size_t len;
	int cnt;
	char *p, *t;

	if (sp->tfhead != NULL) {		/* Free up previous array. */
		for (cnt = 0; sp->tfhead[cnt] != NULL; ++cnt)
			free(sp->tfhead[cnt]->fname);
		free(sp->tfhead);
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
	if ((sp->tfhead = malloc((cnt + 1) * sizeof(TAGF))) == NULL)
		goto mem2;
	sp->tfhead[cnt] = NULL;
	for (p = t = eqp, cnt = 0;; ++p) {	/* Fill in new array. */
		if (*p == '\0' || isspace(*p)) {
			if ((len = p - t) > 1) {
				if ((sp->tfhead[cnt] =
				    malloc(sizeof(TAGF))) == NULL)
					goto mem1;
				if ((sp->tfhead[cnt]->fname =
				    malloc(len + 1)) == NULL) {
mem1:					sp->tfhead[cnt] = NULL;
mem2:					msgq(sp, M_ERROR,
					    "Error: %s", strerror(errno));
					return (1);
				}
				memmove(sp->tfhead[cnt]->fname, t, len);
				sp->tfhead[cnt]->fname[len] = '\0';
				sp->tfhead[cnt]->flags = 0;
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
f_term(sp, valp, eqp)
	SCR *sp;
	void *valp;
	char *eqp;
{
	return (f_flash(sp, NULL, NULL));
}

int
f_wrapmargin(sp, valp, eqp)
	SCR *sp; 
	void *valp;
	char *eqp;
{
	u_long val;

	val = *(u_long *)valp;

	if (val > LVAL(O_COLUMNS)) {
		msgq(sp, M_ERROR,
		    "Wrapmargin value can't be larger than screen size.");
		return (1);
	}
	return (0);
}
