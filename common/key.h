/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: key.h,v 5.17 1993/02/21 19:44:10 bostic Exp $ (Berkeley) $Date: 1993/02/21 19:44:10 $
 */

#define	K_CARAT		1
#define	K_CNTRLD	2
#define	K_CR		3
#define	K_ESCAPE	4
#define	K_FORMFEED	5
#define	K_NL		6
#define	K_TAB		7
#define	K_VERASE	8
#define	K_VKILL		9
#define	K_VLNEXT	10
#define	K_VWERASE	11
#define	K_ZERO		12
extern u_char special[];		/* Special characters. */

#define	GB_BEAUTIFY	0x001		/* Only printable characters. */
#define	GB_BS		0x002		/* Backspace erases past command. */
#define	GB_ESC		0x004		/* Escape executes command. */
#define	GB_MAPCOMMAND	0x008		/* Use the command map. */
#define	GB_MAPINPUT	0x010		/* Use the input map. */
#define	GB_NL		0x020		/* Leave the newline on. */
#define	GB_NLECHO	0x040		/* Echo the newline. */
#define	GB_OFF		0x080		/* Leave first buffer char empty. */

#define	ISQ(off)	gb_qb[(off)]
#define	QINIT		memset(gb_qb, 0, gb_blen);
#define	QSET(off)	gb_qb[(off)] = 1

extern u_char *atkeybuf;		/* Base of shared at buffer. */
extern u_char *atkeyp;			/* Next char of shared at buffer. */
extern u_long atkeybuflen;		/* Remaining keys in shared @ buffer. */

/*
 * The routines that fill a buffer from the terminal share these three data
 * structures.  They are a buffer to hold the return value, a quote buffer
 * so we know which characters are quoted, and a widths buffer.  The last is
 * used internally to hold the widths of each character.  Any new routines
 * will need to support these too.
 */
extern u_char *gb_cb;			/* Return buffer. */
extern u_char *gb_qb;			/* Quote buffer. */
extern u_char *gb_wb;			/* Widths buffer. */
extern u_long gb_blen;			/* Buffer lengths. */

					/* Real routines. */
int	ex_gb __P((EXF *, int, u_char **, size_t *, u_int));
int	v_gb __P((EXF *, int, u_char **, size_t *, u_int));

int	gb_inc __P((EXF *));		/* Support routines. */
void	gb_init __P((EXF *));
int	getkey __P((EXF *, u_int));
