/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: key.h,v 5.18 1993/02/28 11:54:19 bostic Exp $ (Berkeley) $Date: 1993/02/28 11:54:19 $
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
extern u_char special[];		/* Special characters. */

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
