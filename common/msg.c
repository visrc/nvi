/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: msg.c,v 10.9 1995/07/05 23:19:31 bostic Exp $ (Berkeley) $Date: 1995/07/05 23:19:31 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "vi.h"

/*
 * msgq --
 *	Display a message.
 *
 * PUBLIC: void msgq __P((SCR *, mtype_t, const char *, ...));
 */
void
#ifdef __STDC__
msgq(SCR *sp, mtype_t mt, const char *fmt, ...)
#else
msgq(sp, mt, fmt, va_alist)
	SCR *sp;
	mtype_t mt;
        char *fmt;
        va_dcl
#endif
{
#ifndef NL_ARGMAX
#define	__NL_ARGMAX	20		/* Set to 9 by System V. */
	struct {
		const char *str;	/* String pointer. */
		size_t	 arg;		/* Argument number. */
		size_t	 prefix;	/* Prefix string length. */
		size_t	 skip;		/* Skipped string length. */
		size_t	 suffix;	/* Suffix string length. */
	} str[__NL_ARGMAX];
#endif
	static int reenter;		/* STATIC: Re-entrancy check. */
	CHAR_T ch;
	GS *gp;
	size_t blen, cnt1, cnt2, len, mlen, nlen, soff;
	const char *p, *t, *u;
	char *bp, *mp, *rbp, *s_rbp;
        va_list ap;

#ifdef __STDC__
        va_start(ap, fmt);
#else
        va_start(ap);
#endif
	/*
	 * !!!
	 * It's possible to enter msg when there's no screen to hold the
	 * message.  If sp is NULL, ignore the special cases and put the
	 * message out to stderr.
	 */
	if (sp == NULL) {
		gp = NULL;
		if (mt == M_BERR)
			mt = M_ERR;
		else if (mt == M_VINFO)
			mt = M_INFO;
	} else {
		gp = sp->gp;
		switch (mt) {
		case M_BERR:
			if (F_ISSET(sp, S_VI) && !O_ISSET(sp, O_VERBOSE)) {
				F_SET(gp, G_BELLSCHED);
				return;
			}
			mt = M_ERR;
			break;
		case M_VINFO:
			if (!O_ISSET(sp, O_VERBOSE))
				return;
			mt = M_INFO;
			/* FALLTHROUGH */
		case M_INFO:
			if (F_ISSET(sp, S_EX_SILENT))
				return;
			break;
		case M_ERR:
		case M_SYSERR:
			break;
		default:
			abort();
		}
	}

	/*
	 * It's possible to reenter msg when it allocates space.  We're
	 * probably dead anyway, but there's no reason to drop core.
	 *
	 * We can be entered as the result of a signal arriving, trying
	 * to sync the file and failing.  This shouldn't be a hot spot,
	 * block the signals.
	 *
	 * XXX
	 * Yes, there's a race, but it should only be two instructions.
	 */
	if (gp != NULL)
		SIGBLOCK(gp);
	if (reenter++) {
		if (gp != NULL)
			SIGUNBLOCK(gp);
		return;
	}
	if (gp != NULL)
		SIGUNBLOCK(gp);

	/* Get space for the message. */
	nlen = 1024;
	if (0) {
retry:		FREE_SPACE(sp, bp, blen);
		nlen *= 2;
	}
	bp = NULL;
	blen = 0;
	GET_SPACE_GOTO(sp, bp, blen, nlen);

	/*
	 * Error prefix.
	 *
	 * mp:	 pointer to the current next character to be written
	 * mlen: length of the already written characters
	 * blen: total length of the buffer
	 */
#define	REM	(blen - mlen)
	mp = bp;
	mlen = 0;
	if (mt == M_SYSERR) {
		p = msg_cat(sp, "020|Error: ", &len);
		if (REM < len)
			goto retry;
		memmove(mp, p, len);
		mp += len;
		mlen += len;
	}

	/* File name, line number prefix for errors. */
	if (sp != NULL &&
	    sp->if_name != NULL && (mt == M_ERR || mt == M_SYSERR)) {
		for (p = sp->if_name; *p != '\0'; ++p) {
			len = snprintf(mp, REM, "%s", KEY_NAME(sp, *p));
			mp += len;
			if ((mlen += len) > blen)
				goto retry;
		}
		len = snprintf(mp, REM, ", %d: ", sp->if_lno);
		mp += len;
		if ((mlen += len) > blen)
			goto retry;
	}

	/* If nothing to format, we're done. */
	if (fmt == NULL)
		goto nofmt;
	fmt = msg_cat(sp, fmt, NULL);

#ifndef NL_ARGMAX
	/*
	 * Nvi should run on machines that don't support the numbered argument
	 * specifications (%[digit]*$).  We do this by reformatting the string
	 * so that we can hand it to vsprintf(3) and it will use the arguments
	 * in the right order.  When vsprintf returns, we put the string back
	 * into the right order.  It's undefined, according to SVID III, to mix
	 * numbered argument specifications with the standard style arguments,
	 * so this should be safe.
	 *
	 * In addition, we also need a character that is known to not occur in
	 * any vi message, for separating the parts of the string.  As callers
	 * of msgq are responsible for making sure that all the non-printable
	 * characters are formatted for printing before calling msgq, we use a
	 * random non-printable character selected at terminal initialization
	 * time.  This code isn't fast by any means, but as messages should be
	 * relatively short and normally have only a few arguments, it won't be
	 * too bad.  Regardless, nobody has come up with any other solution.
	 *
	 * The result of this loop is an array of pointers into the message
	 * string, with associated lengths and argument numbers.  The array
	 * is in the "correct" order, and the arg field contains the argument
	 * order.
	 */
	for (p = fmt, soff = 0; soff < __NL_ARGMAX;) {
		for (t = p; *p != '\0' && *p != '%'; ++p);
		if (*p == '\0')
			break;
		++p;
		if (!isdigit(*p)) {
			if (*p == '%')
				++p;
			continue;
		}
		for (u = p; *++p != '\0' && isdigit(*p););
		if (*p != '$')
			continue;

		/* Up to, and including the % character. */
		str[soff].str = t;
		str[soff].prefix = u - t;

		/* Up to, and including the $ character. */
		str[soff].arg = atoi(u);
		str[soff].skip = (p - u) + 1;
		if (str[soff].arg >= __NL_ARGMAX)
			goto ret;

		/* Up to, and including the conversion character. */
		for (u = p; (ch = *++p) != '\0';)
			if (isalpha(ch) &&
			    strchr("diouxXfeEgGcspn", ch) != NULL)
				break;
		str[soff].suffix = p - u;
		if (ch != '\0')
			++p;
		++soff;
	}

	/* If no magic strings, we're done. */
	if (soff == 0)
		goto format;

	 /* Get space for the reordered strings. */
	if ((rbp = malloc(nlen)) == NULL)
		goto ret;
	s_rbp = rbp;

	/*
	 * Reorder the strings into the message string based on argument
	 * order.
	 *
	 * !!!
	 * We ignore arguments that are out of order, i.e. if we don't find
	 * an argument, we continue.  Assume (almost certainly incorrectly)
	 * that whoever created the string knew what they were doing.
	 *
	 * !!!
	 * Brute force "sort", but since we don't expect more than one or two
	 * arguments in a string, the setup cost of a fast sort will be more
	 * expensive than the loop.
	 */
	for (cnt1 = 1; cnt1 <= soff; ++cnt1)
		for (cnt2 = 0; cnt2 < soff; ++cnt2)
			if (cnt1 == str[cnt2].arg) {
				memmove(s_rbp, str[cnt2].str, str[cnt2].prefix);
				memmove(s_rbp + str[cnt2].prefix,
				    str[cnt2].str + str[cnt2].prefix +
				    str[cnt2].skip, str[cnt2].suffix);
				s_rbp += str[cnt2].prefix + str[cnt2].suffix;
				*s_rbp++ =
				    gp == NULL ? DEFAULT_NOPRINT : gp->noprint;
				break;
			}
	*s_rbp = '\0';
	fmt = rbp;
#endif

	/* Format the arguments into the string. */
format:	len = vsnprintf(mp, REM, fmt, ap);
	if (len >= nlen)
		goto retry;

#ifndef NL_ARGMAX
	if (soff == 0)
		goto nofmt;

	/*
	 * Go through the resulting string, and, for each separator character
	 * separated string, enter its new starting position and length in the
	 * array.
	 */
	for (p = t = mp, cnt1 = 1,
	    ch = gp == NULL ? DEFAULT_NOPRINT : gp->noprint; *p != '\0'; ++p)
		if (*p == ch) {
			for (cnt2 = 0; cnt2 < soff; ++cnt2)
				if (str[cnt2].arg == cnt1)
					break;
			str[cnt2].str = t;
			str[cnt2].prefix = p - t;
			t = p + 1;
			++cnt1;
		}

	/*
	 * Reorder the strings once again, putting them back into the
	 * message buffer.
	 *
	 * !!!
	 * Note, the length of the message gets decremented once for
	 * each substring, when we discard the separator character.
	 */
	for (s_rbp = rbp, cnt1 = 0; cnt1 < soff; ++cnt1) {
		memmove(rbp, str[cnt1].str, str[cnt1].prefix);
		rbp += str[cnt1].prefix;
		--len;
	}
	memmove(mp, s_rbp, rbp - s_rbp);

	/* Free the reordered string memory. */
	free(s_rbp);
#endif

nofmt:	mp += len;
	if ((mlen += len) > blen)
		goto retry;
	if (mt == M_SYSERR) {
		len = snprintf(mp, REM, ": %s", strerror(errno));
		mp += len;
		if ((mlen += len) > blen)
			goto retry;
		mt = M_ERR;
	}

	(void)ex_fflush(sp);
	(void)gp->scr_msg(sp, mt, bp, mlen);

	/* Cleanup. */
ret:	FREE_SPACE(sp, bp, blen);
binc_err:
	reenter = 0;
}

/*
 * msg_rpt --
 *	Report on the lines that changed.
 *
 * !!!
 * Historic vi documentation (USD:15-8) claimed that "The editor will also
 * always tell you when a change you make affects text which you cannot see."
 * This isn't true -- edit a large file and do "100d|1".  We don't implement
 * this semantic as it would require that we track each line that changes
 * during a command instead of just keeping count.
 *
 * Line counts weren't right in historic vi, either.  For example, given the
 * file:
 *	abc
 *	def
 * the command 2d}, from the 'b' would report that two lines were deleted,
 * not one.
 *
 * PUBLIC: int msg_rpt __P((SCR *));
 */
int
msg_rpt(sp)
	SCR *sp;
{
	static char * const action[] = {
		"added",
		"changed",
		"deleted",
		"joined",
		"moved",
		"shifted",
		"yanked",
	};
	recno_t total;
	u_long rptval;
	int first, cnt;
	size_t blen, len;
	char * const *ap;
	char *bp, *p;

	/* Change reports are turned off in batch mode. */
	if (F_ISSET(sp, S_EX_SILENT))
		return (0);

	/* Reset changing line number. */
	sp->rptlchange = OOBLNO;

	/*
	 * Don't build a message if not enough changed.
	 *
	 * !!!
	 * And now, a vi clone test.  Historically, vi reported if the number
	 * of changed lines was > than the value, not >=, unless it was a yank
	 * command, which used >=.  No lie.  I got complaints, so we conform
	 * to historic practice.  In addition, setting report to 0 in the 4BSD
	 * historic vi was equivalent to setting it to 1, for an unknown reason
	 * (this bug was apparently fixed in System V at some point).
	 */
#define	ARSIZE(a)	sizeof(a) / sizeof (*a)
#define	MAXNUM		25
	rptval = O_VAL(sp, O_REPORT);
	for (cnt = 0, total = 0; cnt < ARSIZE(action); ++cnt)
		total += sp->rptlines[cnt];
	if (total <= rptval && sp->rptlines[L_YANKED] < rptval) {
		if (total != 0)
			for (cnt = 0; cnt < ARSIZE(action); ++cnt)
				sp->rptlines[cnt] = 0;
		return (0);
	}

	/* Build and display the message. */
	GET_SPACE_RET(sp, bp, blen, sizeof(action) * MAXNUM);
	for (p = bp,
	    ap = action, cnt = 0, first = 1; cnt < ARSIZE(action); ++ap, ++cnt)
		if (sp->rptlines[cnt] != 0) {
			len = snprintf(p, MAXNUM, "%s%lu lines %s",
			    first ? "" : "; ", sp->rptlines[cnt], *ap);
			p += len;
			first = 0;
			sp->rptlines[cnt] = 0;
		}

	(void)ex_fflush(sp);
	(void)sp->gp->scr_msg(sp, M_INFO, bp, len);

	FREE_SPACE(sp, bp, blen);
	return (0);
#undef ARSIZE
#undef MAXNUM
}

/*
 * msg_status --
 *	Report on the file's status.
 *
 * PUBLIC: int msg_status __P((SCR *, recno_t, int));
 */
int
msg_status(sp, lno, showlast)
	SCR *sp;
	recno_t lno;
	int showlast;
{
	recno_t last;
	const char *t;
	char *bp, *p;
	int needsep;
	size_t blen, len;

	len = strlen(sp->frp->name);
	GET_SPACE_RET(sp, bp, blen, len + 128);
	p = bp;

	memmove(p, sp->frp->name, len);
	p += len;
	*p++ = ':';
	*p++ = ' ';

	/*
	 * See nvi/exf.c:file_init() for a description of how and when the
	 * read-only bit is set.
	 *
	 * !!!
	 * The historic display for "name changed" was "[Not edited]".
	 */
	needsep = 0;
	if (F_ISSET(sp->frp, FR_NEWFILE)) {
		F_CLR(sp->frp, FR_NEWFILE);
		t = msg_cat(sp, "021|new file", &len);
		memmove(p, t, len);
		p += len;
		needsep = 1;
	} else {
		if (F_ISSET(sp->frp, FR_NAMECHANGE)) {
			t = msg_cat(sp, "022|name changed", &len);
			memmove(p, t, len);
			p += len;
			needsep = 1;
		}
		if (needsep) {
			*p++ = ',';
			*p++ = ' ';
		}
		if (F_ISSET(sp->ep, F_MODIFIED))
			t = msg_cat(sp, "023|modified", &len);
		else
			t = msg_cat(sp, "024|unmodified", &len);
		memmove(p, t, len);
		p += len;
		needsep = 1;
	}
	if (F_ISSET(sp->frp, FR_UNLOCKED)) {
		if (needsep) {
			*p++ = ',';
			*p++ = ' ';
		}
		t = msg_cat(sp, "025|UNLOCKED", &len);
		memmove(p, t, len);
		p += len;
		needsep = 1;
	}
	if (F_ISSET(sp->frp, FR_RDONLY)) {
		if (needsep) {
			*p++ = ',';
			*p++ = ' ';
		}
		t = msg_cat(sp, "026|readonly", &len);
		memmove(p, t, len);
		p += len;
		needsep = 1;
	}
	if (needsep) {
		*p++ = ':';
		*p++ = ' ';
	}
	if (showlast) {
		if (file_lline(sp, &last))
			return (1);
		if (last > 1) {
			t = msg_cat(sp, "027|line %lu of %lu [%ld%%]", &len);
			(void)sprintf(p, t, lno, last, (lno * 100) / last);
			p += strlen(p);
		} else {
			t = msg_cat(sp, "028|empty file", &len);
			memmove(p, t, len);
			p += len;
		}
	} else {
		t = msg_cat(sp, "029|line %lu", &len);
		(void)sprintf(p, t, lno);
		p += strlen(p);
	}
#ifdef DEBUG
	(void)sprintf(p, " (pid %lu)", (u_long)getpid());
	p += strlen(p);
#endif
	(void)ex_fflush(sp);
	(void)sp->gp->scr_msg(sp, M_INFO, bp, (size_t)(p - bp));

	FREE_SPACE(sp, bp, blen);
	return (0);
}

/*
 * msg_open --
 *	Open the message catalogs.
 *
 * PUBLIC: int msg_open __P((SCR *, char *));
 */
int
msg_open(sp, file)
	SCR *sp;
	char *file;
{
	DB *db;
	DBT data, key;
	recno_t msgno;
	int nf;
	char *p, *t, buf[MAXPATHLEN];

	/*
	 * !!!
	 * Assume that the first file opened is the system default, and that
	 * all subsequent ones user defined.  Only display error messages
	 * if we can't open the user defined ones -- it's useful to know if
	 * the system one wasn't there, but if nvi is being shipped with an
	 * installed system, the file will be there, if it's not, then the
	 * message will be repeated every time nvi is started up.
	 */
	if ((p = strrchr(file, '/')) != NULL && p[1] == '\0' &&
	    ((t = getenv("LANG")) != NULL ||
	    (t = getenv("LC_MESSAGES")) != NULL)) {
		(void)snprintf(buf, sizeof(buf), "%svi_%s", file, t);
		p = buf;
	} else
		p = file;
	if ((db = dbopen(p,
	    O_NONBLOCK | O_RDONLY, 0, DB_RECNO, NULL)) == NULL) {
		if (O_STR(sp, O_MSGCAT) == NULL)
			return (1);
		p = msg_print(sp, p, &nf);
		msgq(sp, M_SYSERR, "%s", p);
		if (nf)
			FREE_SPACE(sp, p, 0);
		return (1);
	}

	/*
	 * Test record 1 for the magic string.  The msgq call
	 * is here so the message catalog build finds it.
	 */
#define	VMC	"VI_MESSAGE_CATALOG"
	key.data = &msgno;
	key.size = sizeof(recno_t);
	msgno = 1;
	if (db->get(db, &key, &data, 0) != 0 ||
	    data.size != sizeof(VMC) - 1 ||
	    memcmp(data.data, VMC, sizeof(VMC) - 1)) {
		(void)db->close(db);
		if (O_STR(sp, O_MSGCAT) == NULL)
			return (1);
		p = msg_print(sp, p, &nf);
		msgq(sp, M_ERR, "030|The file %s is not a message catalog", p);
		if (nf)
			FREE_SPACE(sp, p, 0);
		return (1);
	}

	if (sp->gp->msg != NULL)
		(void)sp->gp->msg->close(sp->gp->msg);
	sp->gp->msg = db;
	return (0);
}

/*
 * msg_close --
 *	Close the message catalogs.
 *
 * PUBLIC: void msg_close __P((GS *));
 */
void
msg_close(gp)
	GS *gp;
{
	if (gp->msg != NULL)
		(void)gp->msg->close(gp->msg);
}

/*
 * msg_cont --
 *	Return common messages.
 *
 * PUBLIC: const char *msg_cmsg __P((SCR *, cmsg_t, size_t *));
 */
const char *
msg_cmsg(sp, which, lenp)
	SCR *sp;
	cmsg_t which;
	size_t *lenp;
{
	switch (which) {
	case CMSG_CONF:
		return (msg_cat(sp, "268|confirm? [ynq]", lenp));
	case CMSG_CONT:

		return (msg_cat(sp, "269|Press any key to continue: ", lenp));
	case CMSG_CONT_S:
		return (msg_cat(sp, "279| cont?", lenp));
	case CMSG_CONT_Q:
		return (msg_cat(sp,
		    "280|Press any key to continue [q to quit]: ", lenp));
	default:
		abort();
	}
	/* NOTREACHED */
}

/*
 * msg_cat --
 *	Return a single message from the catalog, plus its length.
 *
 * !!!
 * Only a single catalog message can be accessed at a time, if multiple
 * ones are needed, they must be copied into local memory.
 *
 * PUBLIC: const char *msg_cat __P((SCR *, const char *, size_t *));
 */
const char *
msg_cat(sp, str, lenp)
	SCR *sp;
	const char *str;
	size_t *lenp;
{
	GS *gp;
	DBT data, key;
	recno_t msgno;

	/*
	 * If it's not a catalog message, i.e. has doesn't have a leading
	 * number and '|' symbol, we're done.
	 */
	if (isdigit(str[0]) &&
	    isdigit(str[1]) && isdigit(str[2]) && str[3] == '|') {
		key.data = &msgno;
		key.size = sizeof(recno_t);
		msgno = atoi(str);

		/*
		 * XXX
		 * Really sleazy hack -- we put an extra character on the
		 * end of the format string, and then we change it to be
		 * the nul termination of the string.  There ought to be
		 * a better way.  Once we can allocate multiple temporary
		 * memory buffers, maybe we can use one of them instead.
		 */
		gp = sp == NULL ? NULL : sp->gp;
		if (gp != NULL && gp->msg != NULL &&
		    gp->msg->get(gp->msg, &key, &data, 0) == 0 &&
		    data.size != 0) {
			if (lenp != NULL)
				*lenp = data.size - 1;
			((char *)data.data)[data.size - 1] = '\0';
			return (data.data);
		}
		str = &str[4];
	}
	if (lenp != NULL)
		*lenp = strlen(str);
	return (str);
}

/*
 * msg_print --
 *	Return a printable version of a string, in allocated memory.
 *
 * PUBLIC: char *msg_print __P((SCR *, const char *, int *));
 */
char *
msg_print(sp, s, needfree)
	SCR *sp;
	const char *s;
	int *needfree;
{
	size_t blen, nlen;
	const char *cp;
	char *bp, *ep, *p, *t;

	*needfree = 0;

	for (cp = s; *cp != '\0'; ++cp)
		if (!isprint(*cp))
			break;
	if (*cp == '\0')
		return ((char *)s);	/* SAFE: needfree set to 0. */

	nlen = 0;
	if (0) {
retry:		if (sp == NULL)
			free(bp);
		else
			FREE_SPACE(sp, bp, blen);
		needfree = 0;
	}
	nlen += 256;
	if (sp == NULL) {
		if ((bp = malloc(nlen)) == NULL)
			goto binc_err;
	} else
		GET_SPACE_GOTO(sp, bp, blen, nlen);
	if (0) {
binc_err:	return ("");
	}
	*needfree = 1;

	for (p = bp, ep = (bp + blen) - 1, cp = s; *cp != '\0' && p < ep; ++cp)
		for (t = KEY_NAME(sp, *cp); *t != '\0' && p < ep; *p++ = *t++);
	if (p == ep)
		goto retry;
	*p = '\0';
	return (bp);
}
