/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: seq.c,v 5.16 1992/11/02 22:29:25 bostic Exp $ (Berkeley) $Date: 1992/11/02 22:29:25 $";
#endif /* not lint */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "seq.h"
#include "term.h"
#include "extern.h"

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
	u_char *name, *input, *output;
	enum seqtype stype;
	int userdef;
{
	register SEQ *ip, *sp;
	int ilen;

#if DEBUG && 0
	TRACE("seq_set: name {%s} input {%s} output {%s}\n",
	    name ? name : "", input, output);
#endif
	/*
	 * Find any previous occurrence, and replace the output field.
	 * At the same time, decide where to insert a new structure if
	 * it's needed.
	 */
	ip = NULL;
	ilen = USTRLEN(input);
	for (sp = seq[*input]; sp; sp = sp->next) {
		if (stype != sp->stype)
			continue;
		if (!USTRCMP(sp->input, input)) {
			free(sp->output);
			if ((sp->output = USTRDUP(output)) == NULL)
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
		if ((sp->name = USTRDUP(name)) == NULL)
			goto mem2;
	if ((sp->input = USTRDUP(input)) == NULL)
		goto mem3;
	if ((sp->output = USTRDUP(output)) == NULL)
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
int
seq_delete(input, stype)
	u_char *input;
	enum seqtype stype;
{
	register SEQ *sp;

	if ((sp = seq_find(input, USTRLEN(input), stype, NULL)) == NULL)
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
	u_char *input;
	size_t ilen;
	enum seqtype stype;
	int *ispartialp;
{
	register SEQ *sp;

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
				if (!USTRNCMP(sp->input, input, sp->ilen))
					return (sp);
			} else {
				if (!USTRNCMP(sp->input, input, ilen))
					*ispartialp = 1;
			}
		}
	} else
		for (sp = seq[*input]; sp; sp = sp->next) {
			if (stype != sp->stype)
				continue;
			if (!USTRNCMP(sp->input, input, ilen))
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
	register int ch, cnt, len;
	register u_char *p;

	if (seqhead.lnext == (SEQ *)&seqhead)
		return (0);

	cnt = 0;
	for (sp = seqhead.lnext; sp != (SEQ *)&seqhead; sp = sp->lnext) {
		if (stype != sp->stype)
			continue;
		++cnt;
		for (p = sp->input, len = 0; (ch = *p); ++p, ++len)
			if (iscntrl(ch)) {
				(void)putc('^', curf->stdfp);
				(void)putc(ch + 0x40, curf->stdfp);
			} else
				(void)putc(ch, curf->stdfp);
		for (len = TAB - len % TAB; len; --len)
			(void)putc(' ', curf->stdfp);

		for (p = sp->output; (ch = *p); ++p)
			if (iscntrl(ch)) {
				(void)putc('^', curf->stdfp);
				(void)putc(ch + 0x40, curf->stdfp);
			} else
				(void)putc(ch, curf->stdfp);

		if (isname && sp->name) {
			for (len = TAB - len % TAB; len; --len)
				(void)putc(' ', curf->stdfp);
			for (p = sp->name, len = 0; (ch = *p); ++p, ++len)
				if (iscntrl(ch)) {
					(void)putc('^', curf->stdfp);
					(void)putc(ch + 0x40, curf->stdfp);
				} else
					(void)putc(ch, curf->stdfp);
		}
		(void)putc('\n', curf->stdfp);
	}
	return (cnt);
}

/*
 * seq_save --
 *	Save the sequence entries to a file.
 */
int
seq_save(fp, prefix, stype)
	FILE *fp;
	u_char *prefix;
	enum seqtype stype;
{
	register SEQ *sp;
	register int ch;
	register u_char *p;

	/* Write a sequence command for all keys the user defined. */
	for (sp = seqhead.lnext; sp != (SEQ *)&seqhead; sp = sp->lnext) {
		if (!(sp->flags & S_USERDEF))
			continue;
		if (prefix)
			(void)fprintf(fp, "%s", prefix);
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
