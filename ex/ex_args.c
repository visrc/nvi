/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_args.c,v 5.8 1992/04/19 08:53:39 bostic Exp $ (Berkeley) $Date: 1992/04/19 08:53:39 $";
#endif /* not lint */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "curses.h"
#include "excmd.h"
#include "extern.h"

static char	**s_flist;		/* start of file list */
static int	total, current;

/*
 * cmd_next -- (:next [files])
 *	Edit the next file.
 */
int
ex_next(cmdp)
	EXCMDARG *cmdp;
{
	/* It's permissible to specify a list of files with :next. */
	if (cmdp->argc)
		file_set(cmdp->argc, cmdp->argv, 1);

	if (current < total)
		if (tmpabort(cmdp->flags & E_FORCE)) {
			tmpstart(s_flist[current]);
			++current;
			return (0);
		} else
			msg("Use next! to discard changes, or w to save them.");
	else
		msg("No more files to edit.");
	return (1);
}

/*
 * file_prev -- (:prev)
 *	Edit the previous file.
 */
int
ex_prev(cmdp)
	EXCMDARG *cmdp;
{
	if (current > 1)
		if (tmpabort(cmdp->flags & E_FORCE)) {
			--current;
			tmpstart(s_flist[current - 1]);
			return (0);
		} else
			msg("Use prev! to discard changes, or w to save them.");
	else
		msg("No previous files to edit.");
	return (1);
}

/*
 * file_rew -- (:rew)
 *	Edit the first file.
 */
int
ex_rew(cmdp)
	EXCMDARG *cmdp;
{
	if (current > 1)
		if (tmpabort(cmdp->flags & E_FORCE)) {
			tmpstart(s_flist[0]);
			current = 1;
			return (0);
		} else
			msg("Use rew! to discard changes, or w to save them.");
	else
		msg("No previous files to edit.");
	return (1);
}

/*
 * file_args -- (:args)
 *	Display the list of files.
 */
int
ex_args(cmdp)
	EXCMDARG *cmdp;
{
	register char **p, *sep;
	int cnt, col, len, newline;

	if (current == 0) {
		msg("No file names.");
		return (1);
	}

	col = len = newline = 0;
	for (p = s_flist, sep = "", cnt = 1; *p; ++p, ++cnt) {
		col += len =
		    strlen(*p) + (*sep ? 1 : 0) + (cnt == current ? 2 : 0);
		if (col >= COLS - 4) {
			addch('\n');
			sep = "";
			col = len;
			newline = 1;
		} else if (cnt != 1)
			sep = " ";
		addstr(sep);
		if (cnt == current)
			addch('[');
		addstr(*p);
		if (cnt == current)
			addch(']');
	}
	if (newline)
		addch('\n');
	ex_refresh();
	return (0);
}

/*
 * file_set --
 *	Set the file list from an argc/argv.
 */
void
file_set(argc, argv, copy)
	int argc, copy;
	char *argv[];
{
	static char **copyav;
	int cnt;
	char **avp;

	/* Copy the array and strings if necessary. */
	if (copy) {
		if (copyav) {
			for (avp = copyav; *avp; ++avp)
				free(*avp);
			free(copyav);
		}
		if ((copyav = malloc((argc + 1) * sizeof(char *))) == NULL) {
			msg("Error: %s.", strerror(errno));
			return;
		}
		for (cnt = 0; cnt < argc; ++cnt)
			if ((copyav[cnt] = strdup(argv[cnt])) == NULL) {
				msg("Error: %s.", strerror(errno));
				break;
			}
		copyav[cnt] = NULL;
		s_flist = copyav;
	} else
		s_flist = argv;

	total = argc;
	current = 0;
}

/*
 * file_cnt --
 *	Return a count of the files left to edit.
 */
int
file_cnt()
{
	return (total - current);
}
