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
static const char sccsid[] = "$Id: conv.c,v 1.10 2001/04/25 23:42:44 skimo Exp $ (Berkeley) $Date: 2001/04/25 23:42:44 $";
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

int 
ascii2int(SCR *sp, const char * str, ssize_t len, CONVWIN *cw, size_t *tolen)
{
    int i;
    CHAR_T **tostr = (CHAR_T **)&cw->bp1;
    size_t  *blen = &cw->blen1;

    BINC_RETW(NULL, *tostr, *blen, len);

    *tolen = len;
    for (i = 0; i < len; ++i)
	(*tostr)[i] = (u_char) str[i];

    return 0;
}

int 
default_char2int(SCR *sp, const char * str, ssize_t len, CONVWIN *cw, size_t *tolen)
{
    int i, j;
    CHAR_T **tostr = (CHAR_T **)&cw->bp1;
    size_t  *blen = &cw->blen1;
    mbstate_t mbs;
    size_t   n;
    ssize_t  nlen = len;

    MEMSET(&mbs, 0, 1);
    BINC_RETW(NULL, *tostr, *blen, nlen);

    for (i = 0, j = 0; j < len; ) {
	n = mbrtowc((*tostr)+i, str+j, len-j, &mbs);
	/* NULL character converted */
	if (n == -1 || n == -2) goto err;
	if (n == 0) n = 1;
	j += n;
	if (++i >= *blen) {
	    nlen += 256;
	    BINC_RETW(NULL, *tostr, *blen, nlen);
	}
    }
    *tolen = i;

    return 0;
err:
    *tolen = i;
    return 1;
}

int 
int2ascii(SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, size_t *tolen)
{
    int i;
    char **tostr = (char **)&cw->bp1;
    size_t  *blen = &cw->blen1;

    BINC_RET(NULL, *tostr, *blen, len);

    *tolen = len;
    for (i = 0; i < len; ++i)
	(*tostr)[i] = str[i];

    return 0;
}

int 
default_int2char(SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, size_t *tolen)
{
    int i, j;
    char **tostr = (char **)&cw->bp1;
    size_t  *blen = &cw->blen1;
    mbstate_t mbs;
    size_t n;
    ssize_t  nlen = len + MB_CUR_MAX;

    MEMSET(&mbs, 0, 1);
    BINC_RET(NULL, *tostr, *blen, nlen);

    for (i = 0, j = 0; i < len; ++i) {
	n = wcrtomb((*tostr)+j, str[i], &mbs);
	if (n == -1) goto err;
	j += n;
	if (*blen < j + MB_CUR_MAX) {
	    nlen += 256;
	    BINC_RET(NULL, *tostr, *blen, nlen);
	}
    }
    n = wcrtomb((*tostr)+j, L'\0', &mbs);
    j += n - 1;				/* don't count NUL at the end */
    *tolen = j;

    return 0;
err:
    *tolen = j;
    return 1;
}

//#ifdef HAVE_NCURSESW
#ifdef HAVE_ADDNWSTR
int 
default_int2disp (SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, size_t *tolen)
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

    return 0;
}

#else

int 
default_int2disp (SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, size_t *tolen)
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

    return 0;
}
#endif

int 
gb2int (SCR *sp, const char * str, ssize_t len, CONVWIN *cw, size_t *tolen)
{
    int i, j;
    CHAR_T **tostr = (CHAR_T **)&cw->bp1;
    size_t  *blen = &cw->blen1;

    BINC_RETW(NULL, *tostr, *blen, len);

    for (i = 0, j = 0; i < len; ++i) {
	if (str[i] & 0x80) {
	    if (i+1 < len && str[i+1] & 0x80) {
		(*tostr)[j++] = INT9494(F_GB,str[i]&0x7F,str[i+1]&0x7F);
		++i;
	    } else {
		(*tostr)[j++] = INTILL(str[i]);
	    }
	} else
	    (*tostr)[j++] = str[i];
    }
    *tolen = j;

    return 0;
}

int
int2gb(SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, size_t *tolen)
{
    int i, j;
    char **tostr = (char **)&cw->bp1;
    size_t  *blen = &cw->blen1;

    BINC_RET(NULL, *tostr, *blen, len * 2);

    for (i = 0, j = 0; i < len; ++i) {
	if (INTIS9494(str[i])) {
	    (*tostr)[j++] = INT9494R(str[i]) | 0x80;
	    (*tostr)[j++] = INT9494C(str[i]) | 0x80;
	} else {
	    (*tostr)[j++] = str[i] & 0xFF;
	}
    }
    *tolen = j;

    return 0;
}

int 
utf82int (SCR *sp, const char * str, ssize_t len, CONVWIN *cw, size_t *tolen)
{
    int i, j;
    CHAR_T c;
    CHAR_T **tostr = (CHAR_T **)&cw->bp1;
    size_t  *blen = &cw->blen1;

    BINC_RETW(NULL, *tostr, *blen, len);

    for (i = 0, j = 0; i < len; ++i) {
	if (str[i] & 0x80) {
	    if ((str[i] & 0xe0) == 0xc0 && i+1 < len && str[i+1] & 0x80) {
		c = (str[i] & 0x1f) << 6;
		c |= (str[i+1] & 0x3f);
		(*tostr)[j++] = c;
		++i;
	    } else if ((str[i] & 0xf0) == 0xe0 && i+2 < len && 
			str[i+1] & 0x80 && str[i+2] & 0x80) {
		c = (str[i] & 0xf) << 12;
		c |= (str[i+1] & 0x3f) << 6;
		c |= (str[i+2] & 0x3f);
		(*tostr)[j++] = c;
		i += 2;
	    } else {
		(*tostr)[j++] = INTILL(str[i]);
	    }
	} else
	    (*tostr)[j++] = str[i];
    }
    *tolen = j;

    return 0;
}

int
int2utf8(SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, size_t *tolen)
{
    char **tostr = (char **)&cw->bp1;
    size_t  *blen = &cw->blen1;
    BINC_RET(NULL, *tostr, *blen, len * 3);

    *tolen = ucs2utf8(str, len, *tostr);

    return 0;
}


CONV default_conv = { ascii2int, int2ascii, 
		      default_char2int, default_int2char, default_int2disp };
CONV gb_conv = { default_char2int, default_int2char, 
		      gb2int, int2gb, default_int2disp };
CONV utf8_conv = { default_char2int, default_int2char, 
		      utf82int, int2utf8, default_int2disp };

void
conv_init (SCR *orig, SCR *sp)
{
    if (orig != NULL)
	sp->conv = orig->conv;
    else {
	setlocale(LC_ALL, "");
	sp->conv = &default_conv;
	o_set(sp, O_FILEENCODING, OS_STRDUP, nl_langinfo(CODESET), 0);
    }
}

int
conv_enc (SCR *sp, char *enc)
{
    if (!*enc) {
	sp->conv = &default_conv;
	return 0;
    }
    if (!strcmp(enc,"GB")) {
	sp->conv = &gb_conv;
	return 0;
    }
    if (!strcmp(enc,"UTF-8")) {
	sp->conv = &utf8_conv;
	return 0;
    }
    return 1;
}

