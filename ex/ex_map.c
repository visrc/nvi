/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_map.c,v 5.3 1992/04/05 09:23:40 bostic Exp $ (Berkeley) $Date: 1992/04/05 09:23:40 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "map.h"
#include "excmd.h"
#include "extern.h"

static int map_dump __P((enum whenmapped));
static int map_set __P((char *, char *, char *, enum whenmapped, int));

MAP *map[UCHAR_MAX];
MAPLIST mhead;

/*
 * map_init --
 *	Initialize the termcap key maps.
 */
int
map_init()
{
	int rval;

	/* Link the linked list together. */
	mhead.lnext = mhead.lprev = (MAP *)&mhead;

	/*
	 * The KU, KD, KL,and KR variables correspond to the :ku=: etc.
	 * termcap capabilities.  The variables are defined as part of
	 * the curses package.
	 */
	rval = 0;
	if (has_KU)
		rval |= map_set("up", has_KU, "k", COMMAND, 0);
	if (has_KD)
		rval |= map_set("down", has_KD, "j", COMMAND, 0);
	if (has_KL)
		rval |= map_set("left", has_KL, "h", COMMAND, 0);
	if (has_KR)
		rval |= map_set("right", has_KR, "l", COMMAND, 0);
	if (has_HM)
		rval |= map_set("home", has_HM, "^", COMMAND, 0);
	if (has_EN)
		rval |= map_set("end", has_EN, "$", COMMAND, 0);
	if (has_PU)
		rval |= map_set("page up", has_PU, "\002", COMMAND, 0);
	if (has_PD)
		rval |= map_set("page down", has_PD, "\006", COMMAND, 0);
	if (has_KI)
		rval |= map_set("insert", has_KI, "i", COMMAND, 0);
	if (ERASEKEY != '\177')
		rval |= map_set("delete", "\177", "x", COMMAND, 0);
	return (rval);
}

/*
 * ex_map -- (:map[!] [key replacement])
 *	Map a key or display mapped keys.
 */
int
ex_map(cmdp)
	CMDARG *cmdp;
{
	enum whenmapped when;
	int key;
	char *name, *input, *output, buf[10];

	when = cmdp->flags & E_FORCE ? INPUT : COMMAND;

	if (cmdp->string == NULL) {
		map_dump(when);
		return (0);
	}

	/*
	 * Input is the first word, output is everything else, i.e. any
	 * space characters are included.  This is why we can't parse
	 * this command in the ex parser itself.
	 */
	for (input = output = cmdp->string;
	    *output && !isspace(*output); ++output);
	if (*output != '\0')
		for (*output++ = '\0'; isspace(*output); ++output);
	if (*output == '\0') {
		msg("Usage: %s.", cmdp->cmd->usage);
		return (1);
	}
	
	/*
	 * If the mapped string is #[0-9], then map to a function
	 * key.
	 */
	if (input[0] == '#' && isdigit(input[1]) && !input[2]) {
		key = atoi(input + 1);
		(void)snprintf(buf, sizeof(buf), "f%d", key);
		if (FKEY[key]) {
			input = FKEY[key];
			name = buf;
		} else {
			msg("This terminal has no %s key.", buf);
			return (1);
		}
	} else
		name = NULL;
	return (map_set(name, input, output, when, 1));
}

/*
 * ex_unmap -- (:unmap[!] key)
 *	Unmap a key.
 */
int
ex_unmap(cmdp)
	CMDARG *cmdp;
{
	register MAP *p;
	register char *input;

	input = cmdp->argv[0];
	for (p = map[*input]; p; p = p->next)
		if (!strcmp(p->input, input)) {
			/* Unlink out of the map array. */
			if (p->prev) {
				if (p->next) {
					p->next->prev = p->prev;
					p->prev->next = p->next;
				} else
					p->prev->next = NULL;
			} else if (p->next) {
				map[*input] = p->next;
				p->next->prev = NULL;
			} else
				map[*input] = NULL;
			
			/* Unlink out of the map list. */
			p->lprev->lnext = p->lnext;
			p->lnext->lprev = p->lprev;

			/* Free up the space. */
			free(p->name);
			free(p->input);
			free(p->output);
			free(p);
			return (0);
		}
	msg("The key sequence \"%s\" was never mapped.", input);
	return (1);
}

/*
 * map_set --
 *	Internal version to map a key.
 */
static int
map_set(name, input, output, when, userdef)
	char *name, *input, *output;
	enum whenmapped when;
	int userdef;
{
	register MAP *mp;
	register int cnt;
	register char *ip, *p;
	char *start;

	for (mp = map[*input]; mp; mp = mp->next)
		if (!strcmp(mp->input, input)) {
			free(mp->output);
			if ((mp->output = strdup(output)) == NULL)
				goto mem1;
			return (0);
		}

	/* Allocate space. */
	if ((mp = malloc(sizeof(MAP))) == NULL) 
		goto mem1;

	if (name == NULL)
		mp->name = NULL;
	else 
		if ((mp->name = strdup(name)) == NULL)
			goto mem2;
	if ((mp->input = strdup(input)) == NULL)
		goto mem3;
	if ((mp->output = strdup(output)) == NULL)
		goto mem4;

	mp->when = when;
	mp->ilen = strlen(mp->input);
	mp->flags |= userdef ? M_USERDEF : 0;

	/* Link into the map array. */
	mp->next = map[*input];
	map[*input] = mp;
	mp->prev = NULL;

	/* Link into the map list. */
	mp->lnext = mhead.lnext;
	mhead.lnext->lprev = mp;
	mhead.lnext = mp;
	mp->lprev = (MAP *)&mhead;
	return (0);

mem4:	free(mp->input);
mem3:	if (mp->name)
		free(mp->name);
mem2:	free(mp);
mem1:	msg("Error: %s", strerror(errno));
	return (1);
}

/*
 * map_dump --
 *	Display key mappings of a specified type.
 */
int
map_dump(when)
	enum whenmapped when;
{
	register MAP *mp;
	register int ch, cnt, len;
	register char *p;

	for (mp = mhead.lnext; mp != (MAP *)&mhead; mp = mp->lnext) {
		if (when != mp->when)
			continue;
		for (p = mp->name ? mp->name : mp->input, len = 0;
		    (ch = *p); ++p, ++len)
			if (iscntrl(ch)) {
				qaddch('^');
				qaddch(ch ^ '@');
			} else
				qaddch(ch);
		for (cnt = TAB - len % TAB; cnt; --cnt)
			qaddch(' ');
		for (p = mp->input, len = 0; (ch = *p); ++p, ++len)
			if (iscntrl(ch)) {
				qaddch('^');
				qaddch(ch ^ '@');
			} else
				qaddch(ch);
		for (cnt = TAB - len % TAB; cnt; --cnt)
			qaddch(' ');
		for (p = mp->output; (ch = *p); ++p)
			if (iscntrl(ch)) {
				qaddch('^');
				qaddch(ch ^ '@');
			} else
				qaddch(ch);

		addch('\n');
		exrefresh();
	}
	return (0);
}

/*
 * map_save --
 *	Save the current mapped keys to a file.
 */
int
map_save(fp)
	FILE *fp;
{
	register MAP *mp;
	register int ch;
	register char *p;
	char buf[1024];

	/* Write a map command for all keys the user defined. */
	for (mp = mhead.lnext; mp != (MAP *)&mhead; mp = mp->lnext) {
		if (!(mp->flags & M_USERDEF))
			continue;
		(void)putc('m', fp);
		(void)putc('a', fp);
		(void)putc('p', fp);
		if (mp->when == INPUT)
			(void)putc('!', fp);
		(void)putc(' ', fp);
		for (p = mp->input; ch = *p; ++p) {
			if (!isprint(ch) || ch == '|')
				(void)putc('\026', fp);
			(void)putc(ch, fp);
		}
		(void)putc(' ', fp);
		for (p = mp->output; ch = *p; ++p) {
			if (!isprint(ch) || ch == '|')
				(void)putc('\026', fp);		/* 026 == ^V */
			(void)putc(ch, fp);
		}
	}
	return (0);
}

/*
 * map_find --
 *	Search the map for a match to a buffer, partial matches count.
 */
MAP *
map_find(input, when, ispartialp)
	u_char *input;
	enum whenmapped when;
	int *ispartialp;
{
	register MAP *mp;
	register int len;

	*ispartialp = 0;
	len = strlen((char *)input);
	for (mp = map[*input]; mp; mp = mp->next) {
		if (mp->when != when)
			continue;
		if (!strncmp(mp->input, (char *)input, len))
			if (len == mp->ilen) {
				*ispartialp = 0;
				return (mp);
			} else
				*ispartialp = 1;
	}
	return (NULL);
}
