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
static const char sccsid[] = "$Id: conv.c,v 1.1 2000/07/18 19:18:43 skimo Exp $ (Berkeley) $Date: 2000/07/18 19:18:43 $";
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

int 
default_char2int(CONV *conv, const char * str, ssize_t len, CHAR_T **tostr, size_t *tolen)
{
    int i;

    BINC_RETW(NULL, conv->buffer, conv->size, len);
    *tostr = conv->buffer;

    *tolen = len;
    for (i = 0; i < len; ++i)
	(*tostr)[i] = (u_char) str[i];

    return 0;
}

int 
default_int2char(CONV *conv, const CHAR_T * str, ssize_t len, char **tostr, size_t *tolen)
{
    int i;

    BINC_RET(NULL, conv->buffer, conv->size, len * 2);
    *tostr = conv->buffer;

    *tolen = len;
    for (i = 0; i < len; ++i)
	(*tostr)[i] = str[i];

    return 0;
}

int 
gb2int(CONV *conv, const char * str, ssize_t len, CHAR_T **tostr, size_t *tolen)
{
    int i, j;

    BINC_RETW(NULL, conv->buffer, conv->size, len);
    *tostr = conv->buffer;

    for (i = 0, j = 0; i < len; ++i) {
	if (str[i] & 0x80) {
	    if (i+1 < len && str[i] & 0x80) {
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
int2gb(CONV *conv, const CHAR_T * str, ssize_t len, char **tostr, size_t *tolen)
{
    int i, j;

    BINC_RET(NULL, conv->buffer, conv->size, len * 2);
    *tostr = conv->buffer;

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

CONV default_conv = { 0, 0, default_char2int, default_int2char, 
		      default_char2int, default_int2char };
CONV gb_conv = { 0, 0, default_char2int, default_int2char, 
		      gb2int, int2gb };

void
conv_init (SCR *sp)
{
    sp->conv = &default_conv;
}

int
conv_enc (SCR *sp, char *enc)
{
    if (!strcmp(enc,"GB"))
	sp->conv = &gb_conv;
}
