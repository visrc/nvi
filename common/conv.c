/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: conv.c,v 1.16 2001/05/14 15:50:23 skimo Exp $ (Berkeley) $Date: 2001/05/14 15:50:23 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#ifdef HAVE_NCURSESW
#include <ncurses.h>
#endif

#include <langinfo.h>
#include <iconv.h>
#include <locale.h>

int 
raw2int(SCR *sp, const char * str, ssize_t len, CONVWIN *cw, size_t *tolen,
	CHAR_T **dst)
{
    int i;
    CHAR_T **tostr = (CHAR_T **)&cw->bp1;
    size_t  *blen = &cw->blen1;

    BINC_RETW(NULL, *tostr, *blen, len);

    *tolen = len;
    for (i = 0; i < len; ++i)
	(*tostr)[i] = (u_char) str[i];

    *dst = cw->bp1;

    return 0;
}

#define CONV_BUFFER_SIZE    512
/* fill the buffer with codeset encoding of string pointed to by str
 * left has the number of bytes left in str and is adjusted
 * len contains the number of bytes put in the buffer
 */
#define CONVERT(str, left, src, len)				    	\
    do {								\
	size_t outleft;							\
	char *bp = buffer;						\
	outleft = CONV_BUFFER_SIZE;					\
	errno = 0;							\
	if (iconv(id, (char **)&str, &left, &bp, &outleft) == -1 &&	\
		errno != E2BIG)						\
	    goto err;							\
	len = CONV_BUFFER_SIZE - outleft;				\
	src = buffer;							\
    } while (0)

int 
default_char2int(SCR *sp, const char * str, ssize_t len, CONVWIN *cw, 
		size_t *tolen, CHAR_T **dst)
{
    int i = 0, j;
    CHAR_T **tostr = (CHAR_T **)&cw->bp1;
    size_t  *blen = &cw->blen1;
    mbstate_t mbs;
    size_t   n;
    ssize_t  nlen = len;
    char *src = (char *)str;
    iconv_t	id = (iconv_t)-1;
    char *enc = O_STR(sp, O_FILEENCODING);
    char	buffer[CONV_BUFFER_SIZE];
    size_t	left = len;

    MEMSET(&mbs, 0, 1);
    BINC_RETW(NULL, *tostr, *blen, nlen);

    if (strcmp(nl_langinfo(CODESET), enc)) {
	id = iconv_open(nl_langinfo(CODESET), enc);
	if (id == (iconv_t)-1)
	    goto err;
	CONVERT(str, left, src, len);
    }

    for (i = 0, j = 0; j < len; ) {
	n = mbrtowc((*tostr)+i, src+j, len-j, &mbs);
	/* NULL character converted */
	if (n == -1 || n == -2) goto err;
	if (n == 0) n = 1;
	j += n;
	if (++i >= *blen) {
	    nlen += 256;
	    BINC_RETW(NULL, *tostr, *blen, nlen);
	}
	if (id != (iconv_t)-1 && j == len && left) {
	    CONVERT(str, left, src, len);
	    j = 0;
	}
    }
    *tolen = i;

    if (id != (iconv_t)-1)
	iconv_close(id);

    *dst = cw->bp1;

    return 0;
err:
    *tolen = i;
    if (id != (iconv_t)-1)
	iconv_close(id);
    *dst = cw->bp1;

    return 1;
}

int 
CHAR_T_int2char(SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, 
	size_t *tolen, char **dst)
{
    *tolen = len * sizeof(CHAR_T);
    *dst = (char*) str;

    return 0;
}

int 
CHAR_T_char2int(SCR *sp, const char * str, ssize_t len, CONVWIN *cw, 
	size_t *tolen, CHAR_T **dst)
{
    *tolen = len / sizeof(CHAR_T);
    *dst = (CHAR_T*) str;

    return 0;
}

int 
int2raw(SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, size_t *tolen,
	char **dst)
{
    int i;
    char **tostr = (char **)&cw->bp1;
    size_t  *blen = &cw->blen1;

    BINC_RET(NULL, *tostr, *blen, len);

    *tolen = len;
    for (i = 0; i < len; ++i)
	(*tostr)[i] = str[i];

    *dst = cw->bp1;

    return 0;
}

int 
default_int2char(SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, 
		size_t *tolen, char **pdst)
{
    int i, j, offset = 0;
    char **tostr = (char **)&cw->bp1;
    size_t  *blen = &cw->blen1;
    mbstate_t mbs;
    size_t n;
    ssize_t  nlen = len + MB_CUR_MAX;
    char *dst;
    size_t buflen;
    char	buffer[CONV_BUFFER_SIZE];
    iconv_t	id = (iconv_t)-1;
    char *enc = O_STR(sp, O_FILEENCODING);

/* convert first len bytes of buffer and append it to cw->bp
 * len is adjusted => 0
 * offset contains the offset in cw->bp and is adjusted
 * cw->bp is grown as required
 */
#define CONVERT2(len, cw, offset)					\
    do {								\
	char *bp = buffer;						\
	while (len != 0) {						\
	    size_t outleft = cw->blen1 - offset;			\
	    char *obp = (char *)cw->bp1 + offset;		    	\
	    if (cw->blen1 < offset + MB_CUR_MAX) {		    	\
		nlen += 256;						\
		BINC_RET(NULL, cw->bp1, cw->blen1, nlen);		\
	    }						    		\
	    errno = 0;						    	\
	    if (iconv(id, &bp, &len, &obp, &outleft) == -1 && 	        \
		    errno != E2BIG)					\
		goto err;						\
	    offset = cw->blen1 - outleft;			        \
	}							        \
    } while (0)


    MEMSET(&mbs, 0, 1);
    BINC_RET(NULL, *tostr, *blen, nlen);
    dst = *tostr; buflen = *blen;

    if (strcmp(nl_langinfo(CODESET), enc)) {
	id = iconv_open(enc, nl_langinfo(CODESET));
	if (id == (iconv_t)-1)
	    goto err;
	dst = buffer; buflen = CONV_BUFFER_SIZE;
    }

    for (i = 0, j = 0; i < len; ++i) {
	n = wcrtomb(dst+j, str[i], &mbs);
	if (n == -1) goto err;
	j += n;
	if (buflen < j + MB_CUR_MAX) {
	    if (id != (iconv_t)-1) {
		CONVERT2(j, cw, offset);
	    } else {
		nlen += 256;
		BINC_RET(NULL, *tostr, *blen, nlen);
		dst = *tostr; buflen = *blen;
	    }
	}
    }

    n = wcrtomb(dst+j, L'\0', &mbs);
    j += n - 1;				/* don't count NUL at the end */
    *tolen = j;

    if (id != (iconv_t)-1) {
	CONVERT2(j, cw, offset);
	*tolen = offset;
    }

    *pdst = cw->bp1;

    return 0;
err:
    *tolen = j;

    *pdst = cw->bp1;

    return 1;
}

#ifdef USE_WIDECHAR
int 
default_int2disp (SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, 
		size_t *tolen, char **dst)
{
    int i, j;
    chtype *dest;
    char **tostr = (char **)&cw->bp1;
    size_t  *blen = &cw->blen1;

    BINC_RET(NULL, *tostr, *blen, len * sizeof(chtype));

    dest = *tostr;

    for (i = 0, j = 0; i < len; ++i)
	if (str[i] > 0xffff) {
	    dest[j++] = 0xfffd;
	} else
	    dest[j++] = str[i];
    *tolen = j;

    *dst = cw->bp1;

    return 0;
}

#else

int 
default_int2disp (SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, 
		size_t *tolen, char **dst)
{
    int i, j;
    char **tostr = (char **)&cw->bp1;
    size_t  *blen = &cw->blen1;

    BINC_RET(NULL, *tostr, *blen, len * 2);

    for (i = 0, j = 0; i < len; ++i)
	if (CHAR_WIDTH(NULL, str[i]) > 1) {
	    (*tostr)[j++] = '[';
	    (*tostr)[j++] = ']';
	} else
	    (*tostr)[j++] = str[i];
    *tolen = j;

    *dst = cw->bp1;

    return 0;
}
#endif


void
conv_init (SCR *orig, SCR *sp)
{
    if (orig != NULL)
	MEMCPY(&sp->conv, &orig->conv, 1);
    else {
	setlocale(LC_ALL, "");
	sp->conv.char2int = raw2int;
	sp->conv.int2char = int2raw;
	sp->conv.file2int = default_char2int;
	sp->conv.int2file = default_int2char;
	sp->conv.int2disp = default_int2disp;
	o_set(sp, O_FILEENCODING, OS_STRDUP, nl_langinfo(CODESET), 0);
    }
}

int
conv_enc (SCR *sp, char *enc)
{
    iconv_t id;

    if (!*enc) {
	sp->conv.file2int = raw2int;
	sp->conv.int2file = int2raw;
	return 0;
    }

    if (!strcmp(enc, "WCHAR_T")) {
	sp->conv.file2int = CHAR_T_char2int;
	sp->conv.int2file = CHAR_T_int2char;
	return 0;
    }

    id = iconv_open(enc, nl_langinfo(CODESET));
    if (id == (iconv_t)-1)
	goto err;
    iconv_close(id);
    id = iconv_open(nl_langinfo(CODESET), enc);
    if (id == (iconv_t)-1)
	goto err;
    iconv_close(id);

    sp->conv.file2int = default_char2int;
    sp->conv.int2file = default_int2char;

    F_CLR(sp, SC_CONV_ERROR);
    F_SET(sp, SC_SCR_REFORMAT);

    return 0;
err:
    msgq(sp, M_ERR,
	"321|File encoding conversion not supported");
    return 1;
}

