/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: msg.h,v 5.1 1993/02/19 11:18:23 bostic Exp $ (Berkeley) $Date: 1993/02/19 11:18:23 $
 */

extern int msgcnt;		/* Current message count. */
extern char *msglist[];		/* Message list. */

#define	M_BELL		0x01	/* Schedule a bell event. */
#define	M_DISPLAY	0x02	/* Always display. */

#define	M_ERROR	(M_BELL | M_DISPLAY)

void	bell __P((EXF *));
void	bell_sched __P((EXF *));
void	msg __P((EXF *, u_int, const char *, ...));
void	msg_eflush __P((EXF *));
