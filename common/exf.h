/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: exf.h,v 8.31 1994/06/29 18:15:10 bostic Exp $ (Berkeley) $Date: 1994/06/29 18:15:10 $
 */
					/* Undo direction. */
/*
 * exf --
 *	The file structure.
 */
struct _exf {
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
	enum direction lundo;		/* Last undo direction. */

	LIST_HEAD(_markh, _lmark) marks;/* Linked list of file MARK's. */

	time_t	 mtime;			/* Last modification time. */

	/*
	 * Recovery in general, and these fields specifically,
	 * are described in recover.c.
	 */
#define	RCV_PERIOD	120		/* Sync every two minutes. */
	char	*rcv_path;		/* Recover file name. */
	char	*rcv_mpath;		/* Recover mail file name. */
	int	 rcv_fd;		/* Locked mail file descriptor. */
	struct timeval rcv_tod;		/* ITIMER_REAL: recovery time-of-day. */

#define	F_FIRSTMODIFY	0x001		/* File not yet modified. */
#define	F_MODIFIED	0x002		/* File is currently dirty. */
#define	F_MULTILOCK	0x004		/* Multiple processes running, lock. */
#define	F_NOLOG		0x008		/* Logging turned off. */
#define	F_RCV_NORM	0x010		/* Don't delete recovery files. */
#define	F_RCV_ON	0x020		/* Recovery is possible. */
#define	F_UNDO		0x040		/* No change since last undo. */
	u_int	 flags;
};

/* Flags to file_write(). */
#define	FS_ALL		0x01		/* Write the entire file. */
#define	FS_APPEND	0x02		/* Append to the file. */
#define	FS_FORCE	0x04		/* Force is set. */
#define	FS_POSSIBLE	0x08		/* Force could be set. */
#define	FS_WILLEXIT	0x10		/* Command will exit on success. */

#define	GETLINE_ERR(sp, lno) {						\
	msgq((sp), M_ERR,						\
	    "Error: %s/%d: unable to retrieve line %u",			\
	    tail(__FILE__), __LINE__, (lno));				\
}

/* EXF routines. */
FREF	*file_add __P((SCR *, CHAR_T *));
int	 file_end __P((SCR *, EXF *, int));
int	 file_init __P((SCR *, FREF *, char *, int));
int	 file_write __P((SCR *, EXF *, MARK *, MARK *, char *, int));

/* Recovery routines. */
int	 rcv_init __P((SCR *, EXF *));
int	 rcv_list __P((SCR *));
int	 rcv_on __P((SCR *, EXF *));
int	 rcv_read __P((SCR *, FREF *));
#define	RCV_EMAIL	0x01		/* Send the user email. */
#define	RCV_ENDSESSION	0x02		/* End the file session. */
#define	RCV_PRESERVE	0x04		/* Preserve the backup file. */
#define	RCV_SNAPSHOT	0x08		/* Snapshot the recovery. */
int	 rcv_sync __P((SCR *, EXF *, u_int));
int	 rcv_tmp __P((SCR *, EXF *, char *));

/* DB interface routines */
int	 file_aline __P((SCR *, EXF *, int, recno_t, char *, size_t));
int	 file_dline __P((SCR *, EXF *, recno_t));
char	*file_gline __P((SCR *, EXF *, recno_t, size_t *));
int	 file_iline __P((SCR *, EXF *, recno_t, char *, size_t));
int	 file_lline __P((SCR *, EXF *, recno_t *));
char	*file_rline __P((SCR *, EXF *, recno_t, size_t *));
int	 file_sline __P((SCR *, EXF *, recno_t, char *, size_t));
