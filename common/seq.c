/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: seq.c,v 8.17 1993/11/29 14:14:52 bostic Exp $ (Berkeley) $Date: 1993/11/29 14:14:52 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "seq.h"
#include "excmd.h"

/*
 * seq_set --
 *	Internal version to enter a sequence.
 */
int
seq_set(sp, name, input, output, stype, userdef)
	SCR *sp;
	char *name, *input, *output;
	enum seqtype stype;
	int userdef;
{
	SEQ *lastqp, *qp;
	int ilen;
	char *p;

#if defined(DEBUG) && 0
	TRACE(sp, "seq_set: name {%s} input {%s} output {%s}\n",
	    name ? name : "", input, output);
#endif
	/* Just replace the output field in any previous occurrence. */
	ilen = strlen(input);
	if ((qp = seq_find(sp, &lastqp, input, ilen, stype, NULL)) != NULL) {
		if ((p = strdup(output)) == NULL)
			goto mem1;
		FREE(qp->output, qp->olen);
		qp->olen = strlen(output);
		qp->output = p;
		return (0);
	}

	/* Allocate and initialize space. */
	if ((qp = malloc(sizeof(SEQ))) == NULL) 
		goto mem1;
	if (name == NULL)
		qp->name = NULL;
	else if ((qp->name = strdup(name)) == NULL)
		goto mem2;
	if ((qp->input = strdup(input)) == NULL)
		goto mem3;
	if ((qp->output = strdup(output)) == NULL) {
		FREE(qp->input, ilen);
mem3:		if (qp->name != NULL)
			FREE(qp->name, strlen(qp->name) + 1);
mem2:		FREE(qp, sizeof(SEQ));
mem1:		msgq(sp, M_SYSERR, NULL);
		return (1);
	}

	qp->stype = stype;
	qp->ilen = ilen;
	qp->olen = strlen(output);
	qp->flags = userdef ? S_USERDEF : 0;

	/* Link into the chain. */
	if (lastqp == NULL) {
		LIST_INSERT_HEAD(&sp->gp->seqq, qp, q);
	} else {
		LIST_INSERT_AFTER(lastqp, qp, q);
	}

	/* Set the fast lookup bit. */
	bit_set(sp->gp->seqb, qp->input[0]);

	return (0);
}

/*
 * seq_delete --
 *	Delete a sequence.
 */
int
seq_delete(sp, input, stype)
	SCR *sp;
	char *input;
	enum seqtype stype;
{
	SEQ *qp;

	if ((qp =
	    seq_find(sp, NULL, input, strlen(input), stype, NULL)) == NULL)
		return (1);

	LIST_REMOVE(qp, q);
	if (qp->name != NULL)
		FREE(qp->name, strlen(qp->name) + 1);
	FREE(qp->input, qp->ilen);
	FREE(qp->output, qp->olen);
	FREE(qp, sizeof(SEQ));
	return (0);
}

/*
 * seq_find --
 *	Search the sequence list for a match to a buffer, if ispartial
 *	isn't NULL, partial matches count.
 */
SEQ *
seq_find(sp, lastqp, input, ilen, stype, ispartialp)
	SCR *sp;
	SEQ **lastqp;
	char *input;
	size_t ilen;
	enum seqtype stype;
	int *ispartialp;
{
	SEQ *lqp, *qp;
	int diff;

	/*
	 * Ispartialp is a location where we return if there was a
	 * partial match, i.e. if the string were extended it might
	 * match something.
	 * 
	 * XXX
	 * Overload the meaning of ispartialp; only the terminal key
	 * search doesn't want the search limited to complete matches,
	 * i.e. ilen may be longer than the match.
	 */
	if (ispartialp != NULL)
		*ispartialp = 0;
	for (lqp = NULL, qp = sp->gp->seqq.lh_first;
	    qp != NULL; lqp = qp, qp = qp->q.le_next) {
		/* Fast checks on the first character and type. */
		if (qp->input[0] > input[0])
			break;
		if (qp->input[0] < input[0] || qp->stype != stype)
			continue;

		/* Check on the real comparison. */
		diff = memcmp(qp->input, input, MIN(qp->ilen, ilen));
		if (diff > 0)
			break;
		if (diff < 0)
			continue;
		/*
		 * If the entry is the same length as the string, return a
		 * match.  If the entry is shorter than the string, return a
		 * match if called from the terminal key routine.  Otherwise,
		 * keep searching for a complete match.
		 */
		if (qp->ilen <= ilen) {
			if (qp->ilen == ilen || ispartialp != NULL) {
				if (lastqp != NULL)
					*lastqp = lqp;
				return (qp);
			}
			continue;
		}
		/*
		 * If the entry longer than the string, return partial match
		 * if called from the terminal key routine.  Otherwise, no
		 * match.
		 */
		if (ispartialp != NULL)
			*ispartialp = 1;
		break;
	}
	if (lastqp != NULL)
		*lastqp = lqp;
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
	CHNAME const *cname;
	SEQ *qp;
	int ch, cnt, len, tablen;
	char *p;

	cnt = 0;
	cname = sp->gp->cname;
	tablen = O_VAL(sp, O_TABSTOP);
	for (qp = sp->gp->seqq.lh_first; qp != NULL; qp = qp->q.le_next) {
		if (stype != qp->stype)
			continue;
		++cnt;
		for (p = qp->input, len = 0; (ch = *p); ++p, ++len)
			(void)ex_printf(EXCOOKIE, "%s", cname[ch].name);
		for (len = tablen - len % tablen; len; --len)
			(void)ex_printf(EXCOOKIE, " ");

		for (p = qp->output; (ch = *p); ++p)
			(void)ex_printf(EXCOOKIE, "%s", cname[ch].name);

		if (isname && qp->name) {
			for (len = tablen - len % tablen; len; --len)
				(void)ex_printf(EXCOOKIE, " ");
			for (p = qp->name, len = 0; (ch = *p); ++p, ++len)
				(void)ex_printf(EXCOOKIE, "%s", cname[ch].name);
		}
		(void)ex_printf(EXCOOKIE, "\n");
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
	char *prefix;
	enum seqtype stype;
{
	SEQ *qp;
	int ch, esc;
	char *p;

	esc = sp->gp->original_termios.c_cc[VLNEXT];

	/* Write a sequence command for all keys the user defined. */
	for (qp = sp->gp->seqq.lh_first; qp != NULL; qp = qp->q.le_next) {
		if (!F_ISSET(qp, S_USERDEF))
			continue;
		if (stype != qp->stype)
			continue;
		if (prefix)
			(void)fprintf(fp, "%s", prefix);
		for (p = qp->input; (ch = *p) != '\0'; ++p) {
			if (ch == esc || ch == '|' ||
			    isblank(ch) || term_key_val(sp, ch) == K_NL)
				(void)putc(esc, fp);
			(void)putc(ch, fp);
		}
		(void)putc(' ', fp);
		for (p = qp->output; (ch = *p) != '\0'; ++p) {
			if (ch == esc || ch == '|' ||
			    term_key_val(sp, ch) == K_NL)
				(void)putc(esc, fp);
			(void)putc(ch, fp);
		}
		(void)putc('\n', fp);
	}
	return (0);
}
