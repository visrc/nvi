/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: key.h,v 5.21 1993/04/05 07:12:48 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:12:48 $
 */

#define	K_CARAT		1
#define	K_CNTRLD	2
#define	K_CNTRLR	3
#define	K_CNTRLZ	4
#define	K_CR		5
#define	K_ESCAPE	6
#define	K_FORMFEED	7
#define	K_NL		8
#define	K_TAB		9
#define	K_VERASE	10
#define	K_VKILL		11
#define	K_VLNEXT	12
#define	K_VWERASE	13
#define	K_ZERO		14

#define	GB_BEAUTIFY	0x001		/* Only printable characters. */
#define	GB_BS		0x002		/* Backspace erases past command. */
#define	GB_ESC		0x004		/* Escape executes command. */
#define	GB_MAPCOMMAND	0x008		/* Use the command map. */
#define	GB_MAPINPUT	0x010		/* Use the input map. */
#define	GB_NLECHO	0x020		/* Echo the newline. */
#define	GB_OFF		0x040		/* Leave first buffer char empty. */

#define	GB_VALID_EX \
	(GB_BEAUTIFY | GB_MAPCOMMAND | GB_MAPINPUT | GB_NLECHO)
#define	GB_VALID_VI \
	(GB_BEAUTIFY | GB_BS | GB_ESC | GB_MAPCOMMAND | GB_MAPINPUT | \
	    GB_NLECHO | GB_OFF)

#define	ISQ(off)	sp->gb_qb[(off)]
#define	QINIT		memset(sp->gb_qb, 0, sp->gb_len);
#define	QSET(off)	sp->gb_qb[(off)] = 1

/* Keyboard routines. */
int	ex_gb __P((struct _scr *, int, char **, size_t *, u_int));
int	gb_init __P((struct _scr *));
int	getkey __P((struct _scr *, u_int));
int	key_special __P((struct _scr *));
int	v_gb __P((struct _scr *, int, char **, size_t *, u_int));
