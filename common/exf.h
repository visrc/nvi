/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: exf.h,v 8.14 1993/11/01 11:33:39 bostic Exp $ (Berkeley) $Date: 1993/11/01 11:33:39 $
 */
					/* Undo direction. */
enum udirection { UBACKWARD, UFORWARD };

/*
 * exf --
 *	The file structure.
 */
typedef struct _exf {
	int	 refcnt;		/* Reference count. */

					/* Underlying database state. */
	DB	*db;			/* File db structure. */
	char	*c_lp;			/* Cached line. */
	size_t	 c_len;			/* Cached line length. */
	recno_t	 c_lno;			/* Cached line number. */
	recno_t	 c_nlines;		/* Cached lines in the file. */

	DB	*log;			/* Log db structure. */
	char	*l_lp;			/* Log buffer. */
	size_t	 l_len;			/* Log buffer length. */
	recno_t	 l_high;		/* Log last + 1 record number. */
	recno_t	 l_cur;			/* Log current record number. */
	MARK	 l_cursor;		/* Log cursor position. */
	enum udirection lundo;		/* Last undo direction. */

	struct list_entry marks;	/* Linked list of file MARK's. */

	/*
	 * If F_RCV_NORM is not set, rcv_path and rcv_mpath are unlinked
	 * on file exit.  If rcv_path or rcv_mpath are not NULL, they are
	 * free'd as well.  (This means that, on error, F_RCV_NORM will
	 * normally be set.)
	 */
	char	*rcv_path;		/* Recover file name. */
	char	*rcv_mpath;		/* Recover mail file name. */

#define	F_FIRSTMODIFY	0x001		/* File not yet modified. */
#define	F_MODIFIED	0x002		/* File is currently dirty. */
#define	F_MULTILOCK	0x004		/* Multiple processes running, lock. */
#define	F_NOLOG		0x008		/* Logging turned off. */
#define	F_RCV_ALRM	0x010		/* File should be synced. */
#define	F_RCV_NORM	0x020		/* Don't remove the recovery file. */
#define	F_RCV_ON	0x040		/* File is recoverable. */
#define	F_UNDO		0x080		/* No change since last undo. */
	u_int	 flags;
} EXF;

/* Flags to file_write(). */
#define	FS_ALL		0x01		/* Write the entire file. */
#define	FS_APPEND	0x02		/* Append to the file. */
#define	FS_FORCE	0x04		/* Force is set. */
#define	FS_POSSIBLE	0x08		/* Force could be set. */

#define	GETLINE_ERR(sp, lno) {						\
	msgq((sp), M_ERR,						\
	    "Error: %s/%d: unable to retrieve line %u.",		\
	    tail(__FILE__), __LINE__, (lno));				\
}

/* FREF routines. */
FREF	*file_add __P((SCR *, FREF *, char *, int));
FREF	*file_first __P((SCR *, int));
FREF	*file_next __P((SCR *, int));

/* EXF routines. */
int	 file_end __P((SCR *, EXF *, int));
int	 file_init __P((SCR *, FREF *, char *, int));
int	 file_write __P((SCR *, EXF *, MARK *, MARK *, char *, int));

/* DB interface routines */
int	 file_aline __P((SCR *, EXF *, int, recno_t, char *, size_t));
int	 file_dline __P((SCR *, EXF *, recno_t));
char	*file_gline __P((SCR *, EXF *, recno_t, size_t *));
int	 file_iline __P((SCR *, EXF *, recno_t, char *, size_t));
int	 file_lline __P((SCR *, EXF *, recno_t *));
char	*file_rline __P((SCR *, EXF *, recno_t, size_t *));
int	 file_sline __P((SCR *, EXF *, recno_t, char *, size_t));
