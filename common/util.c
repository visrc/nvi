/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1991, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: util.c,v 10.18 2000/07/20 19:21:53 skimo Exp $ (Berkeley) $Date: 2000/07/20 19:21:53 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

/*
 * binc --
 *	Increase the size of a buffer.
 *
 * PUBLIC: void *binc __P((SCR *, void *, size_t *, size_t));
 */
void *
binc(sp, bp, bsizep, min)
	SCR *sp;			/* sp MAY BE NULL!!! */
	void *bp;
	size_t *bsizep, min;
{
	size_t csize;

	/* If already larger than the minimum, just return. */
	if (min && *bsizep >= min)
		return (bp);

	csize = *bsizep + MAX(min, 256);
	REALLOC(sp, bp, void *, csize);

	if (bp == NULL) {
		/*
		 * Theoretically, realloc is supposed to leave any already
		 * held memory alone if it can't get more.  Don't trust it.
		 */
		*bsizep = 0;
		return (NULL);
	}
	/*
	 * Memory is guaranteed to be zero-filled, various parts of
	 * nvi depend on this.
	 */
	memset((char *)bp + *bsizep, 0, csize - *bsizep);
	*bsizep = csize;
	return (bp);
}

/*
 * nonblank --
 *	Set the column number of the first non-blank character
 *	including or after the starting column.  On error, set
 *	the column to 0, it's safest.
 *
 * PUBLIC: int nonblank __P((SCR *, db_recno_t, size_t *));
 */
int
nonblank(sp, lno, cnop)
	SCR *sp;
	db_recno_t lno;
	size_t *cnop;
{
	CHAR_T *p;
	size_t cnt, len, off;
	int isempty;

	/* Default. */
	off = *cnop;
	*cnop = 0;

	/* Get the line, succeeding in an empty file. */
	if (db_eget(sp, lno, &p, &len, &isempty))
		return (!isempty);

	/* Set the offset. */
	if (len == 0 || off >= len)
		return (0);

	for (cnt = off, p = &p[off],
	    len -= off; len && isblank(*p); ++cnt, ++p, --len);

	/* Set the return. */
	*cnop = len ? cnt : cnt - 1;
	return (0);
}

/*
 * tail --
 *	Return tail of a path.
 *
 * PUBLIC: char *tail __P((char *));
 */
char *
tail(path)
	char *path;
{
	char *p;

	if ((p = strrchr(path, '/')) == NULL)
		return (path);
	return (p + 1);
}

/*
 * v_strdup --
 *	Strdup for wide character strings with an associated length.
 *
 * PUBLIC: char *v_strdup __P((SCR *, const char *, size_t));
 */
char *
v_strdup(sp, str, len)
	SCR *sp;
	const char *str;
	size_t len;
{
	char *copy;

	MALLOC(sp, copy, char *, (len + 1));
	if (copy == NULL)
		return (NULL);
	memcpy(copy, str, len);
	copy[len] = '\0';
	return (copy);
}

/*
 * v_strdup --
 *	Strdup for wide character strings with an associated length.
 *
 * PUBLIC: CHAR_T *v_wstrdup __P((SCR *, const CHAR_T *, size_t));
 */
CHAR_T *
v_wstrdup(sp, str, len)
	SCR *sp;
	const CHAR_T *str;
	size_t len;
{
	CHAR_T *copy;

	MALLOC(sp, copy, CHAR_T *, (len + 1) * sizeof(CHAR_T));
	if (copy == NULL)
		return (NULL);
	MEMCPYW(copy, str, len);
	copy[len] = '\0';
	return (copy);
}

/*
 * PUBLIC: size_t v_strlen __P((const CHAR_T *str));
 */
size_t
v_strlen(const CHAR_T *str)
{
	size_t len = -1;

	while(str[++len]);
	return len;
}

/*
 * PUBLIC: void * v_charset __P((CHAR_T *s, CHAR_T c, size_t n));
 */
void *
v_charset(CHAR_T *s, CHAR_T c, size_t n)
{
	CHAR_T *ss = s;

	while (n--) *s++ = c;
	return ss;
}

/*
 * PUBLIC: int v_strcmp __P((const CHAR_T *s1, const CHAR_T *s2))
 */
int 
v_strcmp(const CHAR_T *s1, const CHAR_T *s2)
{
	while (*s1 && *s2 && *s1 == *s2) s1++, s2++;
	return *s1 - *s2;
}

/*
 * nget_uslong --
 *      Get an unsigned long, checking for overflow.
 *
 * PUBLIC: enum nresult nget_uslong __P((SCR *, u_long *, const CHAR_T *, CHAR_T **, int));
 */
enum nresult
nget_uslong(sp, valp, p, endp, base)
	SCR *sp;
	u_long *valp;
	const CHAR_T *p;
	CHAR_T **endp;
	int base;
{
	CONST char *np;
	char *endnp;
	size_t nlen;

	INT2CHAR(sp, p, v_strlen(p) + 1, np, nlen);
	errno = 0;
	*valp = strtoul(np, &endnp, base);
	*endp = (CHAR_T*)p + (endnp - np);
	if (errno == 0)
		return (NUM_OK);
	if (errno == ERANGE && *valp == ULONG_MAX)
		return (NUM_OVER);
	return (NUM_ERR);
}

/*
 * nget_slong --
 *      Convert a signed long, checking for overflow and underflow.
 *
 * PUBLIC: enum nresult nget_slong __P((SCR *, long *, const CHAR_T *, CHAR_T **, int));
 */
enum nresult
nget_slong(sp, valp, p, endp, base)
	SCR *sp;
	long *valp;
	const CHAR_T *p;
	CHAR_T **endp;
	int base;
{
	CONST char *np;
	char *endnp;
	size_t nlen;

	INT2CHAR(sp, p, v_strlen(p) + 1, np, nlen);
	errno = 0;
	*valp = strtol(np, &endnp, base);
	*endp = (CHAR_T*)p + (endnp - np);
	if (errno == 0)
		return (NUM_OK);
	if (errno == ERANGE) {
		if (*valp == LONG_MAX)
			return (NUM_OVER);
		if (*valp == LONG_MIN)
			return (NUM_UNDER);
	}
	return (NUM_ERR);
}
