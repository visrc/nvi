/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: gs.h,v 8.11 1993/11/02 11:04:01 bostic Exp $ (Berkeley) $Date: 1993/11/02 11:04:01 $
 */

struct _scr;
typedef struct _gs {
	struct list_entry screens;	/* Linked list of SCR structures. */
	
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

#define	G_ISFROMTTY	0x0001		/* Reading from a tty. */
#define	G_RECOVER_SET	0x0002		/* Recover system initialized. */
#define	G_SETMODE	0x0004		/* Tty mode changed. */
#define	G_SIGALRM	0x0008		/* SIGALRM arrived. */
#define	G_SIGHUP	0x0010		/* SIGHUP arrived. */
#define	G_SIGTERM	0x0020		/* SIGTERM arrived. */
#define	G_SIGWINCH	0x0040		/* SIGWINCH arrived. */
#define	G_SLEEPING	0x0080		/* Asleep (die on signal). */
#define	G_SNAPSHOT	0x0100		/* Always snapshot files. */
#define	G_TMP_INUSE	0x0200		/* Temporary buffer in use. */
	u_int	 flags;
} GS;

extern GS *__global_list;		/* List of screens. */
