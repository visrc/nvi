/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex.c,v 5.17 1992/04/17 09:39:38 bostic Exp $ (Berkeley) $Date: 1992/04/17 09:39:38 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <glob.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "vi.h"
#include "curses.h"
#include "excmd.h"
#include "options.h"
#include "pathnames.h"
#include "extern.h"

char *defcmdarg[2];

static int fileexpand __P((glob_t *, char *, int));

/*
 * ex --
 *	Read an ex command and execute it.
 */
void
ex()
{
	static long oldline;
	register int cmdlen;
	CMDARG cmd;
	char cmdbuf[512];

	significant = FALSE;
	oldline = markline(cursor);

	while (mode == MODE_EX) {
		cmdlen =
		    vgets(ISSET(O_PROMPT) ? ':' : '\0', cmdbuf, sizeof(cmdbuf));
		if (cmdlen < 0)
			return;

		/* If empty line, assume ".+1". */
		if (cmdlen == 0) {
			(void)strcpy(cmdbuf, ".+1");
			qaddch('\r');
			clrtoeol();
		} else
			addch('\n');
		refresh();

		/* Parse & execute the command. */
		(void)ex_cmd(cmdbuf);

		/* Handle autoprint. */
		if (significant || markline(cursor) != oldline) {
			significant = FALSE;
			oldline = markline(cursor);
			if (ISSET(O_AUTOPRINT) && mode == MODE_EX) {
				SETCMDARG(cmd,
				    NULL, 2, cursor, cursor, 0, NULL);
				ex_print(&cmd);
			}
		}
	}
}

/*
 * ex_cfile --
 *	Execute EX commands from a file.
 */
int
ex_cfile(filename, noexisterr)
	char *filename;
	int noexisterr;
{
	struct stat sb;
	int fd, len, rval;
	char *bp;

	if ((fd = open(filename, O_RDONLY, 0)) < 0) {
		if (noexisterr)
			goto e1;
		return (0);
	}
	if (fstat(fd, &sb))
		goto e2;
	/* Add in +1 so can have a trailing EOS. */
	if ((bp = malloc(sb.st_size + 1)) == NULL)
		goto e2;

	len = read(fd, bp, sb.st_size);
	if (len == -1 || len != sb.st_size) {
		if (len != sb.st_size)
			errno = EIO;
		goto e3;
	}
	bp[sb.st_size] = '\0';

	rval = ex_cstring(bp, len);
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
 *	by newlines or by | characters, and may be quoted.  The string is
 *	expected to be EOS terminated.
 */
int
ex_cstring(cmd, len)
	register char *cmd;
	register int len;
{
	register char *p, *t;
	char *start;

	/* Maybe an empty string. */
	if (len == 0)
		return (0);

	/*
	 * Walk the command, checking for ^V quotes.  The string "^V\n" is
	 * treated as a single ^V.  Len is artificially incremented by one
	 * so that we find the trailing EOS and do the last command,
	 */
	QSTART(cmd);
	for (p = t = cmd, ++len; len--;)
		switch(*p) {
		case '|':
		case '\n':
			*p = '\0';
			/* FALLTHROUGH */
		case '\0':
			if (p > cmd) {
				QEND();
				if (ex_cmd(cmd))
					return (1);
			}
			cmd = t = ++p;
			QSTART(cmd);
			break;
		case 'V' & 0x1f:
			if (len > 1 && p[1] != '\n') {
				QSET(t);
				++p;
				--len;
			}
			/* FALLTHROUGH */
		default:
			*p++ = *t++;
			break;
		}

	return (0);
}

static CMDLIST *lastcmd = &cmds[C_PRINT];

/*
 * ex_cmd --
 *    Parse and execute an ex command.
 */
int
ex_cmd(exc)
	register char *exc;
{
	extern int reading_exrc;
	register int cmdlen;
	register char *p;
	CMDARG	cmd;
	CMDLIST *cp;
	u_long count, num;
	u_short flags;
	char *ep;

	/*
	 * Ex commands can't be undone via the shift-U command.
	 * XXX Why not?
	 */
	U_line = 0L;

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
	bzero(&cmd, sizeof(CMDARG));

	/*
	 * Parse line specifiers.  New command line position is returned,
	 * or NULL on error.
	 */
	if ((exc = linespec(exc, &cmd)) == NULL)
		return (1);

	/* Skip whitespace. */
	for (; isspace(*exc); ++exc);

	/*
	 * If no command, ex does the last specified of p, l, or #, and
	 * vi moves to the line.
	 */
	if (!*exc) {
		cp = lastcmd;
		goto addr1;
	}

	/*
	 * Figure out how long the command name is.  There are a few
	 * commands that aren't alphabetic, but they are all single
	 * character commands.
	 */
	if (index("!&<=>@", *exc)) {
		p = exc;
		exc++;
		cmdlen = 1;
	} else {
		for (p = exc; isalpha(*exc); ++exc);
		cmdlen = exc - p;
	}
	for (cp = cmds; cp->name && strncmp(p, cp->name, cmdlen); ++cp);
	if (cp->name == NULL) {
		msg("The %.*s command is unknown.", cmdlen, p);
		return (1);
	}
	cmd.cmd = cp;

	/* Some commands aren't permitted in .exrc files. */
	if (reading_exrc && !(cp->flags & E_EXRCOK)) {
		msg("Can't use the %s command in a .exrc file.", cp->name);
		return (1);
	}

	/* Some commands are turned off. */
	if (cp->flags & E_NOPERM) {
		msg("This version doesn't support the %.*s command.",
		    cmdlen, p);
		return (1);
	}

	/*
	 * Set the default addresses.  It's an error to specify an address
	 * for a command that doesn't take addresses.  If two addresses are
	 * specified for a command that only takes one, lose the first one.
	 * A nasty special case here, the '!' command takes 0, 1, or 2
	 * addresses.
	 */
addr1:	switch(cp->flags & (E_ADDR1|E_ADDR2|E_ADDR2_OR_0)) {
	case E_ADDR1:				/* One address: */
		switch(cmd.addrcnt) {
		case 0:				/* Default to cursor. */
			cmd.addrcnt = 1;
			cmd.addr1 = cursor;
			break;
		case 1:
			break;
		case 2:				/* Lose the first address. */
			cmd.addrcnt = 1;
			cmd.addr1 = cmd.addr2;
		}
		break;
	case E_ADDR2_OR_0:			/* Zero or two addresses: */
		if (cmd.addrcnt == 0) {		/* Default to entire file. */
			cmd.addrcnt = 2;
			cmd.addr1 = MARK_FIRST;
			cmd.addr2 = MARK_LAST;
			break;
		}
		/* FALLTHROUGH */
	case E_ADDR2:				/* Two addresses: */
		switch(cmd.addrcnt) {
		case 0:				/* Default to cursor. */
			cmd.addrcnt = 2;
			cmd.addr1 = cmd.addr2 = cursor;
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
	p = cp->syntax;
	if (*p == 's') {
		for (p = exc; *p && isspace(*p); ++p);
		cmd.string = *p ? exc : NULL;
		goto addr2;
	}
	for (count = 0; *p; ++p) {
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
			for (;; ++exc)
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
				default:
					goto end1;
				}
end1:			break;
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
		case '>':				/*  >> */
			if (exc[0] == '>' && exc[1] == '>') {
				cmd.flags |= E_F_APPEND;
				exc += 2;
			}
			break;
		case 'b':				/* buffer */
			if (isalpha(*exc))
				cmd.buffer = *exc++;
			break;
		case 'c':				/* count */
			if (isdigit(*exc)) {
				count = strtol(exc, &ep, 10);
				if (count == 0) {
					msg("Count may not be zero.");
					return (1);
				}
				exc = ep;
			}
			/*
			 * Fix up the addresses.  Count's only occur with
			 * commands taking two addresses.  Replace the first
			 * with the second and recompute the second.
			 */
			cmd.addr1 = cmd.addr2;
			cmd.addr2 = cmd.addr1 + count;
			break;
		case 'l':				/* line */
			/*
			 * XXX
			 * Check for illegal line numbers.
			 */
			if (isdigit(*exc)) {
				cmd.lineno = strtol(exc, &ep, 10);
				if (cmd.lineno == 0) {
					msg("Line number may not be zero.");
					return (1);
				}
				exc = ep;
			}
			break;
		case 'f':				/* file */
			if (buildargv(exc, 1, &cmd))
				return (1);
			goto countchk;
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
	if (*exc || strpbrk(p, "lr")) {
usage:		msg("Usage: %s.", cp->usage);
		return (1);
	}

	/* Verify that the addresses are legal. */
addr2:	switch(cmd.addrcnt) {
	case 2:
		num = markline(cmd.addr2);
		if (num < 0) {
			msg("%lu is an invalid address.", num);
			return (1);
		}
		if (num > nlines) {
			msg("Only $lu lines in the file.", nlines);
			return (1);
		}
		/* FALLTHROUGH */
	case 1:
		num = markline(cmd.addr1);
		if (num == 0 && !(flags & E_ZERO)) {
			msg("The %s command doesn't permit an address of 0.",
			    cp->name);
			return (1);
		}
		if (num < 0) {
			msg("%lu is an invalid address.", num);
			return (1);
		}
		if (num > nlines) {
			msg("Only $lu lines in the file.", nlines);
			return (1);
		}
		break;
	}

	/* If doing a default command, vi just moves to the line. */
	if (mode == MODE_VI && cp == lastcmd) {
		cursor = cmd.addr1;
		return;
	}

	/* Write a newline if called from visual mode. */
	if (flags & E_NL && mode != MODE_EX && !exwrote) {
		addch('\n');
		ex_refresh();
	}
#if defined(DEBUG) && 1
{
	int __cnt;

	TRACE("ex_cmd: %s", cmd.cmd->name);
	if (cmd.addrcnt > 0) {
		TRACE("address 1: %d", cmd.addr1);
		if (cmd.addrcnt > 1)
			TRACE("address 2: %d", cmd.addr2);
	}
	if (cmd.lineno)
		TRACE("\tlineno %d", cmd.lineno);
	if (cmd.flags)
		TRACE("\tflags %0x", cmd.flags);
	if (cmd.command)
		TRACE("\tcommand %s", cmd.command);
	if (cmd.plus)
		TRACE("\tplus %s", cmd.plus);
	if (cmd.buffer)
		TRACE("\tbuffer %c", cmd.buffer);
	for (__cnt = 0; __cnt < cmd.argc; ++__cnt)
		TRACE("\targ %d: {%s}", __cnt, cmd.argv[__cnt]);
	TRACE("\n");
}
#endif
	/* Do the command. */
	return ((*cp->fn)(&cmd));
}

/*
 * linespec --
 *	Parse a line specifier for ex commands.
 */
char *
linespec(cmd, cp)
	char *cmd;
	CMDARG *cp;
{
	MARK cur, savecursor;
	u_long num, total;
	int delimiter;
	char ch, *ep;

	/* Percent character is all lines in the file. */
	if (*cmd == '%') {
		cp->addr1 = MARK_FIRST;
		cp->addr2 = MARK_LAST;
		cp->addrcnt = 2;
		return (++cmd);
	}

	/* Parse comma or semi-colon delimited line specs. */
	savecursor = MARK_UNSET;
	for (cp->addrcnt = 0;;) {
		delimiter = 0;
		switch(*cmd) {
		case ';':		/* Delimiter. */
		case ',':		/* Delimiter. */
			delimiter = 1;
			/* FALLTHROUGH */
		case '.':		/* Current position. */
			cur = cursor;
			++cmd;
			break;
		case '$':		/* Last line. */
			cur = MARK_LAST;
			++cmd;
			break;
					/* Absolute line number. */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			cur = MARK_AT_LINE(strtol(cmd, &ep, 10));
			cmd = ep;
			break;
		case '\'':		/* Set mark. */
			cur = m_tomark(cursor, 1L, (int)cmd[1]);
			cmd += 2;
			break;
		case '/':		/* Search forward. */
			/* Terminate the search pattern. */
			ep = parseptrn(cmd);

			/* Search for the pattern. */
			cur &= ~(BLKSIZE - 1);
			pfetch(markline(cur));
			if (plen > 0)
				cur += plen - 1;
			cur = m_fsrch(cur, cmd);
			cmd = ep;
			break;
		case '?':		/* Search backward. */
			/* Terminate the search pattern. */
			ep = parseptrn(cmd);

			/* Search for the pattern. */
			cur &= ~(BLKSIZE - 1);
			cur = m_bsrch(cur, cmd);
			break;
		default:
			goto done;
		}

		/*
		 * Comma delimiters only delimit; semi-colon delimiters change
		 * the current line address for the second address to be the
		 * first address.  Trailing/multiple delimiters are discarded.
		 */
		if (delimiter) {
			if (*cmd == ';')
				savecursor = cursor;
		}

		/*
		 * Evaluate any offset.  Offsets are +/- any number, or, any
		 * number of +/- signs, or any combination thereof.
		 */
		else {
			total = 0;
			while (*cmd == '-' || *cmd == '+') {
				num = *cmd == '-' ? -1 : 1;
				if (isdigit(*++cmd)) {
					num *= strtol(cmd, &ep, 10);
					cmd = ep;
				}
				total += num;
			}
			if (total)
				cur = m_updnto(cur, num, ch);
		}

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
	 * This is probably not right for treatment of savecursor -- figure
	 * out what the historical ex did for ";,;,;5p" or similar stupidity.
	 * XXX
	 */
done:	if (savecursor != MARK_UNSET)
		cursor = savecursor;

	return (cmd);
}

typedef struct {
	int len;		/* Buffer length. */
	char *bp;		/* Buffer. */
#define	A_ALLOCATED	0x01	/* If allocated space. */
	u_char flags;
} ARGS;

/*
 * buildargv --
 *	Build an argv from the rest of the command line.
 */
int
buildargv(exc, expand, cp)
	char *exc;
	int expand;
	CMDARG	*cp;
{
	static ARGS *args;
	static int argscnt;
	static char **argv;
	static glob_t g;
	register int ch;
	int cnt, done, globoff, len, needslots, off;
	char *ap;

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
				argv[off] = args[off].bp = g.gl_pathv[cnt];
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
	char *word;
	int wordlen;
{
	static int tpathlen;
	static char *tpath;
	int cnt, len, olen, plen;
	char ch, *p;

	/*
	 * Check for escaped %, # characters.
	 * XXX
	 */
	/* Figure out how much space we need for this argument. */
	len = wordlen;
	for (p = word, olen = plen = 0; p = strpbrk(p, "%#"); ++p)
		if (*p == '%') {
			if (!*origname) {
				msg("No filename to substitute for %%.");
				return (1);
			}
			if (!olen)
				olen = strlen(origname);
			len += olen;
		} else {
			if (!*prevorig) {
				msg("No filename to substitute for #.");
				return (1);
			}
			if (!plen)
				plen = strlen(prevorig);
			len += cnt * plen;
		}

	if (olen || plen) {
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
				bcopy(origname, p, olen);
				p += olen;
				break;
			case '#':
				bcopy(prevorig, p, plen);
				p += plen;
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

	glob(p, GLOB_APPEND|GLOB_NOSORT|GLOB_NOCHECK|GLOB_QUOTE, NULL, gp);
	return (0);
}
