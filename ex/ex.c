/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex.c,v 8.40 1993/10/28 14:14:49 bostic Exp $ (Berkeley) $Date: 1993/10/28 14:14:49 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

static void	 ep_comm __P((SCR *, char **, char **, int *, char **, int *));
static char	*ep_line __P((SCR *, EXF *, char *, MARK *));
static char	*ep_range __P((SCR *, EXF *, char *, EXCMDARG *));
static void	 ep_re __P((SCR *, char **, char **, int *));

#define	DEFCOM	".+1"

/*
 * ex --
 *	Read an ex command and execute it.
 */
int
ex(sp, ep)
	SCR *sp;
	EXF *ep;
{
	TEXT *tp;
	int eval;
	char defcom[sizeof(DEFCOM)];

	if (ex_init(sp, ep))
		return (1);

	if (sp->s_refresh(sp, ep))
		return (ex_end(sp));

	/* Edited as it can be. */
	F_SET(sp->frp, FR_EDITED);

	for (eval = 0;;) {
		/* Get the next command. */
		switch (sp->s_get(sp, ep,
		    &sp->bhdr, ':', TXT_CR | TXT_PROMPT)) {
		case INP_OK:
			break;
		case INP_EOF:
			F_SET(sp, S_EXIT_FORCE);
			goto ret;
		case INP_ERR:
			continue;
		}

		tp = sp->bhdr.next;
		if (tp->len == 0) {
			if (F_ISSET(sp->gp, G_ISFROMTTY)) {
				(void)fputc('\r', sp->stdfp);
				(void)fflush(sp->stdfp);
			}
			memmove(defcom, DEFCOM, sizeof(DEFCOM));
			(void)ex_cstring(sp, ep, defcom, sizeof(DEFCOM) - 1);
		} else {
			if (F_ISSET(sp->gp, G_ISFROMTTY))
				(void)fputc('\n', sp->stdfp);
			(void)ex_cstring(sp, ep, tp->lb, tp->len);
		}
		msg_rpt(sp, sp->stdfp);

		if (!F_ISSET(sp, S_MODE_EX) || F_ISSET(sp, S_MAJOR_CHANGE))
			break;

		if (sp->s_refresh(sp, ep)) {
			eval = 1;
			break;
		}
	}
ret:	return (ex_end(sp) || eval);
}

/*
 * ex_cfile --
 *	Execute ex commands from a file.
 */
int
ex_cfile(sp, ep, filename)
	SCR *sp;
	EXF *ep;
	char *filename;
{
	struct stat sb;
	int fd, len, rval;
	char *bp;

	if ((fd = open(filename, O_RDONLY, 0)) < 0)
		goto e1;
	if (fstat(fd, &sb))
		goto e2;

	/*
	 * XXX
	 * We'd like to test if the file is too big to malloc.  Since we don't
	 * know what size or type off_t's or size_t's are, what the largest
	 * unsigned integral type is, or what random insanity the local C
	 * compiler will perpetrate, doing the comparison in a portable way
	 * is flatly impossible.  Hope that malloc fails if the file is too
	 * large.
	 */
	if ((bp = malloc((size_t)sb.st_size)) == NULL)
		goto e2;

	len = read(fd, bp, (int)sb.st_size);
	if (len == -1 || len != sb.st_size) {
		if (len != sb.st_size)
			errno = EIO;
		goto e3;
	}
	bp[sb.st_size] = '\0';

	rval = ex_cstring(sp, ep, bp, len);
	/*
	 * XXX
	 * THE UNDERLYING EXF MAY HAVE CHANGED (but we don't care).
	 */
	free(bp);
	(void)close(fd);
	return (rval);

e3:	free(bp);
e2:	(void)close(fd);
e1:	msgq(sp, M_ERR, "%s: %s.", filename, strerror(errno));
	return (1);
}

/*
 * ex_cstring --
 *	Execute EX commands from a string.
 */
int
ex_cstring(sp, ep, cmd, len)
	SCR *sp;
	EXF *ep;
	char *cmd;
	int len;
{
	u_int saved_mode;
	int ch, arg1_len;
	char *p, *t, *arg1;

	/*
	 * Ex goes through here for each vi :colon command and for each ex
	 * command, however, globally executed commands don't go through
	 * here, instead, they call ex_cmd directly.  So, reset all of the
	 * interruptible flags now.
	 */
	F_CLR(sp, S_INTERRUPTED | S_INTERRUPTIBLE);

	/* This is probably not necessary, but it's worth being safe. */
	if (len == 0)
		return (0);

	/*
	 * QUOTING NOTE:
	 *
	 * Commands may be separated by newline or '|' characters, and may
	 * be escaped by literal next characters.  The quote characters are
	 * stripped here since they are no longer useful.
	 *
	 * There are two exceptions to this, the two commands that take
	 * ex commands are arguments (ex and edit), and the three commands
	 * that take RE patterns (global, vglobal and substitute).  These
	 * commands want their first argument be specially delimited, not
	 * necessarily by '|' characters.  See ep_comm() and ep_re() below
	 * for details.
	 */
	arg1 = NULL;
	arg1_len = 0;
	saved_mode = F_ISSET(sp, S_MODE_EX | S_MODE_VI | S_MAJOR_CHANGE);
	for (p = t = cmd;;) {
		if (p == cmd) {
			/* Skip leading whitespace. */
			for (; len > 0 && (isblank(t[0]) || t[0] == '|' ||
			    sp->special[t[0]] == K_VLNEXT); ++t, --len);

			/* Special case ex/edit, RE commands. */
			if (len > 0 &&
			    strchr("egvs", t[0] == ':' ? t[1] : t[0]))
				if (t[0] == ':' && t[1] == 'e' || t[0] == 'e')
					ep_comm(sp,
					    &p, &t, &len, &arg1, &arg1_len);
				else
					ep_re(sp, &p, &t, &len);
			if (len == 0)
				goto cend;
		}

		ch = *t++;
		--len;			/* Characters remaining. */

		/*
		 * Historically, vi permitted ^V's to escape <newline>'s in
		 * the .exrc file.  It was almost certainly a bug, but that's
		 * what bug-for-bug compatibility means, Grasshopper.
		 */
		if (sp->special[ch] == K_VLNEXT && len > 0 &&
		   (t[0] == '|' || sp->special[t[0]] == K_NL)) {
			--len;
			*p++ = *t++;
			continue;
		}
		if (len == 0 || ch == '|' || sp->special[ch] == K_NL) {
			if (len == 0 && ch != '|' && sp->special[ch] != K_NL)
				*p++ = ch;
cend:			if (p > cmd) {
				*p = '\0';	/* XXX: 8BIT */
				if (ex_cmd(sp, ep, cmd, arg1_len)) {
					if (len || TERM_KEY_MORE(sp)) {
						TERM_KEY_FLUSH(sp);
						msgq(sp, M_ERR,
		    "Ex command failed, remaining command input discarded.");
					}
					return (1);
				}
				p = cmd;
			}
			if (saved_mode !=
			    F_ISSET(sp, S_MODE_EX | S_MODE_VI | S_MAJOR_CHANGE))
				break;
			if (len == 0)
				return (0);
		} else
			*p++ = ch;
	}
	/*
	 * Only here if the mode of the underlying file changed, the user
	 * switched files or is exiting.  There are two things that we may
	 * have to save.  First, any "+cmd" field that ep_comm() set up will
	 * have to be saved for later.  Also, there may be part of the
	 * current ex command which we haven't executed:
	 *
	 *	:edit +25 file.c|s/abc/ABC/|1
	 *
	 * for example.
	 *
	 * The historic vi just hung, of course; we handle by pushing the
	 * keys onto the SCR's tty buffer.  If we're switching modes to
	 * vi, since the commands are intended as ex commands, add the extra
	 * characters to make it work.
	 *
	 * For the fun of it, if you want to see if a vi clone got the ex
	 * argument parsing right, try:
	 *
	 *	echo 'foo|bar' > file1; echo 'foo/bar' > file2;
	 *	vi
	 *	:edit +1|s/|/PIPE/|w file1| e file2|1 | s/\//SLASH/|wq
	 */
	if (arg1 == NULL && len == 0)
		return (0);
	if (F_ISSET(sp, S_MODE_VI) && term_push(sp, sp->gp->tty, "\n", 1))
		goto err;
	if (len != 0)
		if (term_push(sp, sp->gp->tty, t, len))
			goto err;
	if (arg1 != NULL) {
		if (F_ISSET(sp, S_MODE_VI) && len != 0 &&
		    term_push(sp, sp->gp->tty, "|", 1))
			goto err;
		if (term_push(sp, sp->gp->tty, arg1, arg1_len))
			goto err;
	}
	if (F_ISSET(sp, S_MODE_VI) && term_push(sp, sp->gp->tty, ":", 1)) {
err:		TERM_KEY_FLUSH(sp);
		msgq(sp, M_ERR, "Error: %s: remaining command input discarded",
		    strerror(errno));
	}
	return (0);
}

/*
 * ex_cmd --
 *	Parse and execute an ex command.  
 */
int
ex_cmd(sp, ep, exc, arg1_len)
	SCR *sp;
	EXF *ep;
	char *exc;
	int arg1_len;
{
	EXCMDARG cmd;
	EXCMDLIST const *cp;
	MARK cur;
	recno_t lcount, lno, num;
	long flagoff;
	u_int saved_mode;
	int ch, cmdlen, esc, flags, uselastcmd;
	char *p, *t, *endp;

#if defined(DEBUG) && 0
	TRACE(sp, "ex: {%s}\n", exc);
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
	for (; isblank(*exc); ++exc);

	/* Initialize the argument structure. */
	memset(&cmd, 0, sizeof(EXCMDARG));
	sp->ex_argv[0] = "";
	sp->ex_argv[1] = NULL;
	cmd.argc = 0;
	cmd.argv = sp->ex_argv;
	cmd.buffer = OOBCB;

	/*
	 * Parse line specifiers if the command uses addresses.
	 * New command line position is returned, or NULL on error.  
	 */
	if ((exc = ep_range(sp, ep, exc, &cmd)) == NULL)
		return (1);

	/* Skip whitespace. */
	for (; isblank(*exc); ++exc);

	/*
	 * If no command, ex does the last specified of p, l, or #, and vi
	 * moves to the line.  Otherwise, find out how long the command name
	 * is.  There are a few commands that aren't alphabetic, but they
	 * are all single character commands.
	 */
#define	SINGLE_CHAR_COMMANDS	"!#&<=>@"
	if (*exc) {
		if (strchr(SINGLE_CHAR_COMMANDS, *exc)) {
			p = exc;
			exc++;
			cmdlen = 1;
		} else {
			for (p = exc; isalpha(*exc); ++exc);
			cmdlen = exc - p;
			if (cmdlen == 0) {
				msgq(sp, M_ERR, "Unknown command name.");
				return (1);
			}
		}
		for (cp = cmds; cp->name && memcmp(p, cp->name, cmdlen); ++cp);

		/*
		 * !!!
		 * Historic vi permitted the mark to immediately follow the 'k'
		 * in the 'k' command.  Make it work.
		 *
		 * Use of msgq below is safe, command names are all alphabetics.
		 */
		if (cp->name == NULL)
			if (p[0] == 'k' && p[1] && !p[2]) {
				exc = p + 1;
				cp = &cmds[C_K];
			} else {
				msgq(sp, M_ERR,
				    "The %.*s command is unknown.", cmdlen, p);
				return (1);
			}
		uselastcmd = 0;

		/* Some commands are turned off. */
		if (F_ISSET(cp, E_NOPERM)) {
			msgq(sp, M_ERR,
			    "The %.*s command is not currently supported.",
			    cmdlen, p);
			return (1);
		}

		/* Some commands aren't okay in globals. */
		if (F_ISSET(sp, S_GLOBAL) && F_ISSET(cp, E_NOGLOBAL)) {
			msgq(sp, M_ERR,
"The %.*s command can't be used as part of a global command.", cmdlen, p);
			return (1);
		}

		/*
		 * Multiple < and > characters; another "special" feature.
		 * NOTE: The string may not be nul terminated in this case.
		 */
		if ((cp == &cmds[C_SHIFTL] && *p == '<') ||
		    (cp == &cmds[C_SHIFTR] && *p == '>')) {
			sp->ex_argv[0] = p;
			sp->ex_argv[1] = NULL;
			cmd.argc = 1;
			cmd.argv = sp->ex_argv;
			for (ch = *p, exc = p; *++exc == ch;);
		}

		/*
		 * The visual command has a different syntax when called
		 * from ex than when called from a vi colon command.  FMH.
		 */
		if (cp == &cmds[C_VISUAL_EX] && F_ISSET(sp, S_MODE_VI))
			cp = &cmds[C_VISUAL_VI];
	} else {
		cp = sp->lastcmd;
		uselastcmd = 1;
	}
	LF_INIT(cp->flags);

	/*
	 * File state must be checked throughout this code, because it is
	 * called when reading the .exrc file and similar things.  There's
	 * this little chicken and egg problem -- if we read the file first,
	 * we won't know how to display it.  If we read/set the exrc stuff
	 * first, we can't allow any command that requires file state.
	 * Historic vi generally took the easy way out and dropped core.
 	 */
	if (LF_ISSET(E_NORC) && ep == NULL) {
		msgq(sp, M_ERR,
	"The %s command requires a file to already have been read in.",
		    cp->name);
		return (1);
	}

	/*
	 * Set the default addresses.  It's an error to specify an address for
	 * a command that doesn't take them.  If two addresses are specified
	 * for a command that only takes one, lose the first one.  Two special
	 * cases here, some commands take 0 or 2 addresses.  For most of them
	 * (the E_ADDR2_ALL flag), 0 defaults to the entire file.  For one
	 * (the `!' command, the E_ADDR2_NONE flag), 0 defaults to no lines.
	 *
	 * Also, if the file is empty, some commands want to use an address of
	 * 0, i.e. the entire file is 0 to 0, and the default first address is
	 * 0.  Otherwise, an entire file is 1 to N and the default line is 1.
	 * Note, we also add the E_ZERO flag to the command flags, for the case
	 * where the 0 address is only valid if it's a default address.
	 *
	 * Also, set a flag if we set the default addresses.  Some commands
	 * (ex: z) care if the user specified an address of if we just used
	 * the current cursor.
	 */
	flagoff = 0;
	switch (flags & (E_ADDR1|E_ADDR2|E_ADDR2_ALL|E_ADDR2_NONE)) {
	case E_ADDR1:				/* One address: */
		switch (cmd.addrcnt) {
		case 0:				/* Default cursor/empty file. */
			cmd.addrcnt = 1;
			F_SET(&cmd, E_ADDRDEF);
			if (LF_ISSET(E_ZERODEF)) {
				if (file_lline(sp, ep, &lno))
					return (1);
				if (lno == 0) {
					cmd.addr1.lno = 0;
					LF_SET(E_ZERO);
				} else
					cmd.addr1.lno = sp->lno;
			} else
				cmd.addr1.lno = sp->lno;
			cmd.addr1.cno = sp->cno;
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
		if (cmd.addrcnt == 0) {		/* Default entire/empty file. */
			cmd.addrcnt = 2;
			F_SET(&cmd, E_ADDRDEF);
			if (file_lline(sp, ep, &cmd.addr2.lno))
				return (1);
			if (LF_ISSET(E_ZERODEF) && cmd.addr2.lno == 0) {
				cmd.addr1.lno = 0;
				LF_SET(E_ZERO);
			} else
				cmd.addr1.lno = 1;
			cmd.addr1.cno = cmd.addr2.cno = 0;
			F_SET(&cmd, E_ADDR2_ALL);
			break;
		}
		/* FALLTHROUGH */
	case E_ADDR2:				/* Two addresses: */
two:		switch (cmd.addrcnt) {
		case 0:				/* Default cursor/empty file. */
			cmd.addrcnt = 2;
			F_SET(&cmd, E_ADDRDEF);
			if (LF_ISSET(E_ZERODEF) && sp->lno == 1) {
				if (file_lline(sp, ep, &lno))
					return (1);
				if (lno == 0) {
					cmd.addr1.lno = cmd.addr2.lno = 0;
					LF_SET(E_ZERO);
				} else 
					cmd.addr1.lno = cmd.addr2.lno = sp->lno;
			} else
				cmd.addr1.lno = cmd.addr2.lno = sp->lno;
			cmd.addr1.cno = cmd.addr2.cno = sp->cno;
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
	 * YASC.  The "set tags" command historically used a backslash, not
	 * the user's literal next character, to escape whitespace.  Handle
	 * it here instead of complicating the word_argv() code.  Note, this
	 * isn't a particularly complex trap, and if backslashes were legal
	 * in set commands, this would have to be much more complicated.
	 */
	if (cp == &cmds[C_SET]) {
		esc = sp->gp->original_termios.c_cc[VLNEXT];
		for (p = exc; (ch = *p) != '\0'; ++p)
			if (ch == '\\')
				*p = esc;
	}
		
	for (lcount = 0, p = cp->syntax; *p != '\0'; ++p) {
		/*
		 * The read and write commands are sensitive to leading
		 * whitespace, i.e. "write !" is different from "write!".
		 * If not read or write, skip leading whitespace.
		 */
		if (cp != &cmds[C_READ] && cp != &cmds[C_WRITE])
			for (; isblank(*exc); ++exc);

		/*
		 * When * reach the end of the command, quit, unless it's
		 * a command that does its own parsing, in which case we
		 * want to build a reasonable argv for it.
		 */
		if (*p != 's' && *exc == '\0')
			break;

		switch (*p) {
		case '!':				/* ! */
			if (*exc == '!') {
				++exc;
				F_SET(&cmd, E_FORCE);
			}
			break;
		case '+':				/* +cmd */
			if (*exc != '+')
				break;
			exc += arg1_len + 1;
			if (*exc)
				*exc++ = '\0';
			break;
		case '1':				/* +, -, #, l, p */
			/*
			 * !!!
			 * Historically, some flags were ignored depending
			 * on where they occurred in the command line.  For
			 * example, in the command, ":3+++p--#", historic vi
			 * acted on the '#' flag, but ignored the '-' flags.
			 * It's unambiguous what the flags mean, so we just
			 * handle them regardless of the stupidity of their
			 * location.
			 */
			for (;; ++exc)
				switch (*exc) {
				case '+':
					++flagoff;
					break;
				case '-':
					--flagoff;
					break;
				case '#':
					F_SET(&cmd, E_F_HASH);
					break;
				case 'l':
					F_SET(&cmd, E_F_LIST);
					break;
				case 'p':
					F_SET(&cmd, E_F_PRINT);
					break;
				default:
					goto end1;
				}
end1:			break;
		case '2':				/* -, ., +, ^ */
		case '3':				/* -, ., +, ^, = */
			for (;; ++exc)
				switch (*exc) {
				case '-':
					F_SET(&cmd, E_F_DASH);
					break;
				case '.':
					F_SET(&cmd, E_F_DOT);
					break;
				case '+':
					F_SET(&cmd, E_F_PLUS);
					break;
				case '^':
					F_SET(&cmd, E_F_CARAT);
					break;
				case '=':
					if (*p == '3') {
						F_SET(&cmd, E_F_EQUAL);
						break;
					}
					/* FALLTHROUGH */
				default:
					goto end2;
				}
end2:			break;
		case 'b':				/* buffer */
			cmd.buffer = *exc++;
			break;
		case 'C':				/* count */
		case 'c':				/* count (address) */
			if (!isdigit(*exc))
				break;
			lcount = strtol(exc, &endp, 10);
			if (lcount == 0) {
				msgq(sp, M_ERR,
				    "Count may not be zero.");
				return (1);
			}
			exc = endp;
			/*
			 * Count as address offsets occur in commands taking
			 * two addresses.  Historic vi practice was to use
			 * the count as an offset from the *second* address.
			 *
			 * Set the count flag; some underlying commands (see
			 * join) do different things with counts than with
			 * line addresses.
			 */
			if (*p == 'C')
				cmd.count = lcount;
			else {
				cmd.addr1 = cmd.addr2;
				cmd.addr2.lno = cmd.addr1.lno + lcount - 1;
			}
			F_SET(&cmd, E_COUNT);
			break;
		case 'f':				/* file */
			if (file_argv(sp, ep, exc, &cmd.argc, &cmd.argv))
				return (1);
			goto countchk;
		case 'l':				/* line */
			endp = ep_line(sp, ep, exc, &cur);
			if (endp == NULL || exc == endp) {
				msgq(sp, M_ERR, 
				     "%s: bad line specification", exc);
				return (1);
			} else {
				cmd.lineno = cur.lno;
				exc = endp;
			}
			break;
		case 's':				/* string */
			sp->ex_argv[0] = exc;
			sp->ex_argv[1] = NULL;
			cmd.argc = 1;
			cmd.argv = sp->ex_argv;
			goto addr2;
		case 'W':				/* word string */
			/*
			 * QUOTING NOTE:
			 *
			 * Literal next characters escape the following
			 * character.  The quote characters are stripped
			 * here since they are no longer useful.
			 *
			 * Word.
			 */
			for (p = t = exc; (ch = *p) != '\0'; *t++ = ch, ++p)
				if (sp->special[ch] == K_VLNEXT && p[1] != '\0')
					ch = *++p;
				else if (isblank(ch))
					break;
			if (*p == '\0')
				goto usage;
			sp->ex_argv[0] = exc;

			/* Delete leading whitespace. */
			for (*t++ = '\0'; (ch = *++p) != '\0' && isblank(ch););

			/* String. */
			sp->ex_argv[1] = p;
			for (t = p; (ch = *p++) != '\0'; *t++ = ch)
				if (sp->special[ch] == K_VLNEXT && p[0] != '\0')
					ch = *p++;
			*t = '\0';
			sp->ex_argv[2] = NULL;
			cmd.argc = 2;
			cmd.argv = sp->ex_argv;
			goto addr2;
		case 'w':				/* word */
			if (word_argv(sp, ep, exc, &cmd.argc, &cmd.argv))
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
			msgq(sp, M_ERR,
			    "Internal syntax table error (%s).", cp->name);
		}
	}

	/*
	 * Shouldn't be anything left, and no more required fields.
	 * That means neither 'l' or 'r' in the syntax.
	 */
	for (; *exc && isblank(*exc); ++exc);
	if (*exc || strpbrk(p, "lr")) {
usage:		msgq(sp, M_ERR, "Usage: %s.", cp->usage);
		return (1);
	}

	/* Verify that the addresses are legal. */
addr2:	switch (cmd.addrcnt) {
	case 2:
		if (file_lline(sp, ep, &lcount))
			return (1);
		/*
		 * Historic ex/vi permitted commands with counts to go past
		 * EOF.  So, for example, if the file only had 5 lines, the
		 * ex command "1,6>" would fail, but the command ">300"
		 * would succeed.  Since we don't want to have to make all
		 * of the underlying commands handle random line numbers,
		 * fix it here.
		 */
		if (cmd.addr2.lno > lcount)
			if (F_ISSET(&cmd, E_COUNT))
				cmd.addr2.lno = lcount;
			else {
				if (lcount == 0)
					msgq(sp, M_ERR, "The file is empty.");
				else
					msgq(sp, M_ERR,
					    "Only %lu line%s in the file",
					    lcount, lcount > 1 ? "s" : "");
				return (1);
			}
		/* FALLTHROUGH */
	case 1:
		num = cmd.addr1.lno;
		/*
		 * If it's a "default vi command", zero is okay.  Historic
		 * vi allowed this, note, it's also the hack that allows
		 * "vi + nonexistent_file" to work.
		 */
		if (num == 0 && (!F_ISSET(sp, S_MODE_VI) || uselastcmd != 1) &&
		    !LF_ISSET(E_ZERO)) {
			msgq(sp, M_ERR,
			    "The %s command doesn't permit an address of 0.",
			    cp->name);
			return (1);
		}
		if (file_lline(sp, ep, &lcount))
			return (1);
		if (num > lcount) {
			if (lcount == 0)
				msgq(sp, M_ERR, "The file is empty.");
			else
				msgq(sp, M_ERR, "Only %lu line%s in the file",
				    lcount, lcount > 1 ? "s" : "");
			return (1);
		}
		break;
	}

	/* If doing a default command, vi just moves to the line. */
	if (F_ISSET(sp, S_MODE_VI) && uselastcmd) {
		switch (cmd.addrcnt) {
		case 2:
			sp->lno = cmd.addr2.lno ? cmd.addr2.lno : 1;
			sp->cno = cmd.addr2.cno;
			break;
		case 1:
			sp->lno = cmd.addr1.lno ? cmd.addr1.lno : 1;
			sp->cno = cmd.addr1.cno;
			break;
		}
		return (0);
	}

	/* Reset "last" command. */
	if (LF_ISSET(E_SETLAST))
		sp->lastcmd = cp;

	cmd.cmd = cp;
#if defined(DEBUG) && 0
{
	int __cnt;

	TRACE(sp, "ex_cmd: %s", cmd.cmd->name);
	if (cmd.addrcnt > 0) {
		TRACE(sp, "\taddr1 %d", cmd.addr1.lno);
		if (cmd.addrcnt > 1)
			TRACE(sp, " addr2: %d", cmd.addr2.lno);
		TRACE(sp, "\n");
	}
	if (cmd.lineno)
		TRACE(sp, "\tlineno %d", cmd.lineno);
	if (cmd.flags)
		TRACE(sp, "\tflags %0x", cmd.flags);
	if (cmd.buffer != OOBCB)
		TRACE(sp, "\tbuffer %c", cmd.buffer);
	TRACE(sp, "\n");
	if (cmd.argc) {
		for (__cnt = 0; __cnt < cmd.argc; ++__cnt)
			TRACE(sp, "\targ %d: {%s}", __cnt, cmd.argv[__cnt]);
		TRACE(sp, "\n");
	}
}
#endif
	/* Clear autoprint. */
	F_CLR(sp, S_AUTOPRINT);

	/*
	 * If file state and not doing a global command, log the start of
	 * an action.
	 */
	if (ep != NULL && !F_ISSET(sp, S_GLOBAL))
		(void)log_cursor(sp, ep);

	/* Save the current mode. */
	saved_mode = F_ISSET(sp, S_MODE_EX | S_MODE_VI | S_MAJOR_CHANGE);

	/* Increment the command count if not called from vi. */
	if (!F_ISSET(sp, S_MODE_VI))
		++sp->ccnt;

	/* Do the command. */
	if ((cp->fn)(sp, ep, &cmd))
		return (1);

#ifdef DEBUG
	/* Make sure no function left the temporary space locked. */
	if (F_ISSET(sp->gp, G_TMP_INUSE))
		abort();
#endif

	/* If the world changed, we're done. */
	if (saved_mode != F_ISSET(sp, S_MODE_EX | S_MODE_VI | S_MAJOR_CHANGE))
		return (0);

	/* If just starting up, or not in ex mode, we're done. */
	if (ep == NULL || !F_ISSET(sp, S_MODE_EX))
		return (0);

	/*
	 * The print commands have already handled the `print' flags.
	 * If so, clear them.  Don't return, autoprint may still have
	 * stuff to print out.
	 */
	 if (LF_ISSET(E_F_PRCLEAR))
		F_CLR(&cmd, E_F_HASH | E_F_LIST | E_F_PRINT);

	/*
	 * If the command was successful, and there was an explicit flag to
	 * display the new cursor line, or we're in ex mode, autoprint is set,
	 * and a change was made, display the line.
	 */
	if (flagoff) {
		if (flagoff < 0) {
			if (sp->lno < -flagoff) {
				msgq(sp, M_ERR, "Flag offset before line 1.");
				return (1);
			}
		} else {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (sp->lno + flagoff > lno) {
				msgq(sp, M_ERR,
				    "Flag offset past end-of-file.");
				return (1);
			}
		}
		sp->lno += flagoff;
	}

	if (F_ISSET(sp, S_AUTOPRINT) && O_ISSET(sp, O_AUTOPRINT))
		LF_INIT(E_F_PRINT);
	else
		LF_INIT(F_ISSET(&cmd, E_F_HASH | E_F_LIST | E_F_PRINT));

	memset(&cmd, 0, sizeof(EXCMDARG));
	cmd.addrcnt = 2;
	cmd.addr1.lno = cmd.addr2.lno = sp->lno;
	cmd.addr1.cno = cmd.addr2.cno = sp->cno;
	if (flags) {
		switch (flags) {
		case E_F_HASH:
			cmd.cmd = &cmds[C_HASH];
			ex_number(sp, ep, &cmd);
			break;
		case E_F_LIST:
			cmd.cmd = &cmds[C_LIST];
			ex_list(sp, ep, &cmd);
			break;
		case E_F_PRINT:
			cmd.cmd = &cmds[C_PRINT];
			ex_pr(sp, ep, &cmd);
			break;
		}
	}
	return (0);
}

/*
 * ep_comm, ep_re --
 *
 * Historically, '|' characters in the first argument of the ex, edit,
 * global, vglobal and substitute commands did not separate commands.
 * For example, the following two commands were legal:
 *
 *	:edit +25|s/abc/ABC/ file.c
 *	:substitute s/|/PIPE/
 *
 * It's not quite as simple as it looks, however.  The command:
 *
 *	:substitute s/a/b/|s/c/d|set
 *
 * was also legal, i.e. the historic ex parser (and I use the word loosely,
 * since "parser" must imply some regularity of syntax) delimited the RE's
 * based on its delimiter and not anything so vulgar as a command syntax.
 *
 * The ep_comm() and ep_re() routines make this work.  They are passed the
 * state of ex_cstring(), and, if it's a special case, they parse the first
 * argument and then return the new state.  For the +cmd field, since we
 * don't want to parse the line more than once, a pointer to and the length
 * of the first argument is returned to ex_cstring(), which will subsequently
 * pass it into ex_cmd().  Barf-O-Rama.
 */
static void
ep_comm(sp, pp, tp, lenp, arg1p, arg1_lenp)
	SCR *sp;
	char **pp, **tp, **arg1p;
	int *lenp, *arg1_lenp;
{
	char *cp, *p, *t;
	int ch, cnt, len;

	/* Copy the state to local variables. */
	p = *pp;
	t = *tp;
	len = *lenp;

	/*
	 * Move to the next non-lower-case, alphabetic character.  We can
	 * do this fairly brutally because we know that the command names
	 * are all lower-case alphabetics, and there has to be a '+' to
	 * start the arguments.  If there isn't one, we're done.
	 */
	if (*p == ':') {
		*p++ = *t++;
		--len;
	}
	for (; len && islower(*p = *t); ++p, ++t, --len);
	if (len == 0)
		goto ret;

	/* Make sure it's the ex or edit command. */
	cp = *pp;
	if (*cp == ':') {
		++cp;
		cnt = (p - cp) - 1;
	} else
		cnt = (p - cp);
	if (strncmp(cp, "ex", cnt) && strncmp(cp, "edit", cnt))
		goto ret;

	/*
	 * Move to the '+'.  We don't check syntax here, if it's not
	 * there, we're done.
	 */
	while (len--) {
		ch = *++p = *++t;
		if (!isblank(ch))
			break;
	}
	if (len == 0 || *p != '+')
		goto ret;

	/*
	 * QUOTING NOTE:
	 *
	 * The historic implementation of this "feature" ignored any escape
	 * characters so there was no way to put a space or newline into the
	 * +cmd field.  We do a simplistic job of handling it by moving to
	 * the first whitespace character that isn't escaped by a literal next
	 * character.  The literal next quote characters are stripped here
	 * since they are no longer useful.
	 *
	 * Move to the first non-escaped space.
	 */
	for (cp = p; len;) {
		ch = *++p = *++t;
		--len;
		if (sp->special[ch] == K_VLNEXT && len > 0) {
			*p = *++t;
			if (--len == 0)
				break;
		} else if (isblank(ch))
			break;
	}

	/* Return information about the first argument. */
	*arg1p = cp + 1;
	*arg1_lenp = (p - cp) - 1;

	/* Restore the state. */
ret:	*pp = p;
	*tp = t;
	*lenp = len;
}

static void
ep_re(sp, pp, tp, lenp)
	SCR *sp;
	char **pp, **tp;
	int *lenp;
{
	char *cp, *p, *t;
	int ch, cnt, delim, len;

	/* Copy the state to local variables. */
	p = *pp;
	t = *tp;
	len = *lenp;

	/*
	 * Move to the next non-lower-case, alphabetic character.  We can
	 * do this fairly brutally because we know that the command names
	 * are all lower-case alphabetics, and there has to be a delimiter
	 * to start the arguments.  If there isn't one, we're done.
	 */
	if (*t == ':') {
		*p++ = *t++;
		--len;
	}
	for (; len; ++p, ++t, --len) {
		ch = *p = *t;
		if (!islower(ch))
			break;
	}
	if (len == 0)
		goto ret;

	/* Make sure it's the substitute, global or vglobal command. */
	cp = *pp;
	if (*cp == ':') {
		++cp;
		cnt = (p - cp) - 1;
	} else
		cnt = (p - cp);
	if (strncmp(cp, "substitute", cnt) &&
	    strncmp(cp, "global", cnt) && strncmp(cp, "vglobal", cnt))
		goto ret;

	/*
	 * Move to the delimiter.  (The first character; if it's an illegal
	 * one, the RE code will catch it.)
	 */
	if (isblank(ch))
		while (len--) {
			ch = *++p = *++t;
			if (!isblank(ch))
				break;
		}
	delim = ch;
	if (len == 0)
		goto ret;

	/*
	 * QUOTING NOTE:
	 *
	 * Backslashes quote delimiter characters for regular expressions.
	 * The backslashes are left here since they'll be needed by the RE
	 * code.
	 *
	 * Move to the third (non-escaped) delimiter.
	 */
	for (cnt = 2; len && cnt;) {
		ch = *++p = *++t;
		--len;
		if (ch == '\\' && len > 0) {
			*++p = *++t;
			if (--len == 0)
				break;
		} else if (ch == delim)
			--cnt;
	}

	/* Move past the delimiter if it's possible. */
	if (len > 0) {
		++t;
		++p;
		--len;
	}

	/* Restore the state. */
ret:	*pp = p;
	*tp = t;
	*lenp = len;
}

/*
 * ep_range --
 *	Get a line range for ex commands.
 */
static char *
ep_range(sp, ep, cmd, cp)
	SCR *sp;
	EXF *ep;
	char *cmd;
	EXCMDARG *cp;
{
	MARK cur, savecursor;
	int savecursor_set;
	char *endp;

	/* Percent character is all lines in the file. */
	if (*cmd == '%') {
		cp->addr1.lno = 1;
		if (file_lline(sp, ep, &cp->addr2.lno))
			return (NULL);
		/* If an empty file, then the first line is 0, not 1. */
		if (cp->addr2.lno == 0)
			cp->addr1.lno = 0;
		cp->addr1.cno = cp->addr2.cno = 0;
		cp->addrcnt = 2;
		return (++cmd);
	}

	/* Parse comma or semi-colon delimited line specs. */
	for (savecursor_set = 0, cp->addrcnt = 0;;)
		switch (*cmd) {
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
				savecursor.lno = sp->lno;
				savecursor.cno = sp->cno;
				sp->lno = cp->addr1.lno;
				sp->cno = cp->addr1.cno;
				savecursor_set = 1;
			}
			/* FALLTHROUGH */
		case ' ':
		case '\t':
		case ',':		/* Comma delimiter. */
			++cmd;
			break;
		default:
			if ((endp = ep_line(sp, ep, cmd, &cur)) == NULL)
				return (NULL);
			if (cmd != endp) {
				cmd = endp;

				/*
				 * Extra addresses are discarded, starting with
				 * the first.
				 */
				switch (cp->addrcnt) {
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
				break;
			}
			/* FALLTHROUGH */
		case '\0':
			goto done;
		}

	/*
	 * XXX
	 * This is probably not right for treatment of savecursor -- figure
	 * out what the historical ex did for ";,;,;5p" or similar stupidity.
	 */
done:	if (savecursor_set) {
		sp->lno = savecursor.lno;
		sp->cno = savecursor.cno;
	}
	if (cp->addrcnt == 2 &&
	    (cp->addr2.lno < cp->addr1.lno ||
	    cp->addr2.lno == cp->addr1.lno && cp->addr2.cno < cp->addr1.cno)) {
		msgq(sp, M_ERR,
		    "The second address is smaller than the first.");
		return (NULL);
	}
	return (cmd);
}

/*
 * Get a single line address specifier.
 */
static char *
ep_line(sp, ep, cmd, cur)
	SCR *sp;
	EXF *ep;
	char *cmd;
	MARK *cur;
{
	MARK m, *mp;
	long num, total;
	u_int flags;
	char *endp;

	for (;;) {
		switch (*cmd) {
		case '$':		/* Last line. */
			if (file_lline(sp, ep, &cur->lno))
				return (NULL);
			cur->cno = 0;
			++cmd;
			break;		/* Absolute line number. */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * The way the vi "previous context" mark worked was
			 * that "non-relative" motions set it.  While vi was
			 * not completely consistent about this, ANY numeric
			 * address was considered non-relative, and set the
			 * value.
			 */
			if (F_ISSET(sp, S_MODE_VI)) {
				m.lno = sp->lno;
				m.cno = sp->cno;
				if (mark_set(sp, ep, ABSMARK1, &m, 1))
					return (NULL);
			}
			cur->lno = strtol(cmd, &endp, 10);
			cur->cno = 0;
			cmd = endp;
			break;
		case '\'':		/* Set mark. */
			if (cmd[1] == '\0') {
				msgq(sp, M_ERR,
				    "No mark name; use 'a' to 'z'.");
				return (NULL);
			}
			if ((mp = mark_get(sp, ep, cmd[1])) == NULL)
				return (NULL);
			*cur = *mp;
			cmd += 2;
			break;
		case '/':		/* Search forward. */
			m.lno = sp->lno;
			m.cno = sp->cno;
			flags = SEARCH_MSG | SEARCH_PARSE | SEARCH_SET;
			if (f_search(sp, ep, &m, &m, cmd, &endp, &flags))
				return (NULL);
			cur->lno = m.lno;
			cur->cno = m.cno;
			cmd = endp;
			break;
		case '?':		/* Search backward. */
			m.lno = sp->lno;
			m.cno = sp->cno;
			flags = SEARCH_MSG | SEARCH_PARSE | SEARCH_SET;
			if (b_search(sp, ep, &m, &m, cmd, &endp, &flags))
				return (NULL);
			cur->lno = m.lno;
			cur->cno = m.cno;
			cmd = endp;
			break;
		case '.':		/* Current position. */
			++cmd;
			/* FALLTHROUGH */
		case '+':		/* Increment. */
		case '-':		/* Decrement. */
			/* If an empty file, then '.' is 0, not 1. */
			if (sp->lno == 1) {
				if (file_lline(sp, ep, &cur->lno))
					return (NULL);
				if (cur->lno != 0)
					cur->lno = 1;
			} else
				cur->lno = sp->lno;
			cur->cno = sp->cno;
			break;
		default:
			return (cmd);
		}

		/*
		 * Evaluate any offset.  Offsets are +/- any number, or,
		 * any number of +/- signs, or any combination thereof.
		 */
		for (total = 0; *cmd == '-' || *cmd == '+'; total += num) {
			num = *cmd == '-' ? -1 : 1;
			if (isdigit(*++cmd)) {
				num *= strtol(cmd, &endp, 10);
				cmd = endp;
			}
		}
		if (total < 0 && -total > cur->lno) {
			msgq(sp, M_ERR,
			    "Reference to a line number less than 0.");
			return (NULL);
		}
		cur->lno += total;
	}
}
