/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: key.h,v 5.3 1992/04/28 13:48:52 bostic Exp $ (Berkeley) $Date: 1992/04/28 13:48:52 $
 */

#define	ESCAPE		'\033'		/* Escape character. */

#define	GB_BS		0x01		/* Backspace erases past command. */
#define	GB_ESC		0x02		/* Escape executes command. */
#define	GB_NL		0x04		/* Leave the newline on. */
#define	GB_NLECHO	0x08		/* Echo the newline. */
#define	GB_OFF		0x10		/* Leave first buffer char empty. */

#define	ISQ(off)	qb[(off)]
#define	QINIT		bzero(qb, cblen);
#define	QSET(off)	qb[(off)] = 1

extern u_long cblen;			/* Buffer lengths. */
extern char *qb;			/* Quote buffer. */

extern u_char *atkeybuf;		/* Base of shared at buffer. */
extern u_char *atkeyp;			/* Next char of shared at buffer. */
extern u_long atkeybuflen;		/* Remaining keys in shared @ buffer. */

char	*gb __P((int, u_int));
int	 gb_inc __P((void));
void	 gb_init __P((void));
