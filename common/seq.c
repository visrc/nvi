/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: seq.c,v 5.2 1992/04/05 16:59:56 bostic Exp $ (Berkeley) $Date: 1992/04/05 16:59:56 $";
#endif /* not lint */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "seq.h"

SEQ *seq[UCHAR_MAX];
SEQLIST seqhead;

/*
 * seq_init --
 *	Initialize the sequence lists.
 */
void
seq_init()
{
	seqhead.lnext = seqhead.lprev = (SEQ *)&seqhead;
}

/*
 * seq_set --
 *	Internal version to enter a sequence.
 */
int
seq_set(name, input, output, stype, userdef)
	char *name, *input, *output;
	enum seqtype stype;
	int userdef;
{
	register SEQ *ip, *sp;
	int ilen;
	char *s;

	/*
	 * Find any previous occurrence, and replace the output field.
	 * At the same time, decide where to insert a new structure if
	 * it's needed.
	 */
	ip = NULL;
	ilen = strlen(input);
	for (sp = seq[*input]; sp; sp = sp->next) {
		if (stype != sp->stype)
			continue;
		if (!strcmp(sp->input, input)) {
			free(sp->output);
			if ((sp->output = strdup(output)) == NULL)
				goto mem1;
			return (0);
		}
		if (sp->ilen < ilen)
			ip = sp;
	}

	/* Allocate space. */
	if ((sp = malloc(sizeof(SEQ))) == NULL) 
		goto mem1;

	if (name == NULL)
		sp->name = NULL;
	else 
		if ((sp->name = strdup(name)) == NULL)
			goto mem2;
	if ((sp->input = strdup(input)) == NULL)
		goto mem3;
	if ((sp->output = strdup(output)) == NULL)
		goto mem4;

	sp->stype = stype;
	sp->ilen = ilen;
	sp->flags |= userdef ? S_USERDEF : 0;

	/*
	 * Link into the character array.  If ip is NULL, sp becomes
	 * the first entry in the list whether or not there are any
	 * other entries.
	 */
	if (ip == NULL) {
		if (seq[*input]) {
			sp->next = seq[*input];
			sp->next->prev = sp;
		} else {
			seq[*input] = sp;
			sp->next = NULL;
		}
		sp->prev = NULL;
	} else {
		if (ip->next)
			ip->next->prev = sp;
		sp->next = ip->next;
		ip->next = sp;
		sp->prev = ip;
	}

	/* Link into the sequence list. */
	inseq(sp, &seqhead);
	return (0);

mem4:	free(sp->input);
mem3:	if (sp->name)
		free(sp->name);
mem2:	free(sp);
mem1:	msg("Error: %s", strerror(errno));
	return (1);
}

/*
 * seq_delete --
 *	Delete a sequence.
 */
seq_delete(input, stype)
	char *input;
	enum seqtype stype;
{
	register SEQ *sp;

	if ((sp = seq_find(input, strlen(input), stype, NULL)) == NULL)
		return (1);

	/* Unlink out of the character array. */
	if (sp->prev) {
		if (sp->next) {
			sp->next->prev = sp->prev;
			sp->prev->next = sp->next;
		} else
			sp->prev->next = NULL;
	} else if (sp->next) {
		seq[*input] = sp->next;
		sp->next->prev = NULL;
	} else
		seq[*input] = NULL;

	/* Unlink out of the sequence list. */
	rmseq(sp);

	/* Free up the space. */
	if (sp->name)
		free(sp->name);
	free(sp->input);
	free(sp->output);
	free(sp);
	return (0);
}

/*
 * seq_find --
 *	Search the sequence list for a match to a buffer, if ispartial
 *	isn't NULL, partial matches count.
 */
SEQ *
seq_find(input, ilen, stype, ispartialp)
	char *input;
	size_t ilen;
	enum seqtype stype;
	int *ispartialp;
{
	register SEQ *sp;
	register int len;

	if (ispartialp) {
		*ispartialp = 0;
		for (sp = seq[*input]; sp; sp = sp->next) {
			if (stype != sp->stype)
				continue;
			/*
			 * If sequence is shorter or the same length as the
			 * input, can only find an exact match.  If input is
			 * shorter than the sequence, can only find a partial.
			 */
			if (sp->ilen <= ilen) {
				if (!strncmp(sp->input, input, sp->ilen))
					return (sp);
			} else {
				if (!strncmp(sp->input, input, ilen))
					*ispartialp = 1;
			}
		}
	} else
		for (sp = seq[*input]; sp; sp = sp->next) {
			if (stype != sp->stype)
				continue;
			if (!strncmp(sp->input, input, ilen))
				return (sp);
		}
	return (NULL);
}

/*
 * seq_dump --
 *	Display the sequence entries of a specified type.
 */
int
seq_dump(stype, isname)
	enum seqtype stype;
	int isname;
{
	register SEQ *sp;
	register int ch, len;
	register char *p;

	for (sp = seqhead.lnext; sp != (SEQ *)&seqhead; sp = sp->lnext) {
		if (stype != sp->stype)
			continue;
		if (isname)
			if (sp->name) {
				for (p = sp->name, len = 0;
				    (ch = *p); ++p, ++len)
					if (iscntrl(ch)) {
						qaddch('^');
						qaddch(ch ^ '@');
					} else
						qaddch(ch);
				for (len = TAB - len % TAB; len; --len)
					qaddch(' ');
			} else
				qaddstr("        ");

		for (p = sp->input, len = 0; (ch = *p); ++p, ++len)
			if (iscntrl(ch)) {
				qaddch('^');
				qaddch(ch ^ '@');
			} else
				qaddch(ch);
		for (len = TAB - len % TAB; len; --len)
			qaddch(' ');

		for (p = sp->output; (ch = *p); ++p)
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
 * seq_save --
 *	Save the sequence entries to a file.
 */
int
seq_save(fp, prefix, stype)
	FILE *fp;
	char *prefix;
	enum seqtype stype;
{
	register SEQ *sp;
	register int ch;
	register char *p;

	/* Write a sequence command for all keys the user defined. */
	for (sp = seqhead.lnext; sp != (SEQ *)&seqhead; sp = sp->lnext) {
		if (!(sp->flags & S_USERDEF))
			continue;
		if (prefix)
			(void)fputs(prefix, fp);
		for (p = sp->input; ch = *p; ++p) {
			if (!isprint(ch) || ch == '|')
				(void)putc('\026', fp);
			(void)putc(ch, fp);
		}
		(void)putc(' ', fp);
		for (p = sp->output; ch = *p; ++p) {
			if (!isprint(ch) || ch == '|')
				(void)putc('\026', fp);		/* 026 == ^V */
			(void)putc(ch, fp);
		}
	}
	return (0);
}
