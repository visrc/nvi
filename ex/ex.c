/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex.c,v 5.48 1992/12/05 11:08:22 bostic Exp $ (Berkeley) $Date: 1992/12/05 11:08:22 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "log.h"
#include "options.h"
#include "search.h"
#include "term.h"
#include "vcmd.h"
#include "pathnames.h"

int autoprint;
u_char *defcmdarg[2];

static int	 fileexpand __P((glob_t *, u_char *, int));
static u_char	*linespec __P((u_char *, EXCMDARG *));

#define	DEFCOM	".+1"

/*
 * ex --
 *	Read an ex command and execute it.
 */
int
ex()
{
	size_t len;
	u_char *p;
	char defcom[sizeof(DEFCOM)];

	while (mode == MODE_EX) {
		if (ex_gb(ISSET(O_PROMPT) ? ':' : 0,
		    &p, &len, GB_MAPCOMMAND) || !p)
			continue;
		if (*p)
			ex_cstring(p, len, 0);
		else {
			bcopy(DEFCOM, defcom, sizeof(DEFCOM));
			ex_cstring((u_char *)defcom, 3, 0);
		}
	}
	return (0);
}

/*
 * ex_cfile --
 *	Execute ex commands from a file.
 */
int
ex_cfile(filename, noexisterr)
	char *filename;
	int noexisterr;
{
	struct stat sb;
	int fd, len, rval;
	u_char *bp;

	if ((fd = open(filename, O_RDONLY, 0)) < 0) {
		if (noexisterr)
			goto e1;
		return (0);
	}
	if (fstat(fd, &sb))
		goto e2;

	if (sb.st_size > SIZE_T_MAX) {
		errno = E2BIG;
		goto e2;
	}

	if ((bp = malloc(sb.st_size)) == NULL)
		goto e2;

	len = read(fd, bp, (int)sb.st_size);
	if (len == -1 || len != sb.st_size) {
		if (len != sb.st_size)
			errno = EIO;
		goto e3;
	}
	bp[sb.st_size] = '\0';

	rval = ex_cstring(bp, len, 1);
	free(bp);
	(void)close(fd);
	return (rval);

e3:	free(bp);
e2:	(void)close(fd);
e1:	msg("%s: %s.", filename, strerror(errno));
	return (1);
}

/*
 * ex_cstring --
 *	Execute EX commands from a string.  The commands may be separated
 *	by newlines or by | characters, and may be quoted.
 */
int
ex_cstring(cmd, len, doquoting)
	u_char *cmd;
	register int len, doquoting;
{
	register int cnt;
	register u_char *p, *t;

	/*
	 * Walk the string, checking for '^V' quotes and '|' or '\n'
	 * separated commands.  The string "^V\n" is a single '^V'.
	 */
	if (doquoting)
		QINIT;
	for (p = t = cmd, cnt = 0;; ++cnt, ++t, --len) {
		if (len == 0)
			goto cend;
		if (doquoting && cnt == gb_blen)
			gb_inc();
		switch(*t) {
		case '|':
		case '\n':
cend:			if (p > cmd) {
				*p = '\0';	/* XXX: 8BIT */
				/*
				 * Errors are ignored, although error messages
				 * will be displayed later.
				 */
				(void)ex_cmd(cmd);
				p = cmd;
			}
			if (len == 0)
				return (0);
			if (doquoting)
				QINIT;
			break;
		case ctrl('V'):
			if (doquoting && len > 1 && t[1] != '\n') {
				QSET(cnt);
				++t;
				--len;
			}
			/* FALLTHROUGH */
		default:
			*p++ = *t;
			break;
		}
	}
	/* NOTREACHED */
}

static EXCMDARG parg = { NULL, 2 };
static EXCMDLIST *lastcmd = &cmds[C_PRINT];

/*
 * ex_cmd --
 *	Parse and execute an ex command.  
 */
int
ex_cmd(exc)
	register u_char *exc;
{
	register int cmdlen;
	register u_char *p;
	EXCMDARG cmd;
	EXCMDLIST *cp;
	recno_t lcount, num;
	long flagoff;
	int flags, uselastcmd;
	u_char *ep;

#if DEBUG && 0
	TRACE("ex: {%s}\n", exc);
#endif
	/*
	 * Permit a single extra colon at the start of the line, for
	 * historical reasons.
	 */
	if (*exc == ':')
		++exc;

	/* Ignore command lines that start with a double-quote. */
	if (*exc == '"')
		return (0);

	/* Skip whitespace. */
	for (; isspace(*exc); ++exc);

	/* Initialize the argument structure. */
	bzero(&cmd, sizeof(EXCMDARG));
	cmd.buffer = OOBCB;

	/*
	 * Parse line specifiers if the command uses addresses.  New command
	 * line position is returned, or NULL on error.  
	 */
	if ((exc = linespec(exc, &cmd)) == NULL)
		return (1);

	/* Skip whitespace. */
	for (; isspace(*exc); ++exc);

	/*
	 * If no command, ex does the last specified of p, l, or #, and vi
	 * moves to the line.  Otherwise, find out how long the command name
	 * is.  There are a few commands that aren't alphabetic, but they
	 * are all single character commands.
	 */
	if (*exc) {
		if (index("!#&<=>@", *exc)) {
			p = exc;
			exc++;
			cmdlen = 1;
		} else {
			for (p = exc; isalpha(*exc); ++exc);
			cmdlen = exc - p;
		}
		for (cp = cmds; cp->name && bcmp(p, cp->name, cmdlen); ++cp);
		if (cp->name == NULL) {
			msg("The %.*s command is unknown.", cmdlen, p);
			return (1);
		}
		uselastcmd = 0;

		/* Some commands are turned off. */
		if (cp->flags & E_NOPERM) {
			msg("The %.*s command is not currently supported.",
			    cmdlen, p);
			return (1);
		}
	} else {
		cp = lastcmd;
		uselastcmd = 1;
	}

	/*
	 * File state must be checked throughout this code, because it is
	 * called when reading the .exrc file and similar things.  There's
	 * this little chicken and egg problem -- if we read the file first,
	 * we won't know how to display it.  If we read/set the exrc stuff
	 * first, we can't allow any command that requires file state.
	 * Historic vi generally took the easy way out, by dropping core.
 	 */
	if (ep == NULL &&
	    cp->flags & (E_ADDR1|E_ADDR2|E_ADDR2_ALL|E_ADDR2_NONE)) {
		msg("The %s command requires a file to be read in.", cp->name);
		return (1);
	}

	/*
	 * Set the default addresses.  It's an error to specify an address for
	 * a command that doesn't take them.  If two addresses are specified
	 * for a command that only takes one, lose the first one.  Two special
	 * cases here, some commands take 0 or 2 addresses.  For most of them
	 * (the E_ADDR2_ALL flag), 0 defaults to the entire file.  For one
	 * (the `!' command, the E_ADDR2_NONE flag), 0 defaults to no lines.
	 */
	flagoff = 0;
	switch(cp->flags & (E_ADDR1|E_ADDR2|E_ADDR2_ALL|E_ADDR2_NONE)) {
	case E_ADDR1:				/* One address: */
		switch(cmd.addrcnt) {
		case 0:				/* Default to cursor. */
			cmd.addrcnt = 1;
			cmd.addr1.lno = curf->lno;
			cmd.addr1.cno = curf->cno;
			break;
		case 1:
			break;
		case 2:				/* Lose the first address. */
			cmd.addrcnt = 1;
			cmd.addr1 = cmd.addr2;
		}
		break;
	case E_ADDR2_NONE:			/* Zero/two addresses: */
		if (cmd.addrcnt == 0)		/* Default to nothing. */
			break;
		goto two;
	case E_ADDR2_ALL:			/* Zero/two addresses: */
		if (cmd.addrcnt == 0) {		/* Default to entire file. */
			cmd.addrcnt = 2;
			cmd.addr1.lno = 1;
			cmd.addr1.cno = 0;
			cmd.addr2.lno = file_lline(curf);
			cmd.addr2.cno = 0;
			cmd.flags |= E_ADDR2_ALL;
			break;
		}
		/* FALLTHROUGH */
	case E_ADDR2:				/* Two addresses: */
two:		switch(cmd.addrcnt) {
		case 0:				/* Default to cursor. */
			cmd.addrcnt = 2;
			cmd.addr1.lno = cmd.addr2.lno = curf->lno;
			cmd.addr1.cno = cmd.addr2.cno = curf->cno;
			break;
		case 1:				/* Default to first address. */
			cmd.addrcnt = 2;
			cmd.addr2 = cmd.addr1;
			break;
		case 2:
			break;
		}
		break;
	default:
		if (cmd.addrcnt)		/* Error. */
			goto usage;
	}
		
	/*
	 * If the rest of the string is parsed by the command itself, we
	 * don't even skip leading white-space, it's significant for some
	 * commands.  However, require that there be *something*.
	 */
	p = (u_char *)cp->syntax;
	if (*p == 's') {
		for (p = exc; *p && isspace(*p); ++p);
		cmd.string = *p ? exc : NULL;
		goto addr2;
	}
	for (lcount = 0; *p; ++p) {
		for (; isspace(*exc); ++exc);		/* Skip whitespace. */
		if (!*exc)
			break;

		switch (*p) {
		case '!':				/* ! */
			if (*exc == '!') {
				++exc;
				cmd.flags |= E_FORCE;
			}
			break;
		case '+':				/* +cmd */
			if (*exc != '+')
				break;
			for (cmd.plus = ++exc; !isspace(*exc); ++exc);
			if (*exc)
				*exc++ = '\0';
			break;
		case '1':				/* #, l, p */
			for (; *exc == '+' || *exc == '-'; ++exc)
				switch (*exc) {
				case '+':
					++flagoff;
					break;
				case '-':
					--flagoff;
					break;
				}
			for (; *exc == '#' || *exc == 'l' || *exc == 'p'; ++exc)
				switch (*exc) {
				case '#':
					cmd.flags |= E_F_HASH;
					break;
				case 'l':
					cmd.flags |= E_F_LIST;
					break;
				case 'p':
					cmd.flags |= E_F_PRINT;
					break;
				}
			for (; *exc == '+' || *exc == '-'; ++exc)
				switch (*exc) {
				case '+':
					++flagoff;
					break;
				case '-':
					--flagoff;
					break;
				}
			break;
		case '2':				/* -, ., +, ^ */
			for (;; ++exc)
				switch (*exc) {
				case '-':
					cmd.flags |= E_F_DASH;
					break;
				case '.':
					cmd.flags |= E_F_DOT;
					break;
				case '+':
					cmd.flags |= E_F_PLUS;
					break;
				case '^':
					cmd.flags |= E_F_CARAT;
					break;
				default:
					goto end2;
				}
end2:			break;
#ifdef XXX_THIS_NO_LONGER_USED
		case '>':				/*  >> */
			if (exc[0] == '>' && exc[1] == '>') {
				cmd.flags |= E_APPEND;
				exc += 2;
			}
			break;
#endif
		case 'b':				/* buffer */
			cmd.buffer = *exc++;
			break;
		case 'c':				/* count */
			if (isdigit(*exc)) {
				lcount = USTRTOL(exc, &ep, 10);
				if (lcount == 0) {
					msg("Count may not be zero.");
					return (1);
				}
				exc = ep;
				/*
				 * Fix up the addresses.  Count's only occur
				 * with commands taking two addresses.  Replace
				 * the first with the second and recompute the
				 * second.
				 */
				cmd.addr1 = cmd.addr2;
				cmd.addr2.lno = cmd.addr1.lno + lcount;
			}
			break;
		case 'l':				/* line */
			if (isdigit(*exc)) {
				cmd.lineno = USTRTOL(exc, &ep, 10);
				exc = ep;
			}
			break;
		case 'f':				/* file */
			if (buildargv(exc, 1, &cmd))
				return (1);
			goto countchk;
		case 's':				/* string */
			cmd.string = exc;
			goto addr2;
		case 'w':				/* word */
			if (buildargv(exc, 0, &cmd))
				return (1);
countchk:		if (*++p != 'N') {		/* N */
				/*
				 * If a number is specified, must either be
				 * 0 or that number, if optional, and that
				 * number, if required.
				 */
				num = *p - '0';
				if ((*++p != 'o' || cmd.argc != 0) &&
				    cmd.argc != num)
					goto usage;
			}
			goto addr2;
		default:
			msg("Internal syntax table error (%s).", cp->name);
		}
	}

	/*
	 * Shouldn't be anything left, and no more required fields.
	 * That means neither 'l' or 'r' in the syntax.
	 */
	for (; *exc && isspace(*exc); ++exc);
	if (*exc || USTRPBRK(p, "lr")) {
usage:		msg("Usage: %s.", cp->usage);
		return (1);
	}

	/* Verify that the addresses are legal. */
addr2:	switch(cmd.addrcnt) {
	case 2:
		num = cmd.addr2.lno;
		if (num < 0) {
			msg("%lu is an invalid address.", num);
			return (1);
		}
		lcount = file_lline(curf);
		if (num > lcount) {
			if (lcount == 0)
				msg("The file is empty.");
			else
				msg("Only %lu lines in the file.", lcount);
			return (1);
		}
		/* FALLTHROUGH */
	case 1:
		num = cmd.addr1.lno;
		if (num == 0 && !(cp->flags & E_ZERO)) {
			msg("The %s command doesn't permit an address of 0.",
			    cp->name);
			return (1);
		}
		if (num < 0) {
			msg("%lu is an invalid address.", num);
			return (1);
		}
		if (num > file_lline(curf)) {
			msg("Only %lu lines in the file.", file_lline(curf));
			return (1);
		}
		break;
	}

	/* If doing a default command, vi just moves to the line. */
	if (mode == MODE_VI && uselastcmd) {
		curf->lno = cmd.addr1.lno;
		curf->cno = cmd.addr1.cno;
		return (0);
	}

	/* Reset "last" command. */
	if (cp->flags & E_SETLAST)
		lastcmd = cp;

	cmd.cmd = cp;
#if DEBUG && 0
{
	int __cnt;

	TRACE("ex_cmd: %s", cmd.cmd->name);
	if (cmd.addrcnt > 0) {
		TRACE("\taddr1 %d", cmd.addr1.lno);
		if (cmd.addrcnt > 1)
			TRACE(" addr2: %d", cmd.addr2.lno);
		TRACE("\n");
	}
	if (cmd.lineno)
		TRACE("\tlineno %d", cmd.lineno);
	if (cmd.flags)
		TRACE("\tflags %0x", cmd.flags);
	if (cmd.command)
		TRACE("\tcommand %s", cmd.command);
	if (cmd.plus)
		TRACE("\tplus %s", cmd.plus);
	if (cmd.buffer != OOBCB)
		TRACE("\tbuffer %c", cmd.buffer);
	TRACE("\n");
	if (cmd.argc) {
		for (__cnt = 0; __cnt < cmd.argc; ++__cnt)
			TRACE("\targ %d: {%s}", __cnt, cmd.argv[__cnt]);
		TRACE("\n");
	}
}
#endif
	/* Do the command. */
	autoprint = 0;

	/* If file state, set rptlines and log the start of an action. */
	if (curf != NULL) {
		curf->rptlines = 0;
		(void)log_cursor(curf);
	}

	if ((cp->fn)(&cmd))
		return (1);

	/* If no file state, the rest of this isn't all that useful. */
	if (curf == NULL)
		return (0);

	/*
	 * If the command was successful, and there was an explicit flag to
	 * display the new cursor line, or we're in ex mode, autoprint is set,
	 * and a change was made, display the line.
	 */
	if (flagoff) {
		if (flagoff < 0) {
			if ((recno_t)flagoff > curf->lno) {
				msg("Flag offset before line 1.");
				return (1);
			}
		} else if (curf->lno + flagoff > file_lline(curf)) {
			msg("Flag offset past end-of-file.");
			return (1);
		}
		curf->lno += flagoff;
	}

	if (mode == MODE_EX && autoprint && ISSET(O_AUTOPRINT))
		flags = E_F_PRINT;
	else
		flags = cmd.flags & (E_F_HASH | E_F_LIST | E_F_PRINT);
	parg.addr1.lno = parg.addr2.lno = curf->lno;
	parg.addr1.cno = parg.addr2.cno = curf->cno;
	if (flags) {
		switch (flags) {
		case E_F_HASH:
			parg.cmd = &cmds[C_HASH];
			ex_number(&parg);
			break;
		case E_F_LIST:
			parg.cmd = &cmds[C_LIST];
			ex_list(&parg);
			break;
		case E_F_PRINT:
			parg.cmd = &cmds[C_PRINT];
			ex_pr(&parg);
			break;
		}
	}
	return (0);
}

/*
 * linespec --
 *	Parse a line specifier for ex commands.
 */
static u_char *
linespec(cmd, cp)
	u_char *cmd;
	EXCMDARG *cp;
{
	MARK cur, savecursor, sm, *mp;
	recno_t num, total;
	int savecursor_set;
	u_char *ep;

	/* Percent character is all lines in the file. */
	if (*cmd == '%') {
		cp->addr1.lno = 1;
		cp->addr1.cno = 0;
		cp->addr2.lno = file_lline(curf);
		cp->addr2.cno = 0;
		cp->addrcnt = 2;
		return (++cmd);
	}

	/* Parse comma or semi-colon delimited line specs. */
	savecursor_set = 0;
	for (cp->addrcnt = 0;;) {
		switch(*cmd) {
		case ';':		/* Semi-colon delimiter. */
			/*
			 * Comma delimiters delimit; semi-colon delimiters
			 * change the current address for the 2nd address
			 * to be the first address.  Trailing or multiple
			 * delimiters are discarded.
			 */
			if (cp->addrcnt == 0)
				goto done;
			if (!savecursor_set) {
				savecursor.lno = curf->lno;
				savecursor.cno = curf->cno;
				curf->lno = cp->addr1.lno;
				curf->cno = cp->addr1.cno;
			}
			savecursor_set = 1;
			/* FALLTHROUGH */
		case ',':		/* Comma delimiter. */
			++cmd;
			break;
		case '.':		/* Current position. */
			cur.lno = curf->lno;
			cur.cno = curf->cno;
			++cmd;
			break;
		case '$':		/* Last line. */
			cur.lno = file_lline(curf);
			cur.cno = 0;
			++cmd;
			break;
					/* Absolute line number. */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			cur.lno = USTRTOL(cmd, &ep, 10);
			cur.cno = 0;
			cmd = ep;
			break;
		case '\'':		/* Set mark. */
			if (cmd[1] == '\0') {
				msg("No mark name; use 'a' to 'z'.");
				return (NULL);
			}
			if ((mp = mark_get(cmd[1])) == NULL)
				return (NULL);
			cur = *mp;
			cmd += 2;
			break;
		case '/':		/* Search forward. */
			sm.lno = curf->lno;
			sm.cno = curf->cno;
			if ((mp = f_search(curf,
			    &sm, cmd, &ep, SEARCH_MSG | SEARCH_PARSE)) == NULL)
				return (NULL);
			curf->lno = mp->lno;
			curf->cno = mp->cno;
			cmd = ep;
			break;
		case '?':		/* Search backward. */
			sm.lno = curf->lno;
			sm.cno = curf->cno;
			if ((mp = b_search(curf,
			    &sm, cmd, &ep, SEARCH_MSG | SEARCH_PARSE)) == NULL)
				return (NULL);
			curf->lno = mp->lno;
			curf->cno = mp->cno;
			cmd = ep;
			break;
		default:
			goto done;
		}

		/*
		 * Evaluate any offset.  Offsets are +/- any number, or, any
		 * number of +/- signs, or any combination thereof.
		 */
		for (total = 0;
		    *cmd == '-' || *cmd == '+'; total += num) {
			num = *cmd == '-' ? -1 : 1;
			if (isdigit(*++cmd)) {
				num *= USTRTOL(cmd, &ep, 10);
				cmd = ep;
			}
		}
		if (total < 0 && -total >= cur.lno) {
			bell();
			if (ISSET(O_VERBOSE))
				msg("Line number less than 1.");
			return (NULL);
		}
		cur.lno += total;

		/* Extra addresses are discarded, starting with the first. */
		switch(cp->addrcnt) {
		case 0:
			cp->addr1 = cur;
			cp->addrcnt = 1;
			break;
		case 1:
			cp->addr2 = cur;
			cp->addrcnt = 2;
			break;
		case 2:
			cp->addr1 = cp->addr2;
			cp->addr2 = cur;
			break;
		}
	}

	/*
	 * XXX
	 * This is probably not right for treatment of savecursor -- figure
	 * out what the historical ex did for ";,;,;5p" or similar stupidity.
	 */
done:	if (savecursor_set) {
		curf->lno = savecursor.lno;
		curf->cno = savecursor.cno;
	}

	return (cmd);
}

typedef struct {
	int len;		/* Buffer length. */
	u_char *bp;		/* Buffer. */
#define	A_ALLOCATED	0x01	/* If allocated space. */
	u_char flags;
} ARGS;

/*
 * buildargv --
 *	Build an argv from the rest of the command line.
 */
int
buildargv(exc, expand, cp)
	u_char *exc;
	int expand;
	EXCMDARG *cp;
{
	static ARGS *args;
	static int argscnt;
	static u_char **argv;
	static glob_t g;
	register int ch;
	int cnt, done, globoff, len, needslots, off;
	u_char *ap;

	/* Discard any previous information. */
	if (g.gl_pathc) {
		globfree(&g);
		g.gl_pathc = 0;
		g.gl_offs = 0;
		g.gl_pathv = NULL;
	}

	/* Break into argv vector. */
	for (done = globoff = off = 0;; ) {
		/* New argument; NULL terminate. */
		for (ap = exc; *exc && !isspace(*exc); ++exc);
		if (*exc)
			*exc = '\0';
		else
			done = 1;

		/*
		 * Expand and count up the necessary slots.  Add +1 to
		 * include the trailing NULL.
		 */
		len = exc - ap +1;

		if (expand) {
			if (fileexpand(&g, ap, len))
				return (1);
			needslots = g.gl_pathc - globoff + 1;
		} else
			needslots = 2;

		/*
		 * Allocate more pointer space if necessary; leave a space
		 * for a trailing NULL.
		 */
#define	INCREMENT	20
		if (off + needslots >= argscnt - 1) {
			argscnt += cnt = MAX(INCREMENT, needslots);
			if ((args =
			    realloc(args, argscnt * sizeof(ARGS))) == NULL) {
				free(argv);
				goto mem1;
			}
			if ((argv =
			    realloc(argv, argscnt * sizeof(char *))) == NULL) {
				free(args);
mem1:				argscnt = 0;
				args = NULL;
				argv = NULL;
				msg("Error: %s.", strerror(errno));
				return (1);
			}
			bzero(&args[off], cnt * sizeof(ARGS));
		}

		/*
		 * Copy the argument(s) into place, allocating space if
		 * necessary.
		 */
		if (expand) {
			for (cnt = globoff; cnt < g.gl_pathc; ++cnt, ++off) {
				if (args[off].flags & A_ALLOCATED) {
					free(args[off].bp);
					args[off].flags &= ~A_ALLOCATED;
				}
				argv[off] = args[off].bp =
				    (u_char *)g.gl_pathv[cnt];
			}
			globoff = g.gl_pathc;
		} else {
			if (args[off].len < len && (args[off].bp =
			    realloc(args[off].bp, len)) == NULL) {
				args[off].bp = NULL;
				args[off].len = 0;
				msg("Error: %s.", strerror(errno));
				return (1);
			}
			argv[off] = args[off].bp;
			bcopy(ap, args[off].bp, len);
			args[off].flags |= A_ALLOCATED;
			++off;
		}

		if (done)
			break;

		/* Skip whitespace. */
		while (ch = *++exc) {
			if (ch == '\\' && exc[1])
				++exc;
			if (!isspace(ch))
				break;
		}
		if (!*exc)
			break;
	}
	argv[off] = NULL;
	cp->argv = argv;
	cp->argc = off;
	return (0);
}

static int
fileexpand(gp, word, wordlen)
	glob_t *gp;
	u_char *word;
	int wordlen;
{
	static int tpathlen;
	static u_char *tpath;
	EXF *ep;
	int len, olen, plen;
	u_char ch, *p;

	/*
	 * Check for escaped %, # characters.
	 * XXX
	 */
	/* Figure out how much space we need for this argument. */
	ep = NULL;
	len = wordlen;
	for (p = word, olen = plen = 0; p = USTRPBRK(p, "%#"); ++p)
		if (*p == '%') {
			if (FF_ISSET(curf, F_NONAME)) {
				msg("No filename to substitute for %%.");
				return (1);
			}
			len += curf->nlen;
		} else {
			if (ep == NULL)
				ep = file_prev(curf, 0);
			if (ep == NULL || FF_ISSET(ep, F_NONAME)) {
				msg("No filename to substitute for #.");
				return (1);
			}
			len += ep->nlen;
		}

	if (len != wordlen) {
		/*
		 * Copy argument into temporary space, replacing file
		 * names.  Allocate temporary space if necessary.
		 */
		if (tpathlen < len) {
			len = MAX(len, 64);
			if ((tpath = realloc(tpath, len)) == NULL) {
				tpathlen = 0;
				tpath = NULL;
				msg("Error: %s.", strerror(errno));
				return (1);
			}
			tpathlen = len;
		}

		for (p = tpath; ch = *word; ++word)
			switch(ch) {
			case '%':
				bcopy(curf->name, p, curf->nlen);
				p += curf->nlen;
				break;
			case '#':
				bcopy(ep->name, p, ep->nlen);
				p += ep->nlen;
				break;
			case '\\':
				if (p[1] != '\0')
					++p;
			default:
				*p++ = ch;
			}
		p = tpath;
	} else
		p = word;

	glob((char *)p,
	    GLOB_APPEND|GLOB_NOSORT|GLOB_NOCHECK|GLOB_QUOTE, NULL, gp);
	return (0);
}
