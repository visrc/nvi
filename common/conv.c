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
static const char sccsid[] = "$Id: conv.c,v 1.8 2001/04/21 06:36:25 skimo Exp $ (Berkeley) $Date: 2001/04/21 06:36:25 $";
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

int 
default_char2int(CONV *conv, const char * str, ssize_t len, CHAR_T **tostr, size_t *tolen, size_t *blen)
{
    int i;

    BINC_RETW(NULL, *tostr, *blen, len);

    *tolen = len;
    for (i = 0; i < len; ++i)
	(*tostr)[i] = (u_char) str[i];

    return 0;
}

int 
default_int2char(CONV *conv, const CHAR_T * str, ssize_t len, char **tostr, size_t *tolen, size_t *blen)
{
    int i;

    BINC_RET(NULL, *tostr, *blen, len);

    *tolen = len;
    for (i = 0; i < len; ++i)
	(*tostr)[i] = str[i];

    return 0;
}

//#ifdef HAVE_NCURSESW
#ifdef HAVE_ADDNWSTR
int 
default_int2disp(CONV *conv, const CHAR_T * str, ssize_t len, char **tostr, size_t *tolen, size_t *blen)
{
    int i, j;
    chtype *dest;

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
default_int2disp(CONV *conv, const CHAR_T * str, ssize_t len, char **tostr, size_t *tolen, size_t *blen)
{
    int i, j;

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
gb2int(CONV *conv, const char * str, ssize_t len, CHAR_T **tostr, size_t *tolen, size_t *blen)
{
    int i, j;

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
int2gb(CONV *conv, const CHAR_T * str, ssize_t len, char **tostr, size_t *tolen, size_t *blen)
{
    int i, j;

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
utf82int(CONV *conv, const char * str, ssize_t len, CHAR_T **tostr, size_t *tolen, size_t *blen)
{
    int i, j;
    CHAR_T c;

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
int2utf8(CONV *conv, const CHAR_T * str, ssize_t len, char **tostr, size_t *tolen, size_t *blen)
{
    BINC_RET(NULL, *tostr, *blen, len * 3);

    *tolen = ucs2utf8(str, len, *tostr);

    return 0;
}


CONV default_conv = { 0, 0, default_char2int, default_int2char, 
		      default_char2int, default_int2char, default_int2disp };
CONV gb_conv = { 0, 0, default_char2int, default_int2char, 
		      gb2int, int2gb, default_int2disp };
CONV utf8_conv = { 0, 0, default_char2int, default_int2char, 
		      utf82int, int2utf8, default_int2disp };

void
conv_init (SCR *orig, SCR *sp)
{
    if (orig != NULL)
	sp->conv = orig->conv;
    else
	sp->conv = &default_conv;
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

