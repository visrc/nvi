/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems, Inc.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_tag.c,v 8.12 1993/09/16 10:00:59 bostic Exp $ (Berkeley) $Date: 1993/09/16 10:00:59 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "tag.h"

static char	*binary_search __P((char *, char *, char *));
static int	 compare __P((char *, char *, char *));
static char	*linear_search __P((char *, char *, char *));
static int	 search __P((SCR *, char *, char *, char **));
static int	 tag_get __P((SCR *, char *, char **, char **, char **));

/*
 * ex_tagfirst --
 *	The tag code can be entered from main, i.e. "vi -t tag".
 */
int
ex_tagfirst(sp, tagarg)
	SCR *sp;
	char *tagarg;
{
	EXF *tep;
	FREF *frp;
	MARK m;
	long tl;
	u_int flags;
	int sval;
	char *p, *tag, *fname, *search;

	/* Taglength may limit the number of characters. */
	if ((tl = O_VAL(sp, O_TAGLENGTH)) != 0 && strlen(tagarg) > tl)
		tagarg[tl] = '\0';

	/* Get the tag information. */
	if (tag_get(sp, tagarg, &tag, &fname, &search))
		return (1);

	/* Create the file entry. */
	if ((frp = file_add(sp, NULL, fname, 0)) == NULL)
		return (1);
	if ((tep = file_init(sp, NULL, frp, NULL)) == NULL)
		return (1);

	/*
	 * !!!
	 * Historic vi accepted a line number as well as a search
	 * string, and people are apparently still using the format.
	 */
	if (isdigit(search[0])) {
		m.lno = atoi(search);
		m.cno = 0;
	} else {
		/*
		 * Search for the tag; cheap fallback for C functions if
		 * the name is the same but the arguments have changed.
		 */
		m.lno = 1;
		m.cno = 0;
		flags = SEARCH_FILE | SEARCH_TAG | SEARCH_TERM;
		sval = f_search(sp, tep, &m, &m, search, NULL, &flags);
		if (sval && (p = strrchr(search, '(')) != NULL) {
			p[1] = '\0';
			sval = f_search(sp, tep, &m, &m, search, NULL, &flags);
		}
		if (sval) {
			msgq(sp, M_ERR, "%s: search pattern not found.", tag);
			return (1);
		}
	}

	/* Set up the screen/file. */
	sp->ep = tep;
	frp->lno = m.lno;
	frp->cno = m.cno;
	sp->frp = frp;
	F_SET(frp, FR_CURSORSET);

	/* Might as well make this the default tag. */
	if ((sp->tlast = strdup(tagarg)) == NULL) {
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		return (1);
	}
	return (0);
}

/*
 * ex_tagpush -- :tag [file]
 *	Move to a new tag.
 */
int
ex_tagpush(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	enum {TC_CHANGE, TC_CURRENT} which;
	EXF *tep;
	FREF *frp;
	MARK m;
	TAG *tp;
	u_int flags;
	int sval;
	long tl;
	char *fname, *p, *search, *tag;
	
	switch (cmdp->argc) {
	case 1:
		if (sp->tlast != NULL)
			free(sp->tlast);
		if ((sp->tlast = strdup((char *)cmdp->argv[0])) == NULL) {
			msgq(sp, M_ERR, "Error: %s", strerror(errno));
			return (1);
		}
		break;
	case 0:
		if (sp->tlast == NULL) {
			msgq(sp, M_ERR, "No previous tag entered.");
			return (1);
		}
		break;
	default:
		abort();
	}

	/* Taglength may limit the number of characters. */
	if ((tl = O_VAL(sp, O_TAGLENGTH)) != 0 && strlen(sp->tlast) > tl)
		sp->tlast[tl] = '\0';

	/* Get the tag information. */
	if (tag_get(sp, sp->tlast, &tag, &fname, &search))
		return (1);

	/* Get an FREF structure. */
	if ((frp = file_add(sp, sp->frp, fname, 1)) == NULL) {
		FREE(tag, strlen(tag));
		return (1);
	}

	/* Get an EXF structure, so we can search. */
	if (sp->frp == frp) {
		which = TC_CURRENT;
		tep = sp->ep;
	} else {
		MODIFY_CHECK(sp, sp->ep, F_ISSET(cmdp, E_FORCE));
		if ((tep = file_init(sp, NULL, frp, NULL)) == NULL)
			return (1);
		which = TC_CHANGE;
	}

	/*
	 * !!!
	 * Historic vi accepted a line number as well as a search
	 * string, and people are apparently still using the format.
	 */
	if (isdigit(search[0])) {
		m.lno = atoi(search);
		m.cno = 0;
		sval = 0;
	} else {
		/*
		 * Search for the tag; cheap fallback for C functions
		 * if the name is the same but the arguments have changed.
		 */
		m.lno = 1;
		m.cno = 0;
		flags = SEARCH_FILE | SEARCH_TAG | SEARCH_TERM;
		sval = f_search(sp, tep, &m, &m, search, NULL, &flags);
		if (sval && (p = strrchr(search, '(')) != NULL) {
			p[1] = '\0';
			sval = f_search(sp, tep, &m, &m, search, NULL, &flags);
		}
		if (sval)
			msgq(sp, M_ERR, "%s: search pattern not found.", tag);
	}
	FREE(tag, strlen(tag));

	/*
	 * Save enough information that we can get back; if the malloc
	 * fails, keep going, it just means the user can't return.
	 */
	if ((tp = malloc(sizeof(TAG))) == NULL)
		msgq(sp, M_ERR, "Error: %s.", strerror(errno));
	else {
		tp->frp = sp->frp;
		tp->lno = sp->lno;
		tp->cno = sp->cno;

		HDR_APPEND(tp, &sp->taghdr, next, prev, TAG);
	}

	if (sval) {
		switch (which) {
		case TC_CHANGE:
			sp->n_ep = tep;
			sp->n_frp = frp;
			F_SET(sp, S_FSWITCH);
			break;
		case TC_CURRENT:
			return (1);
		}
	} else switch (which) {
		case TC_CHANGE:
			frp->lno = m.lno;
			frp->cno = m.cno;
			F_SET(frp, FR_CURSORSET);
			sp->n_ep = tep;
			sp->n_frp = frp;
			F_SET(sp, S_FSWITCH);
			break;
		case TC_CURRENT:
			sp->lno = m.lno;
			sp->cno = m.cno;
			break;
		}
	return (0);
}

/*
 * ex_tagpop -- :tagp[op][!]
 *	Pop the tag stack.
 */
int
ex_tagpop(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	TAG *tp;

	/* Pop newest saved information. */
	tp = sp->taghdr.next;
	if (tp == (TAG *)&sp->taghdr) {
		msgq(sp, M_INFO, "The tags stack is empty.");
		return (1);
	}

	/* If not switching files, it's easy; else do the work. */
	if (tp->frp == sp->frp) {
		sp->lno = tp->lno;
		sp->cno = tp->cno;
	} else {
		MODIFY_CHECK(sp, ep, F_ISSET(cmdp, E_FORCE));

		if ((sp->n_ep = file_init(sp, NULL, tp->frp, NULL)) == NULL)
			return (1);

		sp->n_frp = tp->frp;
		tp->frp->lno = tp->lno;
		tp->frp->cno = tp->cno;
		F_SET(sp->n_frp, FR_CURSORSET);

		F_SET(sp, S_FSWITCH);
	}

	/* Delete the saved information from the stack. */
	HDR_DELETE(tp, next, prev, TAG);
	return (0);
}

/*
 * ex_tagtop -- :tagt[op][!]
 *	Clear the tag stack.
 */	
int
ex_tagtop(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	TAG *tp;

	/* Pop oldest saved information. */
	tp = sp->taghdr.prev;
	if (tp == (TAG *)&sp->taghdr) {
		msgq(sp, M_INFO, "The tags stack is empty.");
		return (1);
	}

	/* If not switching files, it's easy; else do the work. */
	if (tp->frp == sp->frp) {
		sp->lno = tp->lno;
		sp->cno = tp->cno;
	} else {
		if ((sp->n_ep = file_init(sp, NULL, tp->frp, NULL)) == NULL)
			return (1);

		sp->n_frp = tp->frp;
		tp->frp->lno = tp->lno;
		tp->frp->cno = tp->cno;
		F_SET(sp->n_frp, FR_CURSORSET);

		F_SET(sp, S_FSWITCH);
	}

	/* Delete the stack. */
	while ((tp = sp->taghdr.next) != (TAG *)&sp->taghdr) {
		HDR_DELETE(tp, next, prev, TAG);
		FREE(tp, sizeof(TAG));
	}
	return (0);
}

/*
 * tag_get --
 *	Get a tag from the tags files.
 */
static int
tag_get(sp, tag, tagp, filep, searchp)
	SCR *sp;
	char *tag, **tagp, **filep, **searchp;
{
	TAGF **tfp;
	int dne;
	char *p;

	/*
	 * Find the tag, only display missing file messages once, and
	 * then only if we didn't find the tag.
	 */
	dne = 0;
	for (tfp = sp->tfhead, p = NULL; *tfp != NULL && p == NULL; ++tfp) {
		errno = 0;
		F_CLR(*tfp, TAGF_DNE);
		if (search(sp, (*tfp)->fname, tag, &p))
			if (errno == ENOENT) {
				if (!F_ISSET(*tfp, TAGF_DNE_WARN)) {
					dne = 1;
					F_SET(*tfp, TAGF_DNE);
				}
			} else
				msgq(sp, M_ERR,
				    "%s: %s", (*tfp)->fname, strerror(errno));
	}
	
	if (p == NULL) {
		msgq(sp, M_ERR, "%s: tag not found.", tag);
		if (dne)
			for (tfp = sp->tfhead; *tfp != NULL; ++tfp)
				if (F_ISSET(*tfp, TAGF_DNE)) {
					msgq(sp, M_ERR, "%s: %s",
					    (*tfp)->fname, strerror(ENOENT));
					F_SET(*tfp, TAGF_DNE_WARN);
				}
		return (1);
	}

	/*
	 * Set the return pointers; tagp points to the tag, and, incidentally
	 * the allocated string, filep points to the nul-terminated file name,
	 * searchp points to the nul-terminated search string.
	 */
	for (*tagp = p; *p && !isblank(*p); ++p);
	if (*p == '\0')
		goto malformed;
	for (*p++ = '\0'; isblank(*p); ++p);
	for (*filep = p; *p && !isblank(*p); ++p);
	if (*p == '\0')
		goto malformed;
	for (*p++ = '\0'; isblank(*p); ++p);
	*searchp = p;
	if (*p == '\0') {
malformed:	free(*tagp);
		msgq(sp, M_ERR, "%s: corrupted tag in %s.", tag, (*tfp)->fname);
		return (1);
	}
	return (0);
}

#define	EQUAL		0
#define	GREATER		1
#define	LESS		(-1)

/*
 * search --
 *	Search a file for a tag.
 */
static int
search(sp, fname, tname, tag)
	SCR *sp;
	char *fname, *tname, **tag;
{
	struct stat sb;
	int fd, len;
	char *endp, *back, *front, *map, *p;

	if ((fd = open(fname, O_RDONLY, 0)) < 0)
		return (1);

	/*
	 * XXX
	 * We'd like to test if the file is too big to mmap.  Since we don't
	 * know what size or type off_t's or size_t's are, what the largest
	 * unsigned integral type is, or what random insanity the local C
	 * compiler will perpetrate, doing the comparison in a portable way
	 * is flatly impossible.  Hope that malloc fails if the file is too
	 * large.
	 */
	if (fstat(fd, &sb) || (map = mmap(NULL, (size_t)sb.st_size,
	    PROT_READ, MAP_PRIVATE, fd, (off_t)0)) == (caddr_t)-1) {
		(void)close(fd);
		return (1);
	}
	front = map;
	back = front + sb.st_size;

	front = binary_search(tname, front, back);
	front = linear_search(tname, front, back);

	if (front == NULL || (endp = strchr(front, '\n')) == NULL) {
		*tag = NULL;
		goto done;
	}

	len = endp - front;
	if ((p = malloc(len + 1)) == NULL) {
		msgq(sp, M_ERR, "Error: %s.", strerror(errno));
		*tag = NULL;
		goto done;
	}
	memmove(p, front, len);
	p[len] = '\0';
	*tag = p;

done:	if (munmap(map, (size_t)sb.st_size))
		msgq(sp, M_ERR, "tags: munmap: %s", strerror(errno));
	if (close(fd))
		msgq(sp, M_ERR, "tags: munmap: %s", strerror(errno));
	return (0);
}

/*
 * Binary search for "string" in memory between "front" and "back".
 * 
 * This routine is expected to return a pointer to the start of a line at
 * *or before* the first word matching "string".  Relaxing the constraint
 * this way simplifies the algorithm.
 * 
 * Invariants:
 * 	front points to the beginning of a line at or before the first 
 *	matching string.
 * 
 * 	back points to the beginning of a line at or after the first 
 *	matching line.
 * 
 * Base of the Invariants.
 * 	front = NULL; 
 *	back = EOF;
 * 
 * Advancing the Invariants:
 * 
 * 	p = first newline after halfway point from front to back.
 * 
 * 	If the string at "p" is not greater than the string to match, 
 *	p is the new front.  Otherwise it is the new back.
 * 
 * Termination:
 * 
 * 	The definition of the routine allows it return at any point, 
 *	since front is always at or before the line to print.
 * 
 * 	In fact, it returns when the chosen "p" equals "back".  This 
 *	implies that there exists a string is least half as long as 
 *	(back - front), which in turn implies that a linear search will 
 *	be no more expensive than the cost of simply printing a string or two.
 * 
 * 	Trying to continue with binary search at this point would be 
 *	more trouble than it's worth.
 */
#define	SKIP_PAST_NEWLINE(p, back)	while (p < back && *p++ != '\n');

static char *
binary_search(string, front, back)
	register char *string, *front, *back;
{
	register char *p;

	p = front + (back - front) / 2;
	SKIP_PAST_NEWLINE(p, back);

	while (p != back) {
		if (compare(string, p, back) == GREATER)
			front = p;
		else
			back = p;
		p = front + (back - front) / 2;
		SKIP_PAST_NEWLINE(p, back);
	}
	return (front);
}

/*
 * Find the first line that starts with string, linearly searching from front
 * to back.
 * 
 * Return NULL for no such line.
 * 
 * This routine assumes:
 * 
 * 	o front points at the first character in a line. 
 *	o front is before or at the first line to be printed.
 */
static char *
linear_search(string, front, back)
	char *string, *front, *back;
{
	while (front < back) {
		switch (compare(string, front, back)) {
		case EQUAL:		/* Found it. */
			return (front);
			break;
		case LESS:		/* No such string. */
			return (NULL);
			break;
		case GREATER:		/* Keep going. */
			break;
		}
		SKIP_PAST_NEWLINE(front, back);
	}
	return (NULL);
}

/*
 * Return LESS, GREATER, or EQUAL depending on how the string1 compares
 * with string2 (s1 ??? s2).
 * 
 * 	o Matches up to len(s1) are EQUAL. 
 *	o Matches up to len(s2) are GREATER.
 * 
 * The string "s1" is null terminated.  The string s2 is '\t', space, (or
 * "back") terminated.
 *
 * !!!
 * Reasonably modern ctags programs use tabs as separators, not spaces.
 * However, historic programs did use spaces, and, I got complaints.
 */
static int
compare(s1, s2, back)
	register char *s1, *s2, *back;
{
	for (; *s1 && s2 < back && (*s2 != '\t' && *s2 != ' '); ++s1, ++s2)
		if (*s1 != *s2)
			return (*s1 < *s2 ? LESS : GREATER);
	return (*s1 ? GREATER : s2 < back &&
	    (*s2 != '\t' && *s2 != ' ') ? LESS : EQUAL);
}
