/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: seq.c,v 5.23 1993/03/25 15:00:26 bostic Exp $ (Berkeley) $Date: 1993/03/25 15:00:26 $";
#endif /* not lint */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "seq.h"
#include "term.h"

/*
 * seq_init --
 *	Initialize the sequence lists.
 */
void
seq_init(sp)
	SCR *sp;
{
	sp->seqhdr.next = sp->seqhdr.prev = (SEQ *)&sp->seqhdr;
}

/*
 * seq_set --
 *	Internal version to enter a sequence.
 */
int
seq_set(sp, name, input, output, stype, userdef)
	SCR *sp;
	u_char *name, *input, *output;
	enum seqtype stype;
	int userdef;
{
	register SEQ *ip, *qp;
	int ilen;

#if DEBUG && 0
	TRACE(ep, "seq_set: name {%s} input {%s} output {%s}\n",
	    name ? name : "", input, output);
#endif
	/*
	 * Find any previous occurrence, and replace the output field.
	 * At the same time, decide where to insert a new structure if
	 * it's needed.
	 */
	ip = NULL;
	ilen = USTRLEN(input);
	for (qp = sp->seq[*input]; qp; qp = qp->snext) {
		if (stype != qp->stype)
			continue;
		if (!USTRCMP(qp->input, input)) {
			free(qp->output);
			if ((qp->output = USTRDUP(output)) == NULL)
				goto mem1;
			return (0);
		}
		if (qp->ilen < ilen)
			ip = qp;
	}

	/* Allocate space. */
	if ((qp = malloc(sizeof(SEQ))) == NULL) 
		goto mem1;

	if (name == NULL)
		qp->name = NULL;
	else 
		if ((qp->name = USTRDUP(name)) == NULL)
			goto mem2;
	if ((qp->input = USTRDUP(input)) == NULL)
		goto mem3;
	if ((qp->output = USTRDUP(output)) == NULL)
		goto mem4;

	qp->stype = stype;
	qp->ilen = ilen;
	qp->flags = userdef ? S_USERDEF : 0;

	/*
	 * Link into the character array.  If ip is NULL, qp becomes
	 * the first entry in the list whether or not there are any
	 * other entries.
	 */
	if (ip == NULL) {
		if (sp->seq[*input]) {
			qp->snext = sp->seq[*input];
			qp->snext->sprev = qp;
		} else {
			sp->seq[*input] = qp;
			qp->snext = NULL;
		}
		qp->sprev = NULL;
	} else {
		if (ip->snext)
			ip->snext->sprev = qp;
		qp->snext = ip->snext;
		ip->snext = qp;
		qp->sprev = ip;
	}

	/* Link into the sequence list. */
	INSERT_HEAD(qp, (SEQ *)&sp->seqhdr, next, prev, SEQ);
	return (0);

mem4:	free(qp->input);
mem3:	if (qp->name)
		free(qp->name);
mem2:	free(qp);
mem1:	msgq(sp, M_ERROR, "Error: %s", strerror(errno));
	return (1);
}

/*
 * seq_delete --
 *	Delete a sequence.
 */
int
seq_delete(sp, input, stype)
	SCR *sp;
	u_char *input;
	enum seqtype stype;
{
	register SEQ *qp;

	if ((qp = seq_find(sp, input, USTRLEN(input), stype, NULL)) == NULL)
		return (1);

	/* Unlink out of the character array. */
	if (qp->sprev) {
		if (qp->snext) {
			qp->snext->sprev = qp->sprev;
			qp->sprev->snext = qp->snext;
		} else
			qp->sprev->snext = NULL;
	} else if (qp->snext) {
		sp->seq[*input] = qp->snext;
		qp->snext->sprev = NULL;
	} else
		sp->seq[*input] = NULL;

	/* Unlink out of the sequence list. */
	DELETE(qp, next, prev);

	/* Free up the space. */
	if (qp->name)
		free(qp->name);
	free(qp->input);
	free(qp->output);
	free(qp);
	return (0);
}

/*
 * seq_find --
 *	Search the sequence list for a match to a buffer, if ispartial
 *	isn't NULL, partial matches count.
 */
SEQ *
seq_find(sp, input, ilen, stype, ispartialp)
	SCR *sp;
	u_char *input;
	size_t ilen;
	enum seqtype stype;
	int *ispartialp;
{
	register SEQ *qp;

	if (ispartialp) {
		*ispartialp = 0;
		for (qp = sp->seq[*input]; qp; qp = qp->snext) {
			if (stype != qp->stype)
				continue;
			/*
			 * If sequence is shorter or the same length as the
			 * input, can only find an exact match.  If input is
			 * shorter than the sequence, can only find a partial.
			 */
			if (qp->ilen <= ilen) {
				if (!USTRNCMP(qp->input, input, qp->ilen))
					return (qp);
			} else {
				if (!USTRNCMP(qp->input, input, ilen))
					*ispartialp = 1;
			}
		}
	} else
		for (qp = sp->seq[*input]; qp; qp = qp->snext) {
			if (stype != qp->stype)
				continue;
			if (!USTRNCMP(qp->input, input, ilen))
				return (qp);
		}
	return (NULL);
}

/*
 * seq_dump --
 *	Display the sequence entries of a specified type.
 */
int
seq_dump(sp, stype, isname)
	SCR *sp;
	enum seqtype stype;
	int isname;
{
	register SEQ *qp;
	register int ch, cnt, len, tablen;
	register u_char *p;

	if (sp->seqhdr.next == (SEQ *)&sp->seqhdr)
		return (0);

	cnt = 0;
	tablen = LVAL(O_TABSTOP);
	for (qp = sp->seqhdr.next; qp != (SEQ *)&sp->seqhdr; qp = qp->next) {
		if (stype != qp->stype)
			continue;
		++cnt;
		for (p = qp->input, len = 0; (ch = *p); ++p, ++len)
			if (iscntrl(ch)) {
				(void)putc('^', sp->stdfp);
				(void)putc(ch + 0x40, sp->stdfp);
			} else
				(void)putc(ch, sp->stdfp);
		for (len = tablen - len % tablen; len; --len)
			(void)putc(' ', sp->stdfp);

		for (p = qp->output; (ch = *p); ++p)
			if (iscntrl(ch)) {
				(void)putc('^', sp->stdfp);
				(void)putc(ch + 0x40, sp->stdfp);
			} else
				(void)putc(ch, sp->stdfp);

		if (isname && qp->name) {
			for (len = tablen - len % tablen; len; --len)
				(void)putc(' ', sp->stdfp);
			for (p = qp->name, len = 0; (ch = *p); ++p, ++len)
				if (iscntrl(ch)) {
					(void)putc('^', sp->stdfp);
					(void)putc(ch + 0x40, sp->stdfp);
				} else
					(void)putc(ch, sp->stdfp);
		}
		(void)putc('\n', sp->stdfp);
	}
	return (cnt);
}

/*
 * seq_save --
 *	Save the sequence entries to a file.
 */
int
seq_save(sp, fp, prefix, stype)
	SCR *sp;
	FILE *fp;
	u_char *prefix;
	enum seqtype stype;
{
	register SEQ *qp;
	register int ch;
	register u_char *p;

	/* Write a sequence command for all keys the user defined. */
	for (qp = sp->seqhdr.next; qp != (SEQ *)&sp->seqhdr; qp = qp->next) {
		if (!(qp->flags & S_USERDEF))
			continue;
		if (prefix)
			(void)fprintf(fp, "%s", prefix);
		for (p = qp->input; ch = *p; ++p) {
			if (!isprint(ch) || ch == '|')
				(void)putc('\026', fp);
			(void)putc(ch, fp);
		}
		(void)putc(' ', fp);
		for (p = qp->output; ch = *p; ++p) {
			if (!isprint(ch) || ch == '|')
				(void)putc('\026', fp);		/* 026 == ^V */
			(void)putc(ch, fp);
		}
	}
	return (0);
}
