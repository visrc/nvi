/* This file contains the code for reading ex commands. */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "config.h"
#include "options.h"
#include "vi.h"
#include "pathnames.h"
#include "extern.h"

static char **buildargv __P((char *, int *));

/* The flags are used to describe the command syntax. */
#define	E_DFLALL	0x0001	/* Default file range is 1,$. */
#define	E_TO		0x0002	/* Allow two line specifications. */
#define E_DFLNONE	0x0004	/* No default file range. */
#define E_EXRCOK	0x0008	/* Can be in a .exrc file. */
#define E_EXTRA		0x0010	/* Allow extra args after command name. */
#define E_FORCE		0x0020	/* Allow a ! after the command name. */
#define E_FROM		0x0040	/* Allow one line specification. */
#define E_NL		0x0080	/* If not MODE_EX mode, write newline first. */
#define E_NODFILE	0x0100	/* Do not default to the current file name. */
#define E_NOSPC		0x0200	/* No spaces allowed in the extra part. */
#define E_PLUS		0x0400	/* Allow a line number, as in ":e +32 foo". */
#define E_XFILE		0x0800	/* Expand wildcards in extra part. */
#define E_ZERO		0x1000	/* Allow 0 to be given as a line number. */

/*
 * This array maps ex command names to command codes.  The order in which
 * command names are listed below is significant -- ambiguous abbreviations
 * are always resolved to be the first possible match.  (e.g. "r" is taken
 * to mean "read", not "rewind", because "read" comes before "rewind").
 */
typedef struct _cmdlist {
	char *name;			/* Command name. */
	enum CMD code;			/* Command identifier. */
	void (*fn) __P((CMDARG *));	/* Underlying function. */
	u_short syntax;			/* Command syntax. */
} CMDLIST;

static CMDLIST cmds[] =  {
	{"append",	CMD_APPEND,	cmd_append,
	    E_FROM|E_ZERO},
#ifdef DEBUG
	{"bug",		CMD_DEBUG,	cmd_debug,
	    E_FROM|E_TO|E_FORCE|E_EXTRA|E_NL},
#endif
	{"change",	CMD_CHANGE,	cmd_append, 
	    E_FROM|E_TO},
	{"delete",	CMD_DELETE,	cmd_delete, 
	    E_FROM|E_TO|E_EXTRA|E_NOSPC},
	{"edit",	CMD_EDIT,	cmd_edit, 
	    E_FORCE|E_XFILE|E_EXTRA|E_NOSPC|E_PLUS},
	{"file",	CMD_FILE,	cmd_file, 
	    E_XFILE|E_EXTRA|E_NOSPC|E_NODFILE},
	{"global",	CMD_GLOBAL,	cmd_global, 
	    E_FROM|E_TO|E_FORCE|E_EXTRA|E_DFLALL},
	{"insert",	CMD_INSERT,	cmd_append,
	    E_FROM},
	{"join",	CMD_INSERT,	cmd_join, 
	    E_FROM|E_TO},
	{"k",	CMD_MARK,	cmd_mark, 
	    E_FROM|E_EXTRA|E_NOSPC},
	{"list",	CMD_LIST,	cmd_print, 
	    E_FROM|E_TO|E_NL},
	{"move",	CMD_MOVE,	cmd_move, 
	    E_FROM|E_TO|E_EXTRA},
	{"next",	CMD_NEXT,	file_next, 
	    E_FORCE|E_XFILE|E_EXTRA|E_NODFILE},
	{"print",	CMD_PRINT,	cmd_print, 
	    E_FROM|E_TO|E_NL},
	{"quit",	CMD_QUIT,	cmd_xit, 
	    E_FORCE		},
	{"read",	CMD_READ,	cmd_read, 
	    E_FROM|E_ZERO|E_XFILE|E_EXTRA|E_NOSPC|E_NODFILE},
	{"substitute",	CMD_SUBSTITUTE,	cmd_substitute, 
	    E_FROM|E_TO|E_EXTRA},
	{"to",	CMD_COPY,	cmd_move, 
	    E_FROM|E_TO|E_EXTRA},
	{"undo",	CMD_UNDO,	cmd_undo, 0},
	{"vglobal",	CMD_VGLOBAL,	cmd_global, 
	    E_FROM|E_TO|E_EXTRA|E_DFLALL},
	{"write",	CMD_WRITE,	cmd_write, 
	    E_FROM|E_TO|E_FORCE|E_XFILE|E_EXTRA|E_NOSPC|E_DFLALL},
	{"xit",	CMD_XIT,	cmd_xit, 
	    E_FORCE|E_NL},
	{"yank",	CMD_YANK,	cmd_delete, 
	    E_FROM|E_TO|E_EXTRA|E_NOSPC},

	{"!",	CMD_BANG,	cmd_shell, 
	    E_EXRCOK|E_FROM|E_TO|E_XFILE|E_EXTRA|E_NODFILE|E_DFLNONE|E_NL},
	{"<",	CMD_SHIFTL,	cmd_shift, 
	    E_FROM|E_TO},
	{">",	CMD_SHIFTR,	cmd_shift, 
	    E_FROM|E_TO},
	{"=",	CMD_EQUAL,	cmd_file, 
	    E_FROM|E_TO},
	{"&",	CMD_SUBAGAIN,	cmd_substitute, 
	    E_FROM|E_TO},
#ifndef	NO_AT
	{"@",	CMD_AT,	cmd_at, 
	    E_EXTRA},
#endif

#ifndef	NO_ABBR
	{"abbreviate",	CMD_ABBR,	cmd_abbr, 
	    E_EXRCOK|E_EXTRA},
#endif
	{"args",	CMD_ARGS,	file_args, 
	    E_EXRCOK},
#ifndef	NO_ERRLIST
	{"cc",	CMD_CC,	cmd_make, 
	    E_FORCE|E_XFILE|E_EXTRA},
#endif
	{"cd",	CMD_CD,	cmd_cd, 
	    E_EXRCOK|E_XFILE|E_EXTRA|E_NOSPC|E_NODFILE},
	{"copy",	CMD_COPY,	cmd_move, 
	    E_FROM|E_TO|E_EXTRA},
#ifndef	NO_DIGRAPH
	{"digraph",	CMD_DIGRAPH,	cmd_digraph, 
	    E_EXRCOK|E_FORCE|E_EXTRA},
#endif
#ifndef	NO_ERRLIST
	{"errlist",	CMD_ERRLIST,	cmd_errlist, 
	    E_FORCE|E_XFILE|E_EXTRA|E_NOSPC|E_NODFILE},
#endif
	{"ex",	CMD_EDIT,	cmd_edit, 
	    E_FORCE|E_XFILE|E_EXTRA|E_NOSPC},
#ifdef	SYSV_COMPAT
	{"mark",	CMD_MARK,	cmd_mark, 
	    E_FROM|E_EXTRA|E_NOSPC},
#endif
	{"map",	CMD_MAP,	cmd_map, 
	    E_EXRCOK|E_FORCE|E_EXTRA},
#ifndef	NO_MKEXRC
	{"mkexrc",	CMD_MKEXRC,	cmd_mkexrc, 
	    E_XFILE|E_EXTRA|E_NOSPC|E_NODFILE},
#endif
	{"number",	CMD_NUMBER,	cmd_print, 
	    E_FROM|E_TO|E_NL},
	{"put",	CMD_PUT,	cmd_put, 
	    E_FROM|E_ZERO|E_EXTRA|E_NOSPC},
	{"set",	CMD_SET,	cmd_set, 
	    E_EXRCOK|E_EXTRA},
	{"shell",	CMD_SHELL,	cmd_shell, 
	    E_NL},
	{"source",	CMD_SOURCE,	cmd_source, 
	    E_EXRCOK|E_XFILE|E_EXTRA|E_NOSPC|E_NODFILE},
	{"tag",	CMD_TAG,	cmd_tag, 
	    E_FORCE|E_EXTRA|E_NOSPC},
	{"version",	CMD_VERSION,	cmd_version, 
	    E_EXRCOK},
	{"visual",	CMD_VISUAL,	cmd_edit, 
	    E_FORCE|E_XFILE|E_EXTRA|E_NOSPC|E_NODFILE},
	{"wq",	CMD_WQUIT,	cmd_xit, 
	    E_NL},

#ifdef	DEBUG
	{"debug",	CMD_DEBUG,	cmd_debug, 
	    E_FROM|E_TO|E_FORCE|E_EXTRA|E_NL},
	{"validate",	CMD_VALIDATE,	cmd_validate, 
	    E_FORCE|E_NL},
#endif
	{"chdir",	CMD_CD,	cmd_cd, 
	    E_EXRCOK|E_XFILE|E_EXTRA|E_NOSPC|E_NODFILE},
#ifndef	NO_COLOR
	{"color",	CMD_COLOR,	cmd_color, 
	    E_EXRCOK|E_EXTRA},
#endif
#ifndef	NO_ERRLIST
	{"make",	CMD_MAKE,	cmd_make, 
	    E_FORCE|E_XFILE|E_EXTRA|E_NODFILE},
#endif
#ifndef	SYSV_COMPAT
	{"mark",	CMD_MARK,	cmd_mark, 
	    E_FROM|E_EXTRA|E_NOSPC},
#endif
	{"previous",	CMD_PREVIOUS,	file_prev, 
	    E_FORCE},
	{"rewind",	CMD_REWIND,	file_rew,
	    E_FORCE},
	{"unmap",	CMD_UNMAP,	cmd_map,
	    E_EXRCOK|E_FORCE|E_EXTRA},
#ifndef	NO_ABBR
	{"unabbreviate",CMD_UNABBR,	cmd_abbr,
	    E_EXRCOK|E_EXTRA|E_NOSPC},
#endif
	{NULL},
};

/* This function reads an ex command and executes it. */
void
ex()
{
	static long oldline;
	register int cmdlen;
	CMDARG cmd;
	char cmdbuf[150];

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
		doexcmd(cmdbuf);

		/* Handle autoprint. */
		if (significant || markline(cursor) != oldline) {
			significant = FALSE;
			oldline = markline(cursor);
			if (ISSET(O_AUTOPRINT) && mode == MODE_EX) {
				cmd.frommark = cmd.tomark = cursor;
				cmd.cmd = CMD_PRINT;
				cmd.force = 0;
				cmd.extra = "";
				cmd_print(&cmd);
			}
		}
	}
}

/*
 * doexcmd --
 *    Parse and execute an ex command.  The format of an ex command is:
 *            :line_address command[!] parameters count flags
 *    Most everything is optional.
 */
void
doexcmd(cmdbuf)
	char *cmdbuf;		/* String containing an ex command. */
{
	register int cmdidx;	/* Command index. */
	register int cmdlen;	/* Length of command name. */
	register char *build;	/* Used while copying filenames. */
	CMDARG	cmdarg;
	CMDLIST *cp;
	MARK addr1, addr2;
	enum CMD cmd;		/* Command. */
	u_short syntax;		/* Argument types for this command. */
	short forceit;		/* `!' character specified. */
	int iswild;		/* Boolean: filenames use wildcards. */
	int isdfl;		/* Using default line ranges. */
	int didsub;		/* Substitute file names for % or #. */
	int lineset;		/* If line range set by user. */

	/*
	 * Ex commands can't be undone via the shift-U command.
	 * XXX Why not?
	 */
	U_line = 0L;

	/* Permit extra colons at the start of the line. */
	while (*cmdbuf == ':')
		++cmdbuf;

	/* Ignore command lines that start with a double-quote. */
	if (*cmdbuf == '"')
		return;

	/* Skip whitespace. */
	for (; isspace(*cmdbuf); ++cmdbuf);

	/*
	 * Parse any line specifiers.  New command position is returned,
	 * or NULL on error.
	 */
TRACE("====\ncmdbuf {%s}", cmdbuf);
	if ((cmdbuf = linespec(cmdbuf, &addr1, &addr2, &lineset)) == NULL)
		return;
TRACE("addr1 %ld", addr1);
TRACE("addr2 %ld", addr2);
TRACE("lineset %d", lineset);
TRACE("cmdbuf {%s}", cmdbuf);

	/* Skip whitespace. */
	for (; isspace(*cmdbuf); ++cmdbuf);

	/* If no command, then just move the cursor to the last address. */
	if (!*cmdbuf) {
		cursor = addr2 == MARK_UNSET ? addr1 : addr2;
		return;
	}

	/* Figure out how long the command name is. */
	if (!isalpha(*cmdbuf))
		cmdlen = 1;
	else
		for (cmdlen = 1; isalpha(cmdbuf[cmdlen]); cmdlen++);

	/* Lookup the command code. */
	for (cp = &cmds[0];
	    cp->name && strncmp(cmdbuf, cp->name, cmdlen); ++cp);

	if (cp->name == NULL) {
		msg("Unknown command \"%.*s\".", cmdlen, cmdbuf);
		return;
	}

	cmd = cp->code;
	syntax = cp->syntax;

	/* If the command ended with a bang, set the forceit flag. */
	cmdbuf += cmdlen;
	if ((syntax & E_FORCE) && *cmdbuf == '!') {
		++cmdbuf;
		forceit = 1;
	} else
		forceit = 0;

	/* Skip any unquoted whitespace, leave cmdbuf pointing to arguments. */
	while (isspace(*cmdbuf) && !QTST(cmdbuf))
		++cmdbuf;

	/* A couple of special cases for filenames. */
	if (syntax & E_XFILE) {
		/* If names were given, process them. */
		if (*cmdbuf) {
			for (build = tmpblk.c, iswild = didsub = FALSE;
			    *cmdbuf; ++cmdbuf)
				switch (QTST(cmdbuf) ? '\0' : *cmdbuf) {
				case '%':
					if (!*origname) {
					msg("No filename to substitute for %%");
						return;
					}
					(void)strcpy(build, origname);
					while (*build)
						++build;
					didsub = TRUE;
					break;
				case '#':
					if (!*prevorig) {
					msg("No filename to substitute for #");
						return;
					}
					(void)strcpy(build, prevorig);
					while (*build)
						++build;
					didsub = TRUE;
					break;
				case '*':
				case '?':
				case '[':
				case '`':
				case '{': /* } */
				case '$':
				case '~':
					*build++ = *cmdbuf;
					iswild = TRUE;
					break;
				default:
					*build++ = *cmdbuf;
				}
			*build = '\0';

			if (cmd == CMD_BANG ||
			    cmd == CMD_READ && tmpblk.c[0] == '!' ||
			    cmd == CMD_WRITE && tmpblk.c[0] == '!') {
				if (didsub) {
					if (mode != MODE_EX)
						addch('\n');
					addstr(tmpblk.c);
					addch('\n');
					exrefresh();
				}
			} else if (iswild && tmpblk.c[0] != '>')
				cmdbuf = wildcard(tmpblk.c);

		/* No names given, maybe assume origname. */
		} else if (!(syntax & E_NODFILE))
			(void)strcpy(tmpblk.c, origname);
		else
			*tmpblk.c = '\0';

		cmdbuf = tmpblk.c;
	}

	/* Bad arguments. */
	if (!(syntax & E_EXRCOK) && nlines < 1L) {
		msg("Can't use the \"%s\" command in a %s file",
		    cmds[cmdidx].name, _NAME_EXRC);
		return;
	}
	if (!(syntax & (E_ZERO | E_EXRCOK)) && addr1 == MARK_UNSET) {
		msg("Can't use address 0 with \"%s\" command.",
		    cmds[cmdidx].name);
		return;
	}
	if (!(syntax & E_FROM) && addr1 != cursor && nlines >= 1L) {
		msg("Can't use address with \"%s\" command.",
		    cmds[cmdidx].name);
		return;
	}
	if (!(syntax & E_TO) && addr2 != addr1 && nlines >= 1L) {
		msg("Can't use a range with \"%s\" command.",
		    cmds[cmdidx].name);
		return;
	}
	if (!(syntax & E_EXTRA) && *cmdbuf) {
		msg("Extra characters after \"%s\" command.",
		    cmds[cmdidx].name);
		return;
	}
	if ((syntax & E_NOSPC) &&
	    !(cmd == CMD_READ && (forceit || *cmdbuf == '!'))) {
		build = cmdbuf;
		if ((syntax & E_PLUS) && *build == '+') {
			while (*build && !isspace(*build))
				++build;
			while (*build && isspace(*build))
				++build;
		}
		for (; *build; build++)
			if (isspace(*build)) {
				msg("Too many %s to \"%s\" command.",
				    (syntax & E_XFILE) ? "filenames" :
				    "arguments", cmds[cmdidx].name);
				return;
			}
	}

	/* Some commands have special default ranges. */
	if (isdfl && (syntax & E_DFLALL)) {
		addr1 = MARK_FIRST;
		addr2 = MARK_LAST;
	} else if (isdfl && (syntax & E_DFLNONE))
		addr1 = addr2 = 0L;

	/* Write a newline if called from visual mode. */
	if ((syntax & E_NL) && mode != MODE_EX && !exwrote) {
		addch('\n');
		exrefresh();
	}

	/* Do the command. */
	cmdarg.frommark = addr1;
	cmdarg.tomark = addr2;
	cmdarg.cmd = cmd;
	cmdarg.force = forceit;
	cmdarg.extra = cmdbuf && *cmdbuf ? cmdbuf : NULL;
	(*cmds[cmdidx].fn)(&cmdarg);
}

/*
 * linespec --
 *	Parse a line specifier for ex commands.
 */
char *
linespec(cmd, addr1p, addr2p, linesetp)
	char *cmd;
	MARK *addr1p, *addr2p;
	int *linesetp;
{
	enum SET { NOTSET, ONESET, TWOSET } set;
	MARK cur, savecursor;
	long num, total;
	int delimiter;
	char ch, *ep;

	/* Percent character is all lines in the file. */
	if (*cmd == '%') {
		*addr1p = *addr2p = MARK_LAST;
		*linesetp = 2;
		return (++cmd);
	}

	/* Parse comma or semi-colon delimited line specs. */
	*linesetp = 0;
	*addr1p = *addr2p = savecursor = MARK_UNSET;
	for (set = NOTSET;;) {
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

		/* Multiple addresses are discarded, starting with the first. */
		switch(set) {
		case NOTSET:
			set = ONESET;
			*addr1p = cur;
			break;
		case ONESET:
			set = TWOSET;
			*addr2p = cur;
			break;
		case TWOSET:
			*addr1p = *addr2p;
			*addr2p = cur;
			break;
		}
	}

	/*
	 * XXX
	 * This is probably not right for treatment of savecursor -- need to
	 * figure out what the historical ex did for ";,;,;5p" or similar
	 * stupidity.
	 */
done:	if (savecursor != MARK_UNSET)
		cursor = savecursor;

	switch(set) {
	case TWOSET:
		++*linesetp;
		num = markline(*addr2p);
		if (num < 1L || num > nlines) {
			msg("Invalid second address (1 to %ld)", nlines);
			return (NULL);
		}
		/* FALLTHROUGH */
	case ONESET:
		++*linesetp;
		num = markline(*addr1p);
		if (num < 1L || num > nlines) {
			msg("Invalid first address (1 to %ld)", nlines);
			return (NULL);
		}
		break;
	}
	return (cmd);
}


/*
 * exfile --
 *	Execute EX commands from a file.
 */
void
exfile(filename)
	char *filename;
{
	struct stat sb;
	int fd, len;
	char *bp;

	if ((fd = open(filename, O_RDONLY, 0)) < 0)
		goto e1;
	if (fstat(fd, &sb))
		goto e2;
	if ((bp = malloc(sb.st_size)) == NULL)
		goto e2;

	len = read(fd, bp, sb.st_size);
	if (len == -1 || len != sb.st_size) {
		if (len != sb.st_size)
			errno = EIO;
		goto e3;
	}

	exstring(bp, len, 0);
	free(bp);
	(void)close(fd);
	return;

e3:	free(bp);
e2:	(void)close(fd);
e1:	msg("%s: %s", filename, strerror(errno));
}

/*
 * exstring --
 *	Execute EX commands from a string.  The commands may be separated
 *	by newlines or by | characters, and may be quoted.
 */
void
exstring(cmd, len, copy)
	register char *cmd;
	register int len;
	int copy;
{
	register char *p, *t;
	char *start;

	/* Maybe an empty string. */
	if (len == 0)
		return;

	/* Make a copy if necessary. */
	if (copy) {
		if ((start = malloc(len)) == NULL) {
			msg("%s", strerror(errno));
			return;
		}
		bcopy(cmd, start, len);
		cmd = start;
	}

	/*
	 * Walk the command, checking for ^V quotes.  The string "^V\n" is
	 * treated as a single ^V.
	 */
	do {
		QSTART(cmd);
		for (p = t = cmd; len-- && p[0] != '|' && p[0] != '\n'; ++p) {
			if (p[0] == ('V' & 0x1f) && p[1] != '\n') {
				QSET(t);
				++p;
				--len;
			}
			*p++ = *t++;
		}
		*p = '\0';
		QEND();

		doexcmd(cmd);
		cmd = p + 1;
	} while (len);

	if (copy)
		free(start);
}

/*
 * buildargv --
 *	Turn the command into an argc/argv pair.
 */
static char **
buildargv(cmd, argcp)
	char *cmd;
	int *argcp;
{
#define	MAXARGS	100
	static char *argv[MAXARGS + 1];
	int cnt;
	char **ap;

	for (ap = argv, cnt = 0; (*ap = strsep(&cmd, " \t")) != NULL;) {
		if (**ap != '\0') {
			++ap;
			++cnt;
		}
		if (cnt == MAXARGS) {
			msg("Too many arguments, maximum is %d.", MAXARGS);
			return (NULL);
		}
	}
	*argcp = cnt;
	return (argv);
}
