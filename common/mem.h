/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: mem.h,v 8.6 1994/06/20 17:01:45 bostic Exp $ (Berkeley) $Date: 1994/06/20 17:01:45 $
 */

/* Increase the size of a malloc'd buffer.  Two versions, one that
 * returns, one that jumps to an error label.
 */
#define	BINC_GOTO(sp, lp, llen, nlen) {					\
	if ((nlen) > llen && binc(sp, &(lp), &(llen), nlen))		\
		goto binc_err;						\
}
#define	BINC_RET(sp, lp, llen, nlen) {					\
	if ((nlen) > llen && binc(sp, &(lp), &(llen), nlen))		\
		return (1);						\
}

/*
 * Get some temporary space, preferably from the global temporary buffer,
 * from a malloc'd buffer otherwise.  Two versions, one that returns, one
 * that jumps to an error label.
 */
#define	GET_SPACE_GOTO(sp, bp, blen, nlen) {				\
	GS *__gp = (sp)->gp;						\
	if (F_ISSET(__gp, G_TMP_INUSE)) {				\
		bp = NULL;						\
		blen = 0;						\
		BINC_GOTO(sp, bp, blen, nlen); 				\
	} else {							\
		BINC_GOTO(sp, __gp->tmp_bp, __gp->tmp_blen, nlen);	\
		bp = __gp->tmp_bp;					\
		blen = __gp->tmp_blen;					\
		F_SET(__gp, G_TMP_INUSE);				\
	}								\
}
#define	GET_SPACE_RET(sp, bp, blen, nlen) {				\
	GS *__gp = (sp)->gp;						\
	if (F_ISSET(__gp, G_TMP_INUSE)) {				\
		bp = NULL;						\
		blen = 0;						\
		BINC_RET(sp, bp, blen, nlen);				\
	} else {							\
		BINC_RET(sp, __gp->tmp_bp, __gp->tmp_blen, nlen);	\
		bp = __gp->tmp_bp;					\
		blen = __gp->tmp_blen;					\
		F_SET(__gp, G_TMP_INUSE);				\
	}								\
}

/*
 * Add space to a GET_SPACE returned buffer.  Two versions, one that
 * returns, one that jumps to an error label.
 */
#define	ADD_SPACE_GOTO(sp, bp, blen, nlen) {				\
	GS *__gp = (sp)->gp;						\
	if (bp == __gp->tmp_bp) {					\
		F_CLR(__gp, G_TMP_INUSE);				\
		BINC_GOTO(sp, __gp->tmp_bp, __gp->tmp_blen, nlen);	\
		bp = __gp->tmp_bp;					\
		blen = __gp->tmp_blen;					\
		F_SET(__gp, G_TMP_INUSE);				\
	} else								\
		BINC_GOTO(sp, bp, blen, nlen);				\
}
#define	ADD_SPACE_RET(sp, bp, blen, nlen) {				\
	GS *__gp = (sp)->gp;						\
	if (bp == __gp->tmp_bp) {					\
		F_CLR(__gp, G_TMP_INUSE);				\
		BINC_RET(sp, __gp->tmp_bp, __gp->tmp_blen, nlen);	\
		bp = __gp->tmp_bp;					\
		blen = __gp->tmp_blen;					\
		F_SET(__gp, G_TMP_INUSE);				\
	} else								\
		BINC_RET(sp, bp, blen, nlen);				\
}

/* Free memory, optionally making pointers unusable. */
#ifdef DEBUG
#define	FREE(p, sz) {							\
	memset(p, 0xff, sz);						\
	free(p);							\
}
#else
#define	FREE(p, sz)	free(p);
#endif

/* Free a GET_SPACE returned buffer. */
#define	FREE_SPACE(sp, bp, blen) {					\
	if (bp == sp->gp->tmp_bp)					\
		F_CLR(sp->gp, G_TMP_INUSE);				\
	else								\
		FREE(bp, blen);						\
}

/*
 * Malloc a buffer, casting the return pointer.  Various versions.
 *
 * !!!
 * The cast should be unnecessary, malloc(3) and friends return void *'s,
 * which is all we need.  However, some systems that nvi needs to run on
 * don't do it right yet, resulting in the compiler printing out roughly
 * a million warnings.  After awhile, it seemed easier to put the casts
 * in instead of explaining it all the time.
 */
#define	CALLOC_NOMSG(sp, p, cast, nmemb, size) {			\
	p = (cast)calloc(nmemb, size);					\
}
#define	CALLOC(sp, p, cast, nmemb, size) {				\
	if ((p = (cast)calloc(nmemb, size)) == NULL)			\
		msgq(sp, M_SYSERR, NULL);				\
}
#define	CALLOC_RET(sp, p, cast, nmemb, size) {				\
	if ((p = (cast)calloc(nmemb, size)) == NULL) {			\
		msgq(sp, M_SYSERR, NULL);				\
		return (1);						\
	}								\
}
#define	MALLOC_NOMSG(sp, p, cast, size) {				\
	p = (cast)malloc(size);						\
}
#define	MALLOC(sp, p, cast, size) {					\
	if ((p = (cast)malloc(size)) == NULL)				\
		msgq(sp, M_SYSERR, NULL);				\
}
#define	MALLOC_RET(sp, p, cast, size) {					\
	if ((p = (cast)malloc(size)) == NULL) {				\
		msgq(sp, M_SYSERR, NULL);				\
		return (1);						\
	}								\
}
/*
 * XXX
 * Don't depend on realloc(NULL, size) working.
 */
#define	REALLOC(sp, p, cast, size) {					\
	if ((p = (cast)(p == NULL ?					\
	    malloc(size) : realloc(p, size))) == NULL)			\
		msgq(sp, M_SYSERR, NULL);				\
}

/*
 * Versions of memmove(3) and memset(3) that use the size of the
 * initial pointer to figure out how much memory to manipulate.
 */
#define	MEMMOVE(p, t, len)	memmove(p, t, (len) * sizeof(*(p)))
#define	MEMSET(p, value, len)	memset(p, value, (len) * sizeof(*(p)))

int	binc __P((SCR *, void *, size_t *, size_t));
