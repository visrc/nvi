/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: seq.c,v 8.11 1993/11/02 18:43:58 bostic Exp $ (Berkeley) $Date: 1993/11/02 18:43:58 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
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
	/*
	 * Find any previous occurrence, and replace the output field.
	 * Q's are sorted by character and length within that character.
	 */
	ilen = strlen(input);
	for (lastqp = NULL, qp = sp->seqq.le_next;
	    qp != NULL; lastqp = qp, qp = qp->q.qe_next) {
		if (qp->input[0] < input[0])
			continue;
		if (qp->input[0] > input[0] || qp->ilen > ilen)
			break;
		if (qp->ilen == ilen &&
		    stype == qp->stype && !strcmp(qp->input, input)) {
			if ((p = strdup(output)) == NULL)
				goto mem1;
			FREE(qp->output, qp->olen);
			qp->olen = strlen(output);
			qp->output = p;
			return (0);
		}
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
mem1:		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		return (1);
	}

	qp->stype = stype;
	qp->ilen = ilen;
	qp->olen = strlen(output);
	qp->flags = userdef ? S_USERDEF : 0;

	/* Link into the chain. */
	if (lastqp == NULL) {
		list_enter_head(&sp->seqq, qp, SEQ *, q);
	} else {
		list_insert_after(&lastqp->q, qp, SEQ *, q);
	}

	/* Set the fast lookup bit. */
	bit_set(sp->seqb, qp->input[0]);

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

	if ((qp = seq_find(sp, input, strlen(input), stype, NULL)) == NULL)
		return (1);

	list_remove(qp, SEQ *, q);
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
seq_find(sp, input, ilen, stype, ispartialp)
	SCR *sp;
	char *input;
	size_t ilen;
	enum seqtype stype;
	int *ispartialp;
{
	SEQ *qp;

	if (ispartialp)
		*ispartialp = 0;

	if (ispartialp)
		for (qp = sp->seqq.le_next; qp != NULL; qp = qp->q.qe_next) {
			if (qp->input[0] < input[0])
				continue;
			if (qp->input[0] > input[0])
				break;
			if (stype != qp->stype)
				continue;
			/*
			 * If sequence is shorter or the same length as the
			 * input, can only find an exact match.  If input is
			 * shorter than the sequence, can only find a partial.
			 */
			if (qp->ilen <= ilen) {
				if (!strncmp(qp->input, input, qp->ilen))
					return (qp);
			} else {
				if (!strncmp(qp->input, input, ilen))
					*ispartialp = 1;
			}
		}
	else
		for (qp = sp->seqq.le_next; qp != NULL; qp = qp->q.qe_next) {
			if (qp->input[0] < input[0])
				continue;
			if (qp->input[0] > input[0] || qp->ilen > ilen)
				break;
			if (stype == qp->stype && qp->ilen == ilen &&
			    !strncmp(qp->input, input, ilen))
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
	CHNAME const *cname;
	SEQ *qp;
	int ch, cnt, len, tablen;
	char *p;

	cnt = 0;
	cname = sp->cname;
	tablen = O_VAL(sp, O_TABSTOP);
	for (qp = sp->seqq.le_next; qp != NULL; qp = qp->q.qe_next) {
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
	for (qp = sp->seqq.le_next; qp != NULL; qp = qp->q.qe_next) {
		if (!F_ISSET(qp, S_USERDEF))
			continue;
		if (stype != qp->stype)
			continue;
		if (prefix)
			(void)fprintf(fp, "%s", prefix);
		for (p = qp->input; (ch = *p) != '\0'; ++p) {
			if (ch == esc || ch == '|' || isblank(ch) ||
			    sp->special[ch] == K_NL)
				(void)putc(esc, fp);
			(void)putc(ch, fp);
		}
		(void)putc(' ', fp);
		for (p = qp->output; (ch = *p) != '\0'; ++p) {
			if (ch == esc || ch == '|' || sp->special[ch] == K_NL)
				(void)putc(esc, fp);
			(void)putc(ch, fp);
		}
		(void)putc('\n', fp);
	}
	return (0);
}
