/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: exf.h,v 5.49 1993/05/05 10:40:59 bostic Exp $ (Berkeley) $Date: 1993/05/05 10:40:59 $
 */

					/* Undo direction. */
enum udirection { UBACKWARD, UFORWARD };

/*
 * exf --
 *	The file structure.
 */
typedef struct _exf {
	struct _exf *next, *prev;	/* Linked list of files. */

	char	*name;			/* File name. */
	char	*tname;			/* Temporary file name. */
	size_t	 nlen;			/* File name length. */
	u_char	 refcnt;		/* Reference count. */

	recno_t	 lno;			/* Last/starting physical line. */
	size_t	 cno;			/* Last/starting physical column. */

					/* Underlying database state. */
	DB	*db;			/* File db structure. */
	char	*c_lp;			/* Cached line. */
	size_t	 c_len;			/* Cached line length. */
	recno_t	 c_lno;			/* Cached line number. */
	recno_t	 c_nlines;		/* Cached lines in the file. */

	DB	*log;			/* Log db structure. */
	char	*l_lp;			/* Log buffer. */
	size_t	 l_len;			/* Log buffer length. */
	u_char	 l_ltype;		/* Log type. */
	recno_t	 l_srecno;		/* Saved record number. */
	enum udirection lundo;		/* Last undo direction. */

	struct _mark	getc_m;		/* Getc mark. */
	char	*getc_bp;		/* Getc buffer. */
	size_t	 getc_blen;		/* Getc buffer length. */

					/* File marks. */
	struct _mark	marks[UCHAR_MAX + 1];

	char	*icommand;		/* Initial command. */

#define	F_ICOMMAND	0x0001		/* Initial command set. */
#define	F_IGNORE	0x0002		/* File to be ignored. */
#define	F_MODIFIED	0x0004		/* File has been modified. */
#define	F_NAMECHANGED	0x0008		/* File name was changed. */
#define	F_NOLOG		0x0010		/* Logging turned off. */
#define	F_NONAME	0x0020		/* File has no name. */
#define	F_NOSETPOS	0x0040		/* No line position. */
#define	F_RDONLY	0x0080		/* File is read-only. */
#define	F_UNDO		0x0100		/* No change since last undo. */

#define	F_CLOSECLR			/* Flags to clear on close. */	\
	(F_MODIFIED | F_NAMECHANGED | F_NOLOG | F_RDONLY | F_UNDO)
	u_int	 flags;
} EXF;

/* Flags to file_write(). */
#define	FS_ALL		0x01		/* Write the entire file. */
#define	FS_APPEND	0x02		/* Append to the file. */
#define	FS_FORCE	0x04		/* Force is set. */
#define	FS_POSSIBLE	0x08		/* Force could be set. */

#define	AUTOWRITE(sp, ep) {						\
	if (F_ISSET((ep), F_MODIFIED) && O_ISSET((sp), O_AUTOWRITE) &&	\
	    file_write((sp), (ep), NULL, NULL, NULL, FS_ALL))		\
		return (1);						\
}

#define	GETLINE_ERR(sp, lno) {						\
	msgq((sp), M_ERR,						\
	    "Error: %s/%d: unable to retrieve line %u.",		\
	    tail(__FILE__), __LINE__, (lno));				\
}

#define	MODIFY_CHECK(sp, ep, force) {					\
	if (F_ISSET((ep), F_MODIFIED))					\
		if (O_ISSET((sp), O_AUTOWRITE)) {			\
			if (file_write((sp), (ep), NULL, NULL, NULL,	\
			    FS_ALL | force?FS_FORCE:0 | FS_POSSIBLE))	\
				return (1);				\
		} else if (ep->refcnt <= 1 && !(force)) {		\
			msgq(sp, M_ERR,					\
	"Modified since last write; write or use ! to override.");	\
			return (1);					\
		}							\
}

#define	MODIFY_WARN(sp, ep) {						\
	if (F_ISSET(ep, F_MODIFIED) && O_ISSET(sp, O_WARN))		\
		msgq(sp, M_ERR, "Modified since last write.");		\
}

/* File routines. */
int	 file_aline __P((struct _scr *,
	    struct _exf *, recno_t, char *, size_t));
int	 file_dline __P((struct _scr *, struct _exf *, recno_t));
EXF	*file_first __P((struct _scr *, int));
EXF	*file_get __P((struct _scr *, struct _exf *, char *, int));
char	*file_gline __P((struct _scr *, struct _exf *, recno_t, size_t *));
int	 file_iline __P((struct _scr *,
	    struct _exf *, recno_t, char *, size_t));
recno_t	 file_lline __P((struct _scr *, struct _exf *));
EXF	*file_next __P((struct _scr *, struct _exf *, int));
EXF	*file_prev __P((struct _scr *, struct _exf *, int));
int	 file_set __P((struct _scr *, int, char *[]));
int	 file_sline __P((struct _scr *,
	    struct _exf *, recno_t, char *, size_t));
EXF	*file_start __P((struct _scr *, struct _exf *));
int	 file_stop __P((struct _scr *, struct _exf *, int));
int	 file_write __P((struct _scr *, struct _exf *,
	    struct _mark *, struct _mark *, char *, int));
