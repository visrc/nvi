#include "config.h"

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

#include "multibyte.h"

/*
 * PUBLIC: void * v_strset __P((CHAR_T *s, CHAR_T c, size_t n));
 */
void *
v_strset(CHAR_T *s, CHAR_T c, size_t n)
{
	CHAR_T *ss = s;

	while (n--) *s++ = c;
	return ss;
}

