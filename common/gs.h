/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: gs.h,v 8.7 1993/10/27 15:53:25 bostic Exp $ (Berkeley) $Date: 1993/10/27 15:53:25 $
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

/* INFORMATION SHARED BY ALL SCREENS. */
	struct _ibuf	*key;		/* Key input buffer. */
	struct _ibuf	*tty;		/* Tty input buffer. */

	struct _cb	*cuts;		/* Cut buffers. */

#define	G_ISFROMTTY	0x001		/* Reading from a tty. */
#define	G_SETMODE	0x002		/* Tty mode changed. */
#define	G_SNAPSHOT	0x004		/* Always snapshot files. */
#define	G_TMP_INUSE	0x008		/* Temporary buffer in use. */
#define	G_RECOVER_SET	0x010		/* Recover system initialized. */
#define	G_SIGALRM	0x020		/* SIGALRM arrived. */
#define	G_SIGHUP	0x040		/* SIGHUP arrived. */
#define	G_SIGTERM	0x080		/* SIGTERM arrived. */
#define	G_SIGWINCH	0x100		/* SIGWINCH arrived. */
	u_int	 flags;
} GS;

extern GS *__global_list;		/* List of screens. */
