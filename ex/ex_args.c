/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_args.c,v 5.11 1992/04/28 11:56:17 bostic Exp $ (Berkeley) $Date: 1992/04/28 11:56:17 $";
#endif /* not lint */

#include <sys/types.h>
#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

static char	**s_flist;		/* start of file list */
static int	total, current;

/*
 * ex_next -- :next [files]
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
 * ex_prev -- :prev
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
 * ex_rew -- :rew
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
 * ex_args -- :args
 *	Display the list of files.
 */
int
ex_args(cmdp)
	EXCMDARG *cmdp;
{
	register char **p;
	register int cnt, col, sep;
	int len;

	if (current == 0) {
		msg("No file names.");
		return (1);
	}

	EX_PRSTART;
	col = len = sep = 0;
	for (p = s_flist, cnt = 1; *p; ++p, ++cnt) {
		col += len = strlen(*p) + sep + (cnt == current ? 2 : 0);
		if (col >= COLS - 1) {
			col = len;
			sep = 0;
			EX_PRNEWLINE;
		} else if (cnt != 1) {
			sep = 1;
			(void)putchar(' ');
		}
		if (cnt == current)
			(void)printf("[%s]", *p);
		else
			(void)printf("%s", *p);
	}
	EX_PRTRAIL;
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
