/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: cl.h,v 8.6 1995/04/13 17:19:47 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:19:47 $
 */

typedef struct _cl_private {
	char	*VB;		/* Visual bell termcap string. */

	int	 eof_count;	/* EOF count. */

	size_t	 linecount;	/* Output overwrite count. */
	size_t	 lcontinue;	/* Output line continue value. */
	size_t	 totalcount;	/* Output overwrite count. */

	int	 busy_state;	/* Busy state. */
	struct timeval
		 busy_tv;	/* Busy timer. */
	size_t	 busy_fx;	/* Busy character x coordinate. */
	size_t	 busy_y, busy_x;/* Busy saved screen coordinates. */

	struct termios exterm;	/* Terminal values ex restores. */
	char	*CE;		/* Clear to EOL termcap string. */
	char	*CM;		/* Cursor movement termcap string. */
	char	*UP;		/* Cursor up termcap string. */
	char	*SE, *SO;	/* Inverse video termcap strings. */

				/* Event queue. */
	TAILQ_HEAD(_eventh, _event) eventq;

#define	INDX_CONT	0	/* Array offsets. */
#define	INDX_HUP	1
#define	INDX_INT	2
#define	INDX_TERM	3
#define	INDX_WINCH	4
#define	INDX_MAX	5	/* Original signal information. */
	struct sigaction oact[INDX_MAX];

#define	CL_BUSY		0x0001	/* Busy message is on. */
#define	CL_DIVIDER	0x0002	/* Screen divider is displayed. */
#define	CL_EX_INIT	0x0004	/* Ex curses/termcap initialized. */
#define	CL_INTERRUPTQ	0x0008	/* Interrupt event queued up. */
#define	CL_IVIDEO	0x0010	/* Display in inverse video. */
#define	CL_SIGCONT	0x0020	/* SIGCONT arrived. */
#define	CL_SIGHUP	0x0040	/* SIGHUP arrived. */
#define	CL_SIGINT	0x0080	/* SIGINT arrived. */
#define	CL_SIGTERM	0x0100	/* SIGTERM arrived. */
#define	CL_SIGWINCH	0x0200	/* SIGWINCH arrived. */
#define	CL_VI_INIT	0x0400	/* Vi curses/termcap initialized. */
	u_int16_t flags;
} CL_PRIVATE;

extern GS *__global_list;	/* GLOBAL: List of screens. */

#define	CLP(sp)		((CL_PRIVATE *)((sp)->gp->cl_private))
#define	G_CLP		((CL_PRIVATE *)__global_list->cl_private)

typedef enum { INP_OK=0, INP_EOF, INP_ERR, INP_INTR, INP_TIMEOUT } input_t;
input_t cl_read __P((SCR *, CHAR_T *, size_t, int *, struct timeval *));

/* The line relative to a specific window. */
#define	RLNO(sp, lno)	(sp)->woff + (lno)

/*
 * search.c:f_search() is called from ex/ex_tag.c:ex_tagfirst(), which
 * runs before the screen really exists, and tries to access the screen.
 * Make sure we don't step on anything.
 *
 * If the terminal isn't initialized, there's nothing to do.
 */
#define	VI_INIT								\
	if (CLP(sp) == NULL || !F_ISSET(CLP(sp), CL_VI_INIT))		\
		return (0);
/* A similar check when running in ex mode. */
#define	EX_INIT								\
	if (CLP(sp) == NULL || !F_ISSET(CLP(sp), CL_EX_INIT))		\
		return (0);
/* Some functions should never be called in ex mode. */
#define	EX_ABORT							\
	if (F_ISSET(sp, S_EX))						\
		abort();
/* Some functions are noop's in ex mode. */
#define	EX_NOOP								\
	if (F_ISSET(sp, S_EX))						\
		return (0);
/* Some functions should never be called in vi mode. */
#define	VI_ABORT							\
	if (F_ISSET(sp, S_VI))						\
		abort();

int	cl_addnstr __P((SCR *, const char *, size_t));
int	cl_addstr __P((SCR *, const char *));
int	cl_bell __P((SCR *));
int	cl_busy __P((SCR *, const char *, int));
void	cl_busy_update __P((SCR *));
int	cl_canon __P((SCR *, int));
int	cl_clear __P((SCR *));
int	cl_clrtoeol __P((SCR *));
int	cl_clrtoeos __P((SCR *));
conf_t	cl_confirm __P((SCR *));
int	cl_continue __P((SCR *));
int	cl_cursor __P((SCR *, size_t *, size_t *));
int	cl_deleteln __P((SCR *));
int	cl_ex_end __P((SCR *));
int	cl_ex_init __P((SCR *));
int	cl_ex_msgflush __P((SCR *, int *));
int	cl_ex_suspend __P((SCR *));
int	cl_ex_tend __P((SCR *));
int	cl_ex_tinit __P((SCR *));
int	cl_exadjust __P((SCR *, exadj_t));
int	cl_fmap __P((SCR *, seq_t, CHAR_T *, size_t, CHAR_T *, size_t));
int	cl_getkey __P((SCR *, CHAR_T *));
int	cl_insertln __P((SCR *));
int	cl_inverse __P((SCR *, int));
int	cl_keypad __P((int));
int	cl_move __P((SCR *, size_t, size_t));
int	cl_optchange __P((SCR *, int));
void	cl_putchar __P((int));
int	cl_refresh __P((SCR *));
int	cl_repaint __P((SCR *));
void	cl_sig_end __P((SCR *));
int	cl_sig_init __P((SCR *));
void	cl_sig_restore __P((SCR *));
int	cl_ssize __P((SCR *, int, recno_t *, size_t *));
int	cl_term_end __P((SCR *));
int	cl_term_init __P((SCR *));
int	cl_vi_end __P((SCR *));
int	cl_vi_init __P((SCR *));
int	cl_vi_msgflush __P((SCR *, int *));
int	cl_vi_suspend __P((SCR *));
int	cl_write __P((void *, const char *, int));
