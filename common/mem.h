/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	$Id: mem.h,v 10.12 2002/01/01 11:28:18 skimo Exp $ (Berkeley) $Date: 2002/01/01 11:28:18 $
 */

/* Increase the size of a malloc'd buffer.  Two versions, one that
 * returns, one that jumps to an error label.
 */
#define	BINC_GOTO(sp, lp, llen, nlen) {					\
	char *L__p = lp;						\
	void *L__bincp;							\
	if ((nlen) > llen) {						\
		if ((L__bincp = binc(sp, L__p, &(llen), nlen)) == NULL)	\
			goto alloc_err;					\
		/*							\
		 * !!!							\
		 * Possible pointer conversion.				\
		 */							\
		lp = L__bincp;						\
	}								\
}
#define	BINC_GOTOW(sp, lp, llen, nlen) {				\
	CHAR_T *L__bp = lp;						\
	BINC_GOTO(sp, (char *)L__bp, llen, (nlen) * sizeof(CHAR_T))    	\
}
#define	BINC_RET(sp, lp, llen, nlen) {					\
	char *L__p = lp;						\
	void *L__bincp;							\
	if ((nlen) > llen) {						\
		if ((L__bincp = binc(sp, L__p, &(llen), nlen)) == NULL)	\
			return (1);					\
		/*							\
		 * !!!							\
		 * Possible pointer conversion.				\
		 */							\
		lp = L__bincp;						\
	}								\
}
#define	BINC_RETW(sp, lp, llen, nlen) {					\
	CHAR_T *L__bp = lp;						\
	BINC_RET(sp, (char *)L__bp, llen, (nlen) * sizeof(CHAR_T))    	\
}

/*
 * Get some temporary space, preferably from the global temporary buffer,
 * from a malloc'd buffer otherwise.  Two versions, one that returns, one
 * that jumps to an error label.
 */
#define	GET_SPACE_GOTO(sp, bp, blen, nlen) {				\
	WIN *L__wp = (sp) == NULL ? NULL : (sp)->wp;			\
	if (L__wp == NULL || F_ISSET(L__wp, W_TMP_INUSE)) {		\
		bp = NULL;						\
		blen = 0;						\
		BINC_GOTO(sp, bp, blen, nlen); 				\
	} else {							\
		BINC_GOTO(sp, L__wp->tmp_bp, L__wp->tmp_blen, nlen);	\
		bp = L__wp->tmp_bp;					\
		blen = L__wp->tmp_blen;					\
		F_SET(L__wp, W_TMP_INUSE);				\
	}								\
}
#define	GET_SPACE_GOTOW(sp, bp, blen, nlen) {				\
	CHAR_T *L__bp = bp;						\
	GET_SPACE_GOTO(sp, (char *)L__bp, blen, (nlen) * sizeof(CHAR_T))\
}
#define	GET_SPACE_RET(sp, bp, blen, nlen) {				\
	WIN *L__wp = (sp) == NULL ? NULL : (sp)->wp;			\
	if (L__wp == NULL || F_ISSET(L__wp, W_TMP_INUSE)) {		\
		bp = NULL;						\
		blen = 0;						\
		BINC_RET(sp, bp, blen, nlen);				\
	} else {							\
		BINC_RET(sp, L__wp->tmp_bp, L__wp->tmp_blen, nlen);	\
		bp = L__wp->tmp_bp;					\
		blen = L__wp->tmp_blen;					\
		F_SET(L__wp, W_TMP_INUSE);				\
	}								\
}
#define	GET_SPACE_RETW(sp, bp, blen, nlen) {				\
	CHAR_T *L__bp = bp;						\
	GET_SPACE_RET(sp, (char *)L__bp, blen, (nlen) * sizeof(CHAR_T))	\
}

/*
 * Add space to a GET_SPACE returned buffer.  Two versions, one that
 * returns, one that jumps to an error label.
 */
#define	ADD_SPACE_GOTO(sp, bp, blen, nlen) {				\
	WIN *L__wp = (sp) == NULL ? NULL : (sp)->wp;			\
	if (L__wp == NULL || bp == L__wp->tmp_bp) {			\
		F_CLR(L__wp, W_TMP_INUSE);				\
		BINC_GOTO(sp, L__wp->tmp_bp, L__wp->tmp_blen, nlen);	\
		bp = L__wp->tmp_bp;					\
		blen = L__wp->tmp_blen;					\
		F_SET(L__wp, W_TMP_INUSE);				\
	} else								\
		BINC_GOTO(sp, bp, blen, nlen);				\
}
#define	ADD_SPACE_GOTOW(sp, bp, blen, nlen) {				\
	CHAR_T *L__bp = bp;						\
	ADD_SPACE_GOTO(sp, (char *)L__bp, blen, (nlen) * sizeof(CHAR_T))\
}
#define	ADD_SPACE_RET(sp, bp, blen, nlen) {				\
	WIN *L__wp = (sp) == NULL ? NULL : (sp)->wp;			\
	if (L__wp == NULL || bp == L__wp->tmp_bp) {			\
		F_CLR(L__wp, W_TMP_INUSE);				\
		BINC_RET(sp, L__wp->tmp_bp, L__wp->tmp_blen, nlen);	\
		bp = L__wp->tmp_bp;					\
		blen = L__wp->tmp_blen;					\
		F_SET(L__wp, W_TMP_INUSE);				\
	} else								\
		BINC_RET(sp, bp, blen, nlen);				\
}
#define	ADD_SPACE_RETW(sp, bp, blen, nlen) {				\
	CHAR_T *L__bp = bp;						\
	ADD_SPACE_RET(sp, (char *)L__bp, blen, (nlen) * sizeof(CHAR_T))	\
}

/* Free a GET_SPACE returned buffer. */
#define	FREE_SPACE(sp, bp, blen) {					\
	WIN *L__wp = (sp) == NULL ? NULL : (sp)->wp;			\
	if (L__wp != NULL && bp == L__wp->tmp_bp)			\
		F_CLR(L__wp, W_TMP_INUSE);				\
	else								\
		free(bp);						\
}
#define	FREE_SPACEW(sp, bp, blen) {					\
	CHAR_T *L__bp = bp;						\
	FREE_SPACE(sp, (char *)L__bp, blen);				\
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
#define	CALLOC(sp, p, cast, nmemb, size) {				\
	if ((p = (cast)calloc(nmemb, size)) == NULL)			\
		msgq(sp, M_SYSERR, NULL);				\
}
#define	CALLOC_GOTO(sp, p, cast, nmemb, size) {				\
	if ((p = (cast)calloc(nmemb, size)) == NULL)			\
		goto alloc_err;						\
}
#define	CALLOC_NOMSG(sp, p, cast, nmemb, size) {			\
	p = (cast)calloc(nmemb, size);					\
}
#define	CALLOC_RET(sp, p, cast, nmemb, size) {				\
	if ((p = (cast)calloc(nmemb, size)) == NULL) {			\
		msgq(sp, M_SYSERR, NULL);				\
		return (1);						\
	}								\
}

#define	MALLOC(sp, p, cast, size) {					\
	if ((p = (cast)malloc(size)) == NULL)				\
		msgq(sp, M_SYSERR, NULL);				\
}
#define	MALLOC_GOTO(sp, p, cast, size) {				\
	if ((p = (cast)malloc(size)) == NULL)				\
		goto alloc_err;						\
}
#define	MALLOC_NOMSG(sp, p, cast, size) {				\
	p = (cast)malloc(size);						\
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
