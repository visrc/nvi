/*-
 * Copyright (c) 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: cl.h,v 8.1 1995/01/23 17:01:44 bostic Exp $ (Berkeley) $Date: 1995/01/23 17:01:44 $
 */

typedef struct _cl_private {
	char	*VB;		/* Visual bell termcap string. */

#define	CL_CURSES_INIT	0x01	/* Curses/termcap initialized. */
	u_int8_t flags;
} CL_PRIVATE;

#define	CLP(sp)		((CL_PRIVATE *)((sp)->cl_private))

/* Generic interfaces to the curses support. */
int	cl_copy __P((SCR *, SCR *));
int	cl_init __P((SCR *));

/* Curses support function prototypes. */
int	cl_addnstr __P((SCR *, char *, size_t));
int	cl_addstr __P((SCR *, char *));
int	cl_bell __P((SCR *));
int	cl_clear __P((SCR *));
int	cl_clrtoeol __P((SCR *));
int	cl_cursor __P((SCR *, size_t *, size_t *));
int	cl_deleteln __P((SCR *));
int	cl_end __P((SCR *));
int	cl_fmap __P((SCR *, enum seqtype, CHAR_T *, size_t, CHAR_T *, size_t));
int	cl_insertln __P((SCR *));
int	cl_inverse __P((SCR *, int));
int	cl_keypad __P((SCR *, int));
int	cl_move __P((SCR *, size_t, size_t));
int	cl_refresh __P((SCR *));
int	cl_restore __P((SCR *));
int	cl_ssize __P((SCR *, int));
int	cl_suspend __P((SCR *));
int	cl_term_end __P((SCR *));
int	cl_term_init __P((SCR *));
int	cl_winch __P((SCR *, CHAR_T *));
