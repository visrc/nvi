/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: gs.h,v 8.1 1993/06/09 22:21:01 bostic Exp $ (Berkeley) $Date: 1993/06/09 22:21:01 $
 */

#include <termios.h>

struct _scr;
typedef struct _gs {
	struct _hdr	 exfhdr;	/* Linked list of EXF structures. */
	struct _hdr	 scrhdr;	/* Linked list of SCR structures. */

	mode_t	 origmode;		/* Original terminal mode. */
	struct termios
		 original_termios;	/* Original terminal values. */

	struct _msg	*msgp;		/* User message list. */

	char	*tmp_bp;		/* Temporary buffer. */
	size_t	 tmp_blen;		/* Size of temporary buffer. */

#ifdef DEBUG
	FILE	*tracefp;		/* Trace file pointer. */
#endif

#define	G_SETMODE	0x01		/* Tty mode changed. */
#define	G_SNAPSHOT	0x02		/* Always snapshot files. */
#define	G_TMP_INUSE	0x04		/* Temporary buffer in use. */
#define	G_RECOVER_SET	0x08		/* Recover system initialized. */
	u_int	 flags;
} GS;

extern GS *__global_list;		/* List of screens. */
