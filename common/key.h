/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: key.h,v 5.1 1992/04/18 18:33:39 bostic Exp $ (Berkeley) $Date: 1992/04/18 18:33:39 $
 */

#define	GB_BS		0x01	/* Backspace erases past command. */
#define	GB_ESC		0x02	/* Escape executes command. */
#define	GB_NL		0x04	/* Leave the newline on. */
#define	GB_OFF		0x08	/* Leave the first buffer character empty. */

#define	ISQ(off)	qb[(off)]
#define	QINIT		bzero(qb, cblen);
#define	QSET(off)	qb[(off)] = 1

extern u_long cblen;		/* Buffer lengths. */
extern u_char *qb;		/* Quote buffer. */

extern u_char *atkeybuf;	/* Base of shared at buffer. */
extern u_char *atkeyp;		/* Next char of shared at buffer. */
extern u_long atkeybuflen;	/* Remaining keys in shared at buffer. */

char	*gb __P((int, u_int));
int	 gb_inc __P((void));
void	 gb_init __P((struct termios *));
