/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_args.c,v 5.1 1992/01/16 13:27:25 bostic Exp $ (Berkeley) $Date: 1992/01/16 13:27:25 $";
#endif /* not lint */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "vi.h"
#include "extern.h"

static char	**s_flist;		/* start of file list */
static int	total, current;

static void	set_file_buf __P((char *));

/*
 * next_file -- (:next [files])
 *	Edit the next file.
 */
void
next_file(frommark, tomark, cmd, bang, extra)
	MARK frommark, tomark;
	CMD cmd;
	int bang;
	char *extra;
{
	/* It's permissible to specify a list of files with :next. */
	if (extra)
		set_file_buf(extra);

	/* Special case for starting a list. */
	if (current == 0) {
		tmpstart(s_flist[0]);
		current = 1;
		return;
	}

	if (current < total)
		if (tmpabort(bang)) {
			tmpstart(s_flist[current]);
			++current;
		} else
			msg("use :next! to discard changes, or w to save them");
	else
		msg("No more files to edit");
}

/*
 * prev_file -- (:prev)
 *	Edit the previous file.
 */
void
prev_file(frommark, tomark, cmd, bang, extra)
	MARK frommark, tomark;
	CMD cmd;
	int bang;
	char *extra;
{
	if (current > 1)
		if (tmpabort(bang)) {
			--current;
			tmpstart(s_flist[current - 1]);
		} else
			msg("use :prev! to discard changes, or w to save them");
	else
		msg("No previous files to edit");
}

/*
 * rew_file -- (:rew)
 *	Edit the first file.
 */
void
rew_file(frommark, tomark, cmd, bang, extra)
	MARK frommark, tomark;
	CMD cmd;
	int bang;
	char *extra;
{
	if (current > 1)
		if (tmpabort(bang)) {
			tmpstart(s_flist[0]);
			current = 1;
		} else
			msg("use :rew! to discard changes, or w to save them");
	else
		msg("No previous files to edit");
}

/*
 * args_file -- (:args [files])
 *	Display the list of files or set the list of files.
 */
void
args_file(frommark, tomark, cmd, bang, extra)
	MARK frommark, tomark;
	CMD cmd;
	int bang;
	char *extra;
{
	int cnt;
	register char **p, *sep;

	if (s_flist == NULL) {
		msg("No file names");
		return;
	}

	/*
	 * XXX
	 * Should break the margin at whitespace.
	 */
	for (p = s_flist, sep = "", cnt = 1; *p; ++p, sep = " ", ++cnt) {
		addstr(sep);
		if (cnt == current)
			addch('[');
		addstr(*p);
		if (cnt == current)
			addch(']');
	}
	/*
	 * XXX
	 * Shouldn't add newline unless over COLUMN chars in length.
	 */
	if (mode == MODE_EX || mode == MODE_COLON)
		addch('\n');
	exrefresh();
}

/*
 * set_file --
 *	Set the file list from an argc/argv.
 */
void
set_file(argc, argv)
	int argc;
	char *argv[];
{
	s_flist = argv;
	current = 0;
	total = argc;
}
	
/*
 * set_file_buf --
 *	Set the file list from a white-space separated buffer.
 */
static void
set_file_buf(buf)
	char *buf;
{
	static int allocated;
	int cnt;
	char **ap, **argv, *bp, *p, *start;
	
	/* Figure out how many file names and allocate space for them. */
	if ((bp = start = strdup(buf)) == NULL)
		goto mem3;
	for (cnt = 1; (p = strsep(&bp, " \t")) != NULL;)
		if (*p != '\0')
			++cnt;
	free(start);

	if ((argv = malloc((size_t)(cnt * sizeof(char *)))) == NULL)
		goto mem3;
	if ((bp = start = strdup(buf)) == NULL) 
		goto mem2;

	/* Put the names into the array, and NULL terminate it. */
	for (ap = argv; (*ap = strsep(&bp, " \t")) != NULL;)
		if (**ap != '\0') {
			if ((*ap = strdup(*ap)) == NULL)
				goto mem1;
			++ap;
		}
	*ap = NULL;

	if (allocated) {
		for (ap = s_flist; *ap; ++ap)
			free(*ap);
		free(s_flist);
	} else
		allocated = 1;

	set_file(cnt - 1, argv);
	return;

mem1:	while (--ap >= argv)
		free(*ap);
	free(start);
mem2:	free(argv);
mem3:	msg("args failed: %s", strerror(errno));
	return;
}

/*
 * cnt_file --
 *	Return a count of the files left to edit.
 */
int
cnt_file()
{
	return (total - current);
}
