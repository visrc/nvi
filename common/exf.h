/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: exf.h,v 5.34 1993/02/21 19:07:52 bostic Exp $ (Berkeley) $Date: 1993/02/21 19:07:52 $
 */

#ifndef _EXF_H_
#define	_EXF_H_

typedef struct exf {
	struct exf *next, *prev;	/* Linked list of files. */

					/* Screen state. */
	recno_t top;			/* 1-N:     Physical screen top line. */
	recno_t otop;			/* 1-N: Old physical screen top line. */
	recno_t lno;			/* 1-N:     Physical cursor line. */
	recno_t olno;			/* 0-N: Old physical cursor line. */
	size_t cno;			/* 0-N:     Physical cursor col. */
	size_t ocno;			/* 0-N: Old physical cursor col. */
	size_t scno;			/* 0-N: Logical screen cursor col. */
	size_t shift;			/* 0-N: Shift offset in cols. */
	size_t lines;			/* Physical number of lines. */
	size_t cols;			/* Physical number of cols. */
	void *h_smap;			/* Map of lno's to the screen (head). */
	void *t_smap;			/* Map of lno's to the screen (tail). */

	size_t rcm;			/* 0-N: Column suck. */
#define	RCM_FNB		0x01		/* Column suck: first non-blank. */
#define	RCM_LAST	0x02		/* Column suck: last. */
	u_char rcmflags;

	MSG *msgp;			/* Linked list of messages. */

	/*
	 * s_confirm:	confirm an action, yes or no.
	 * s_end:	end the session.
	 */
	enum confirmation
		(*s_confirm)	__P((struct exf *, MARK *, MARK *));
	int	(*s_end)	__P((struct exf *));

					/* Underlying database state. */
	DB *db;				/* File db structure. */
	u_char *c_lp;			/* Cached line. */
	size_t c_len;			/* Cached line length. */
	recno_t c_lno;			/* Cached line number. */
	recno_t c_nlines;		/* Lines in the file. */

	DB *log;			/* Log db structure. */
	u_char *l_lp;			/* Log buffer. */
	size_t l_len;			/* Log buffer length. */

	FILE *stdfp;			/* Ex/vi output function. */

	regex_t sre;			/* Current RE. */

	recno_t	rptlines;		/* Count of lines modified. */
	char *rptlabel;			/* How lines modified. */

	char *name;			/* File name. */
	char *tname;			/* Temporary file name. */
	size_t nlen;			/* File name length. */

#define	F_AUTOPRINT	0x00001		/* Autoprint flag. */
#define	F_BELLSCHED	0x00002		/* Bell scheduled. */
#define	F_CHARDELETED	0x00004		/* Character deleted. */
#define	F_DUMMY		0x00008		/* Character deleted. */
#define	F_IGNORE	0x00010		/* File not on the command line. */
#define	F_IN_GLOBAL	0x00020		/* Doing a global command. */
#define	F_MODIFIED	0x00040		/* File has been modified. */
#define	F_MSGWAIT	0x00080		/* Hold messages for awhile. */
#define	F_NAMECHANGED	0x00100		/* File name was changed. */
#define	F_NEEDMERASE	0x00200		/* Erase modeline after keystroke. */
#define	F_NEWSESSION	0x00400		/* File has just been edited. */
#define	F_NONAME	0x00800		/* File has no name. */
#define	F_RDONLY	0x01000		/* File is read-only. */
#define	F_READING	0x02000		/* Waiting on a read. */
#define	F_REDRAW	0x04000		/* Repaint the screen. */
#define	F_REFRESH	0x08000		/* Refresh the screen. */
#define	F_RESIZE	0x10000		/* Resize the screen. */
#define	F_RE_SET	0x20000		/* The file's RE has been set. */
#define	F_UNDO		0x40000		/* No change since last undo. */

#define	F_RETAINMASK	(F_IGNORE)	/* Flags to retain. */

#define	FF_SET(ep, f)	(ep)->flags |= (f)
#define	FF_CLR(ep, f)	(ep)->flags &= ~(f)
#define	FF_ISSET(ep, f)	((ep)->flags & (f))
	u_int flags;
} EXF;

extern EXF *curf;			/* Current file. */

typedef struct {
	EXF *next, *prev;
} EXFLIST;

/* Remove from the file chain. */
#define	rmexf(p) {							\
        (p)->prev->next = (p)->next;					\
        (p)->next->prev = (p)->prev;					\
}

/* Insert into the file chain after p. */
#define insexf(p, hp) {							\
        (p)->next = (hp)->next;						\
        (p)->prev = (EXF *)(hp);					\
        (hp)->next->prev = (p);						\
        (hp)->next = (p);						\
}

/* Insert into the file chain before p. */
#define instailexf(p, hp) {						\
	(hp)->prev->next = (p);						\
	(p)->prev = (hp)->prev;						\
	(hp)->prev = (p);						\
	(p)->next = (EXF *)(hp);					\
}

#define	GETLINE_ERR(ep, lno) {						\
	msg(ep, M_ERROR,						\
	    "Error: %s/%d: unable to retrieve line %u.",		\
	    tail(__FILE__), __LINE__, lno);				\
}

#define	AUTOWRITE(ep) {							\
	if ((ep)->flags & F_MODIFIED && ISSET(O_AUTOWRITE) &&		\
	    file_sync(ep, 0))						\
		return (1);						\
}

#define	MODIFY_CHECK(ep, force) {					\
	if ((ep)->flags & F_MODIFIED)					\
		if (ISSET(O_AUTOWRITE)) {				\
			if (file_sync((ep), (force)))			\
				return (1);				\
		} else if (!(force)) {					\
			msg(ep, M_ERROR,				\
	"Modified since last write; write or use ! to override.");	\
			return (1);					\
		}							\
}

#define	MODIFY_WARN(ep) {						\
	if ((ep)->flags & F_MODIFIED && ISSET(O_WARN))			\
		msg(ep, M_ERROR, "Modified since last write.");		\
}

#define	PANIC {								\
	msg(NULL, M_ERROR, "No file state!");				\
	mode = MODE_QUIT;						\
	return (1);							\
}
#endif /* !_EXF_H_ */
