/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: gs.h,v 8.4 1993/09/29 16:42:08 bostic Exp $ (Berkeley) $Date: 1993/09/29 16:42:08 $
 */

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

#define	G_ISFROMTTY	0x01		/* Reading from a tty. */
#define	G_SETMODE	0x02		/* Tty mode changed. */
#define	G_SNAPSHOT	0x04		/* Always snapshot files. */
#define	G_TMP_INUSE	0x08		/* Temporary buffer in use. */
#define	G_RECOVER_SET	0x10		/* Recover system initialized. */
#define	G_SIGWINCH	0x20		/* Window size change received. */
	u_int	 flags;
} GS;

extern GS *__global_list;		/* List of screens. */
