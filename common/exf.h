/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: exf.h,v 5.37 1993/02/25 20:34:40 bostic Exp $ (Berkeley) $Date: 1993/02/25 20:34:40 $
 */

#ifndef _EXF_H_
#define	_EXF_H_

typedef struct exf {
	struct exf *next, *prev;	/* Next/prev file. */
	struct exf *enext, *eprev;	/* Next/prev edit session. */

	void *scrp;			/* Screen. */
	MSG *msgp;			/* Linked list of messages. */

	/*
	 * s_confirm:	confirm an action, yes or no.
	 * s_end:	end the file session.
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

	enum direction searchdir;	/* Search direction. */
	regex_t sre;			/* Saved RE. */

	recno_t	rptlines;		/* Count of lines modified. */
	char *rptlabel;			/* How lines modified. */

	char *name;			/* File name. */
	char *tname;			/* Temporary file name. */
	size_t nlen;			/* File name length. */

	u_char *tmp_bp;			/* Temporary buffer. */
	size_t tmp_blen;		/* Size of temporary buffer. */

	MARK getc_m;			/* Getc mark. */
	u_char *getc_bp;		/* Getc buffer. */
	size_t getc_blen;		/* Getc buffer length. */

#define	F_EXIT		0x000001	/* Exiting (forced). */
#define	F_EXIT_FORCE	0x000002	/* Exiting (not forced). */
#define	F_MODE_EX	0x000004	/* In ex mode. */
#define	F_MODE_VI	0x000008	/* In vi mode. */
#define	F_SWITCH	0x000010	/* Switch files (forced). */
#define	F_SWITCH_FORCE	0x000020	/* Switch files (not forced). */

#define	F_FILE_RESET	(F_EXIT | F_EXIT_FORCE | F_SWITCH | F_SWITCH_FORCE)

#define	F_AUTOPRINT	0x000100	/* Autoprint flag. */
#define	F_BELLSCHED	0x000200	/* Bell scheduled. */
#define	F_DUMMY		0x000400	/* Faked exf structure. */
#define	F_IGNORE	0x000800	/* File not on the command line. */
#define	F_IN_GLOBAL	0x001000	/* Doing a global command. */
#define	F_MODIFIED	0x002000	/* File has been modified. */
#define	F_MSGWAIT	0x004000	/* Hold messages for awhile. */
#define	F_NAMECHANGED	0x008000	/* File name was changed. */
#define	F_NEEDMERASE	0x010000	/* Erase modeline after keystroke. */
#define	F_NEWSESSION	0x020000	/* If a new edit session. */
#define	F_NONAME	0x040000	/* File has no name. */
#define	F_RDONLY	0x080000	/* File is read-only. */
#define	F_RE_SET	0x100000	/* The file's RE has been set. */
#define	F_TMP_INUSE	0x200000	/* Temporary buffer in use. */
#define	F_UNDO		0x400000	/* No change since last undo. */

					/* Retained over edit sessions. */
#define	F_RETAINMASK	(F_IGNORE | F_MODE_EX | F_MODE_VI)
					/* Copied during file switch. */
#define	F_COPYMASK	(F_BELLSCHED | F_MODE_EX | F_MODE_VI)

#define	FF_SET(ep, f)	(ep)->flags |= (f)
#define	FF_CLR(ep, f)	(ep)->flags &= ~(f)
#define	FF_ISSET(ep, f)	((ep)->flags & (f))
	u_int flags;
} EXF;

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
	if (FF_ISSET(ep, F_MODIFIED) && ISSET(O_AUTOWRITE) &&		\
	    file_sync(ep, 0))						\
		return (1);						\
}

#define	MODIFY_CHECK(ep, force) {					\
	if (FF_ISSET(ep, F_MODIFIED))					\
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
	if (FF_ISSET(ep, F_MODIFIED) && ISSET(O_WARN))			\
		msg(ep, M_ERROR, "Modified since last write.");		\
}
#endif /* !_EXF_H_ */
