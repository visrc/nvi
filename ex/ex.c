/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex.c,v 10.1 1995/04/13 17:21:48 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:21:48 $";
#endif /* not lint */

#include <sys/types.h>
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

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"

#if defined(DEBUG) && defined(COMLOG)
static void	ex_comlog __P((SCR *, EXCMD *));
#endif
static EXCMDLIST const *
		ex_comm_search __P((char *, size_t));
static int	ex_gat __P((SCR *, EXCMD *));
static int	ex_gatfree __P((SCR *));
static int	ex_line __P((SCR *, EXCMD *, int *));
static void	ex_unknown __P((SCR *, char *, size_t));

/*
 * ex --
 *	Ex event handler.
 */
int
ex(sp, evp)
	SCR *sp;
	EVENT *evp;
{
	EX_PRIVATE *exp;
	GS *gp;
	TEXT *tp;
	int complete, notused;

	/* Local variables. */
	gp = sp->gp;
	exp = EXP(sp);

	switch (evp->e_event) {			/* !E_CHARACTER events. */
	case E_CHARACTER:
		break;
	case E_INTERRUPT:
		/* Interrupts get passed on to other handlers. */
		if (gp->cm_state == ES_RUNNING)
			break;

		/* Command mode displays a message. */
		goto interrupt;
	case E_SIGCONT:
		/* If in text input, continue works like an interrupt. */
		if (gp->cm_state == ES_RUNNING &&
		    gp->cm_next == ES_CTEXT_TEARDOWN)
			goto cmd;
		return (0);
	case E_START:
		sp->rows = O_VAL(sp, O_LINES);
		sp->cols = O_VAL(sp, O_COLUMNS);

		/* If reading from a file, messages should have line info. */
		if (!F_ISSET(gp, G_STDIN_TTY)) {
			sp->if_lno = 0;
			sp->if_name = strdup("input");
		}

		gp->cm_state = ES_GET_CMD;
		break;
	case E_RESIZE:
		sp->rows = O_VAL(sp, O_LINES);
		sp->cols = O_VAL(sp, O_COLUMNS);
		return (0);
	case E_EOF:
	case E_ERR:
	case E_REPAINT:
	case E_STOP:
		return (0);
	case E_TIMEOUT:
		break;
	default:
		abort();
		/* NOTREACHED */
	}

next_state:
	switch (gp->cm_state) {		/* E_CHARACTER events. */
	/*
	 * ES_RUNNING:
	 *
	 * Something else is running -- a search loop or a text input loop,
	 * and wanted to check for interrupts, or a file switched and the
	 * main code is the only thing that can handle that.  Pass the event
	 * on to the handler.  If the handler finishes, move to the next state.
	 */
	case ES_RUNNING:
		if (exp->run_func != NULL) {
			if (exp->run_func(sp, evp, &complete))
				return (1);
			if (!complete)
				return (0);
		}
		FL_CLR(gp->ec_flags, EC_INTERRUPT);

		if (evp->e_event == E_INTERRUPT) {
interrupt:		(void)ex_printf(EXCOOKIE, "\n");
			msgq(sp, M_ERR, v_event_flush(sp, CH_MAPPED) ?
			    "072|Interrupted: mapped keys discarded" :
			    "134|Interrupted");
			goto cmd;
		}

		gp->cm_state = gp->cm_next;
		goto next_state;

	/*
	 * ES_GET_CMD:
	 *
	 * Initial command state.
	 */
	case ES_GET_CMD:
cmd:		gp->cm_state = ES_GET_CMD;

		/* Flush everything out to the screen. */
		(void)msg_rpt(sp, 0);
		if (gp->scr_msgflush(sp, &notused))
			return (1);
				
		/* If we're exiting the screen, clean up. */
		switch (F_ISSET(sp, S_VI | S_EXIT | S_EXIT_FORCE | S_SSWITCH)) {
		case S_VI:
			gp->cm_state = ES_PARSE;
			return (0);
		case S_EXIT_FORCE:			/* Exit the file. */
		case S_EXIT:				/* Exit the file. */
			if (file_end(sp, NULL, F_ISSET(sp, S_EXIT_FORCE)))
				break;
			sp->nextdisp = NULL;
			gp->cm_state = ES_PARSE;
			return (0);
		case S_SSWITCH:
			return (0);
		}

		/*
		 * !!!
		 * Set up text flags.  The beautify edit option historically
		 * applied to ex command input read from a file.  In addition,
		 * the first time a ^H was discarded from the input, a message
		 * "^H discarded" was displayed.  We don't bother.
		 */
		FL_INIT(exp->im_flags, TXT_BACKSLASH | TXT_CNTRLD | TXT_CR);
		if (O_ISSET(sp, O_BEAUTIFY))
			FL_SET(exp->im_flags, TXT_BEAUTIFY);
		if (O_ISSET(sp, O_PROMPT))
			FL_SET(exp->im_flags, TXT_PROMPT);

		/* Increment the line count. */
		++sp->if_lno;
		
		if (ex_txt_setup(sp, ':'))
			return (1);
		break;

	/*
	 * ES_ITEXT_TEARDOWN:
	 *
	 * We have just ended text input mode.  Tear down the setup and
	 * finish the command.
	 */
	case ES_ITEXT_TEARDOWN:
		if (ex_aci_td(sp))
			return (1);
		gp->cm_state = ES_PARSE_FUNC;
		goto parse;

	/*
	 * ES_CTEXT_TEARDOWN:
	 *
	 * We have just ended command text input mode.  Tear down the setup
	 * and execute the command.
	 */
	 case ES_CTEXT_TEARDOWN:
		/*
		 * If the user entered a carriage return, send ex_cmd() a
		 * space -- it discards single newlines.
		 */
		tp = exp->im_tiq.cqh_first;
		if (tp->len == 0) {
			tp->len = 1;
			tp->lb[0] = ' ';	/* __TK__ why not '|'? */
		}

		gp->excmd.cp = tp->lb;
		gp->excmd.cplen = tp->len;
		F_INIT(&gp->excmd, E_NEEDSEP);
		gp->cm_state = ES_PARSE;
		goto parse;

	/*
	 * ES_PARSE_EXIT, ES_PARSE_FUNC, ES_PARSE_LINE, ES_PARSE_RANGE:
	 *
	 * Restart the parser at some point.
	 */
	case ES_PARSE_EXIT:
		FL_CLR(gp->ec_flags, EC_INTERRUPT);
		/* FALLTHROUGH */

	case ES_PARSE_FUNC:
	case ES_PARSE_LINE:
	case ES_PARSE_RANGE:
		goto parse;

	/*
	 * ES_PARSE:
	 *
	 * Start the parser.
	 */
	case ES_PARSE:
parse:		if (ex_cmd(sp))
			return (1);
		if (gp->cm_state != ES_PARSE)
			break;
		goto cmd;
	default:
		abort();
		/* NOTREACHED */
	}
	return (0);
}

/*
 * ex_cmd --
 *	The guts of the ex parser: parse and execute a string containing
 *	ex commands.
 *
 * !!!
 * This code MODIFIES the string that gets passed in, to delete quoting
 * characters, etc.  The string cannot be readonly/text space, nor should
 * you expect to use it again after ex_cmd() returns.
 *
 * !!!
 * For the fun of it, if you want to see if a vi clone got the ex argument
 * parsing right, try:
 *
 *	echo 'foo|bar' > file1; echo 'foo/bar' > file2;
 *	vi
 *	:edit +1|s/|/PIPE/|w file1| e file2|1 | s/\//SLASH/|wq
 *
 * For extra credit, try it in a startup .exrc file.
 */
int
ex_cmd(sp)
	SCR *sp;
{
	enum nresult nret;
	EX_PRIVATE *exp;
	EXCMD *cmdp;
	GS *gp;
	MARK cur;
	recno_t lno;
	size_t len;
	long ltmp;
	int ch, cnt, delim, isaddr, gcnt, namelen;
	int newscreen, nf, notempty, tmp, vi_address;
	char *p, *s, *t;

	gp = sp->gp;
	exp = EXP(sp);
	cmdp = &gp->excmd;

	switch (gp->cm_state) {
	/*
	 * ES_PARSE:
	 *
	 * Starting to parse a new command, do setup and start.
	 */
	case ES_PARSE:
		cmdp->sep = F_ISSET(cmdp, E_NEEDSEP) ? SEP_NOTSET : SEP_NONE;
		break;

	/* 
	 * ES_PARSE_EXIT:
	 *
	 * Restarting after a screen change.
	 */
	case ES_PARSE_EXIT:
		gp->cm_state = ES_PARSE;
		break;
	
	/*
	 * ES_PARSE_FUNC:
	 *
	 * Restarting from the underlying function.
	 */
	case ES_PARSE_FUNC:
		gp->cm_state = ES_PARSE;
		goto func_restart;

	/*
	 * ES_PARSE_LINE:
	 *
	 * Restarting from the line routine.
	 */
	case ES_PARSE_LINE:
		if (ex_line(sp, cmdp, &isaddr))
			return (1);
		switch (gp->cm_state) {
		case ES_PARSE:
			goto line_restart;
		case ES_PARSE_ERROR:
			goto err;
		case ES_RUNNING:
			gp->cm_next = ES_PARSE_LINE;
			return (0);
		default:
			abort();
		}
		/* NOTREACHED */

	/*
	 * ES_PARSE_RANGE:
	 *
	 * Restarting from the range routine.
	 */
	case ES_PARSE_RANGE:
		if (ex_range(sp, cmdp))
			return (1);
		switch (gp->cm_state) {
		case ES_PARSE:
			goto range_restart;
		case ES_PARSE_ERROR:
			goto err;
		case ES_RUNNING:
			gp->cm_next = ES_PARSE_RANGE;
			return (0);
		default:
			abort();
		}
		/* NOTREACHED */

	default:
		abort();
	}

	/* Global interrupt check count. */
	gcnt = INTERRUPT_CHECK;

loop:	/*
	 * If a move to the end of the file is scheduled for this command,
	 * do it now.
	 */
	if (F_ISSET(cmdp, E_MOVETOEND)) {
		 if (file_lline(sp, &sp->lno))
			return (1);
		 sp->cno = 0;
		 F_CLR(cmdp, E_MOVETOEND);
	}

	/* If we found a newline, increment the count now. */
	if (F_ISSET(cmdp, E_NEWLINE)) {
		F_CLR(cmdp, E_NEWLINE);
		++sp->if_lno;
	}

	/*
	 * Initialize the EXCMD structure.  Only the current command
	 * fields are saved.
	 */
	p = cmdp->cp;
	len = cmdp->cplen;
	memset(cmdp, 0, sizeof(EXCMD));
	cmdp->cp = p;
	cmdp->cplen = len;

	/* Initialize the argument structures. */
	if (argv_init(sp, cmdp))
		goto err;

	/* Initialize +cmd, saved command information. */
	cmdp->arg1 = NULL;
	cmdp->save_cmdlen = 0;

	/* Skip <blank>s, empty lines.  */
	for (notempty = 0; cmdp->cplen > 0; ++cmdp->cp, --cmdp->cplen)
		if ((ch = *cmdp->cp) == '\n')
			++sp->if_lno;
		else if (isblank(ch))
			notempty = 1;
		else
			break;

	/*
	 * !!!
	 * Permit extra colons at the start of the line.  Historically,
	 * ex/vi allowed a single extra one.  It's simpler not to count.
	 * The stripping is done here because, historically, any command
	 * could have preceding colons, e.g. ":g/pattern/:p" worked.
	 */
	if (cmdp->cplen != 0 && ch == ':') {
		notempty = 1;
		while (--cmdp->cplen > 0 && (ch = *++cmdp->cp) == ':');
	}

	/*
	 * Command lines that start with a double-quote are comments.
	 *
	 * !!!
	 * Historically, there was no escape or delimiter for a comment,
	 * e.g. :"foo|set was a single comment and nothing was output.
	 * Since nvi permits users to escape <newline> characters into
	 * command lines, we have to check for that case.
	 */
	if (cmdp->cplen != 0 && ch == '"') {
		while (--cmdp->cplen > 0 && *++cmdp->cp != '\n');
		if (*cmdp->cp == '\n') {
			F_SET(cmdp, E_NEWLINE);
			++cmdp->cp;
			--cmdp->cplen;
		}
		goto loop;
	}

	/* Skip whitespace. */
	for (; cmdp->cplen > 0; ++cmdp->cp, --cmdp->cplen) {
		ch = *cmdp->cp;
		if (!isblank(ch))
			break;
	}

	/*
	 * The last point at which an empty line can mean do nothing.  It may
	 * not look like it, but here's where we end up when we finish each ex
	 * command.  At which point, if there is an @ buffer or global command,
	 * load them up.
	 *
	 * !!!
	 * Historically, in ex mode, lines containing only <blank> characters
	 * were the same as a single <carriage-return>, i.e. a default command.
	 * In vi mode, they were ignored.  In .exrc files this was a serious
	 * annoyance, as vi kept trying to treat them as print commands.  We
	 * ignore backward compatibility in this case, discarding lines that
	 * contain only <blank> characters from .exrc files.
	 */
	if (cmdp->cplen == 0 &&
	    (!notempty || F_ISSET(sp, S_VI) || F_ISSET(cmdp, E_BLIGNORE))) {
		if (exp->gatq.lh_first != NULL) {
			if (ex_gat(sp, cmdp))
				return (1);
			if (cmdp->cplen != 0)
				goto loop;
		}
		goto ret;
	}

	/*
	 * Check to see if this is a command for which we may want to move
	 * the cursor back up to the previous line.  (The command :1<CR>
	 * wants a <newline> separator, but the command :<CR> wants to erase
	 * the command line.)  If the line is empty except for <blank>s,
	 * <carriage-return> or <eof>, we'll probably want to move up.  I
	 * don't think there's any way to get <blank> characters *after* the
	 * command character, but this is the ex parser, and I've been wrong
	 * before.
	 */
	if (cmdp->sep == NOTSET)
		cmdp->sep = cmdp->cplen == 0 || cmdp->cplen == 1 &&
		    cmdp->cp[0] == '\004' ? SEP_NEED_NR : SEP_NEED_N;

	/*
	 * Parse command addresses.
	 *
	 * If the range contained a search expression, we may have changed
	 * state during the call, and we're now searching the file.  Push
	 * ourselves onto the state stack.
	 */
	if (ex_range(sp, cmdp))
		return (1);
	switch (gp->cm_state) {
	case ES_PARSE:
		break;
	case ES_PARSE_ERROR:
		goto err;
	case ES_RUNNING:
		gp->cm_next = ES_PARSE_RANGE;
		return (0);
	default:
		abort();
	}
range_restart:

	/* Skip whitespace. */
	for (; cmdp->cplen > 0; ++cmdp->cp, --cmdp->cplen) {
		ch = *cmdp->cp;
		if (!isblank(ch))
			break;
	}

	/*
	 * If no command, ex does the last specified of p, l, or #, and vi
	 * moves to the line.  Otherwise, determine the length of the command
	 * name by looking for the first non-alphabetic character.  (There
	 * are a few non-alphabetic characters in command names, but they're
	 * all single character commands.)  This isn't a great test, because
	 * it means that, for the command ":e +cut.c file", we'll report that
	 * the command "cut" wasn't known.  However, it makes ":e+35 file" work
	 * correctly.
	 *
	 * !!!
	 * Historically, lines with multiple adjacent (or <blank> separated)
	 * command separators were very strange.  For example, the command
	 * |||<carriage-return>, when the cursor was on line 1, displayed
	 * lines 2, 3 and 5 of the file.  In addition, the command "   |  "
	 * would only display the line after the next line, instead of the
	 * next two lines.  No ideas why.  It worked reasonably when executed
	 * from vi mode, and displayed lines 2, 3, and 4, so we do a default
	 * command for each separator.
	 */
#define	SINGLE_CHAR_COMMANDS	"\004!#&*<=>@~"
	newscreen = 0;
	if (cmdp->cplen != 0 && cmdp->cp[0] != '|' && cmdp->cp[0] != '\n') {
		if (strchr(SINGLE_CHAR_COMMANDS, *cmdp->cp)) {
			p = cmdp->cp;
			++cmdp->cp;
			--cmdp->cplen;
			namelen = 1;
		} else {
			for (p = cmdp->cp;
			    cmdp->cplen > 0; --cmdp->cplen, ++cmdp->cp)
				if (!isalpha(*cmdp->cp))
					break;
			if ((namelen = cmdp->cp - p) == 0) {
				msgq(sp, M_ERR, "099|Unknown command name");
				goto err;
			}
		}

		/*
		 * !!!
		 * Historic vi permitted flags to immediately follow any
		 * subset of the 'delete' command, but then did not permit
		 * further arguments (flag, buffer, count).  Make it work.
		 * Permit further arguments for the few shreds of dignity
		 * it offers.
		 *
		 * Adding commands that start with 'd', and match "delete"
		 * up to a l, p, +, - or # character can break this code.
		 *
		 * !!!
		 * Capital letters beginning the command names ex, edit,
		 * next, previous, tag and visual (in vi mode) indicate the
		 * command should happen in a new screen.
		 */
		switch (p[0]) {
		case 'd':
			for (s = p,
			    t = cmds[C_DELETE].name; *s == *t; ++s, ++t);
			if (s[0] == 'l' || s[0] == 'p' || s[0] == '+' ||
			    s[0] == '-' || s[0] == '^' || s[0] == '#') {
				len = (cmdp->cp - p) - (s - p);
				cmdp->cp -= len;
				cmdp->cplen += len;
				cmdp->rcmd = cmds[C_DELETE];
				cmdp->rcmd.syntax = "1bca1";
				cmdp->cmd = &cmdp->rcmd;
				goto skip_srch;
			}
			break;
		case 'E': case 'N': case 'P': case 'T': case 'V':
			newscreen = 1;
			p[0] = tolower(p[0]);
			break;
		}

		/*
		 * Search the table for the command.
		 *
		 * !!!
		 * Historic vi permitted the mark to immediately follow the
		 * 'k' in the 'k' command.  Make it work.
		 *
		 * !!!
		 * Historic vi permitted any flag to follow the s command, e.g.
		 * "s/e/E/|s|sgc3p" was legal.  Make the command "sgc" work.
		 * Since the following characters all have to be flags, i.e.
		 * alphabetics, we can let the s command routine return errors
		 * if it was some illegal command string.  This code will break
		 * if an "sg" or similar command is ever added.  The substitute
		 * code doesn't care if it's a "cgr" flag or a "#lp" flag that
		 * follows the 's', but we limit the choices here to "cgr" so
		 * that we get unknown command messages for wrong combinations.
		 */
		if ((cmdp->cmd = ex_comm_search(p, namelen)) == NULL)
			switch (p[0]) {
			case 'k':
				if (p[1] && !p[2]) {
					cmdp->cp -= namelen - 1;
					cmdp->cplen += namelen - 1;
					cmdp->cmd = &cmds[C_K];
					break;
				}
				goto unknown;
			case 's':
				for (s = p + 1, cnt = namelen; --cnt; ++s)
					if (s[0] != 'c' &&
					    s[0] != 'g' && s[0] != 'r')
						break;
				if (cnt == 0) {
					cmdp->cp -= namelen - 1;
					cmdp->cplen += namelen - 1;
					cmdp->rcmd = cmds[C_SUBSTITUTE];
					cmdp->rcmd.fn = ex_subagain;
					cmdp->cmd = &cmdp->rcmd;
					break;
				}
				/* FALLTHROUGH */
			default:
unknown:			if (newscreen)
					p[0] = toupper(p[0]);
				ex_unknown(sp, p, namelen);
				goto err;
			}

		/*
		 * The visual command has a different syntax when called
		 * from ex than when called from a vi colon command.  FMH.
		 * Make the change now, before we test for the newscreen
		 * semantic, so that we're testing the right one.
		 */
skip_srch:	if (cmdp->cmd == &cmds[C_VISUAL_EX] && F_ISSET(sp, S_VI))
			cmdp->cmd = &cmds[C_VISUAL_VI];

		/*
		 * !!!
		 * Historic vi permitted a capital 'P' at the beginning of
		 * any command that started with 'p'.  Probably wanted the
		 * P[rint] command for backward compatibility, and the code
		 * just made Preserve and Put work by accident.  Nvi uses
		 * Previous to mean previous-in-a-new-screen, so be careful.
		 */
		if (newscreen && !F_ISSET(cmdp->cmd, E_NEWSCREEN) &&
		    (cmdp->cmd == &cmds[C_PRINT] ||
		    cmdp->cmd == &cmds[C_PRESERVE]))
			newscreen = 0;

		/* Test for a newscreen associated with this command. */
		if (newscreen && !F_ISSET(cmdp->cmd, E_NEWSCREEN))
			goto unknown;

		/*
		 * Hook for commands that are either not yet implemented
		 * or turned off.
		 */
		if (F_ISSET(cmdp->cmd, E_NOPERM)) {
			msgq(sp, M_ERR,
			    "101|The %s command is not currently supported",
			    cmdp->cmd->name);
			goto err;
		}

		/*
		 * Multiple < and > characters; another "feature".  Note,
		 * The string passed to the underlying function may not be
		 * nul terminated in this case.
		 */
		if ((cmdp->cmd == &cmds[C_SHIFTL] && *p == '<') ||
		    (cmdp->cmd == &cmds[C_SHIFTR] && *p == '>')) {
			for (ch = *p;
			    cmdp->cplen > 0; --cmdp->cplen, ++cmdp->cp)
				if (*cmdp->cp != ch)
					break;
			if (argv_exp0(sp, cmdp, p, cmdp->cp - p))
				goto err;
		}

		/* Set the format style flags for the next command. */
		if (cmdp->cmd == &cmds[C_HASH])
			exp->fdef = E_C_HASH;
		else if (cmdp->cmd == &cmds[C_LIST])
			exp->fdef = E_C_LIST;
		else if (cmdp->cmd == &cmds[C_PRINT])
			exp->fdef = E_C_PRINT;
		F_CLR(cmdp, E_USELASTCMD);
	} else {
		/* Print is the default command. */
		cmdp->cmd = &cmds[C_PRINT];

		/* Set the saved format flags. */
		F_SET(cmdp, exp->fdef);

		/*
		 * !!!
		 * If no address was specified, and it's not a global command,
		 * we up the address by one.  (I have no idea why globals are
		 * exempted, but it's (ahem) historic practice.)
		 */
		if (cmdp->addrcnt == 0 && !F_ISSET(sp, S_EX_GLOBAL)) {
			cmdp->addrcnt = 1;
			cmdp->addr1.lno = sp->lno + 1;
			cmdp->addr1.cno = sp->cno;
		}

		F_SET(cmdp, E_USELASTCMD);
	}

	/*
	 * !!!
	 * Historically, the number option applied to both ex and vi.  One
	 * strangeness was that ex didn't switch display formats until a
	 * command was entered, e.g. <CR>'s after the set didn't change to
	 * the new format, but :1p would.
	 */
	if (O_ISSET(sp, O_NUMBER)) {
		F_SET(cmdp, E_OPTNUM);
		FL_SET(cmdp->iflags, E_C_HASH);
	} else
		F_CLR(cmdp, E_OPTNUM);

	/* Check for ex mode legality. */
	if (F_ISSET(sp, S_EX) && (F_ISSET(cmdp->cmd, E_VIONLY) || newscreen)) {
		msgq(sp, M_ERR, "095|Command not available in ex mode");
		goto err;
	}

	/* Add standard command flags. */
	F_SET(cmdp, cmdp->cmd->flags);
	if (!newscreen)
		F_CLR(cmdp, E_NEWSCREEN);

	/*
	 * There are three normal termination cases for an ex command.  They
	 * are the end of the string (cmdp->cplen), or unescaped (by <literal
	 * next> characters) <newline> or '|' characters.  As we're now past
	 * possible addresses, we can determine how long the command is, so we
	 * don't have to look for all the possible terminations.  Naturally,
	 * there are some exciting special cases:
	 *
	 * 1: The bang, global, v and the filter versions of the read and
	 *    write commands are delimited by <newline>s (they can contain
	 *    shell pipes).
	 * 2: The ex, edit, next and visual in vi mode commands all take ex
	 *    commands as their first arguments.
	 * 3: The s command takes an RE as its first argument, and wants it
	 *    to be specially delimited.
	 *
	 * Historically, '|' characters in the first argument of the ex, edit,
	 * next, vi visual, and s commands didn't delimit the command.  And,
	 * in the filter cases for read and write, and the bang, global and v
	 * commands, they did not delimit the command at all.
	 *
	 * For example, the following commands were legal:
	 *
	 *	:edit +25|s/abc/ABC/ file.c
	 *	:s/|/PIPE/
	 *	:read !spell % | columnate
	 *	:global/pattern/p|l
	 *
	 * It's not quite as simple as it sounds, however.  The command:
	 *
	 *	:s/a/b/|s/c/d|set
	 *
	 * was also legal, i.e. the historic ex parser (using the word loosely,
	 * since "parser" implies some regularity) delimited the RE's based on
	 * its delimiter and not anything so irretrievably vulgar as a command
	 * syntax.
	 *
	 * One thing that makes this easier is that we can ignore most of the
	 * command termination conditions for the commands that want to take
	 * the command up to the next newline.  None of them are legal in .exrc
	 * files, so if we're here, we only dealing with a single line, and we
	 * can just eat it.
	 *
	 * Anyhow, the following code makes this all work.  First, for the
	 * special cases we move past their special argument(s).  Then, we
	 * do normal command processing on whatever is left.  Barf-O-Rama.
	 */
	cmdp->arg1_len = 0;
	cmdp->save_cmd = cmdp->cp;
	if (cmdp->cmd == &cmds[C_EDIT] || cmdp->cmd == &cmds[C_EX] ||
	    cmdp->cmd == &cmds[C_NEXT] || cmdp->cmd == &cmds[C_VISUAL_VI]) {
		/*
		 * Move to the next non-whitespace character.  A '!'
		 * immediately following the command is eaten as a
		 * force flag.
		 */
		if (cmdp->cplen > 0 && *cmdp->cp == '!') {
			++cmdp->cp;
			--cmdp->cplen;
			FL_SET(cmdp->iflags, E_C_FORCE);

			/* Reset, don't reparse. */
			cmdp->save_cmd = cmdp->cp;
		}
		for (; cmdp->cplen > 0; --cmdp->cplen, ++cmdp->cp)
			if (!isblank(*cmdp->cp))
				break;
		/*
		 * QUOTING NOTE:
		 *
		 * The historic implementation ignored all escape characters
		 * so there was no way to put a space or newline into the +cmd
		 * field.  We do a simplistic job of fixing it by moving to the
		 * first whitespace character that isn't escaped by a literal
		 * next character.  The literal next characters are stripped
		 * as they're no longer useful.
		 */
		if (cmdp->cplen > 0 && *cmdp->cp == '+') {
			++cmdp->cp;
			--cmdp->cplen;
			for (cmdp->arg1 = p = cmdp->cp;
			    cmdp->cplen > 0; --cmdp->cplen, ++cmdp->cp) {
				ch = *cmdp->cp;
				if (IS_ESCAPE(sp,
				    cmdp, ch) && cmdp->cplen > 1) {
					--cmdp->cplen;
					ch = *++cmdp->cp;
				} else if (isblank(ch))
					break;
				*p++ = ch;
			}
			cmdp->arg1_len = cmdp->cp - cmdp->arg1;

			/* Reset, so the first argument isn't reparsed. */
			cmdp->save_cmd = cmdp->cp;
		}
	} else if (cmdp->cmd == &cmds[C_BANG] ||
	    cmdp->cmd == &cmds[C_GLOBAL] || cmdp->cmd == &cmds[C_V]) {
		cmdp->cp += cmdp->cplen;
		cmdp->cplen = 0;
	} else if (cmdp->cmd == &cmds[C_READ] || cmdp->cmd == &cmds[C_WRITE]) {
		/*
		 * Move to the next character.  If it's a '!', it's a filter
		 * command and we want to eat it all, otherwise, we're done.
		 */
		for (; cmdp->cplen > 0; --cmdp->cplen, ++cmdp->cp) {
			ch = *cmdp->cp;
			if (!isblank(ch))
				break;
		}
		if (cmdp->cplen > 0 && ch == '!') {
			cmdp->cp += cmdp->cplen;
			cmdp->cplen = 0;
		}
	} else if (cmdp->cmd == &cmds[C_SUBSTITUTE]) {
		/*
		 * Move to the next non-whitespace character, we'll use it as
		 * the delimiter.  If the character isn't an alphanumeric or
		 * a '|', it's the delimiter, so parse it.  Otherwise, we're
		 * into something like ":s g", so use the special s command.
		 */
		for (; cmdp->cplen > 0; --cmdp->cplen, ++cmdp->cp)
			if (!isblank(cmdp->cp[0]))
				break;

		if (isalnum(cmdp->cp[0]) || cmdp->cp[0] == '|') {
			cmdp->rcmd = cmds[C_SUBSTITUTE];
			cmdp->rcmd.fn = ex_subagain;
			cmdp->cmd = &cmdp->rcmd;
		} else if (cmdp->cplen > 0) {
			/*
			 * QUOTING NOTE:
			 *
			 * Backslashes quote delimiter characters for RE's.
			 * The backslashes are NOT removed since they'll be
			 * used by the RE code.  Move to the third delimiter
			 * that's not escaped (or the end of the command).
			 */
			delim = *cmdp->cp;
			++cmdp->cp;
			--cmdp->cplen;
			for (cnt = 2; cmdp->cplen > 0 &&
			    cnt != 0; --cmdp->cplen, ++cmdp->cp)
				if (cmdp->cp[0] == '\\' &&
				    cmdp->cplen > 1) {
					++cmdp->cp;
					--cmdp->cplen;
				} else if (cmdp->cp[0] == delim)
					--cnt;
		}
	}

	/*
	 * Use normal quoting and termination rules to find the end of this
	 * command.
	 *
	 * QUOTING NOTE:
	 *
	 * Historically, vi permitted ^V's to escape <newline>'s in the .exrc
	 * file.  It was almost certainly a bug, but that's what bug-for-bug
	 * compatibility means, Grasshopper.  Also, ^V's escape the command
	 * delimiters.  Literal next quote characters in front of the newlines,
	 * '|' characters or literal next characters are stripped as they're
	 * no longer useful.
	 */
	vi_address = cmdp->cplen != 0 && cmdp->cp[0] != '\n';
	for (cnt = 0,
	    p = cmdp->cp; cmdp->cplen > 0; --cmdp->cplen, ++cmdp->cp) {
		ch = cmdp->cp[0];
		if (IS_ESCAPE(sp, cmdp, ch) && cmdp->cplen > 1) {
			tmp = cmdp->cp[1];
			if (tmp == '\n' || tmp == '|') {
				if (tmp == '\n')
					++sp->if_lno;
				--cmdp->cplen;
				++cmdp->cp;
				++cnt;
				ch = tmp;
			}
		} else if (ch == '\n' || ch == '|') {
			if (ch == '\n')
				F_SET(cmdp, E_NEWLINE);
			--cmdp->cplen;
			break;
		}
		*p++ = ch;
	}

	/*
	 * Save off the next command information, go back to the
	 * original start of the command.
	 */
	p = cmdp->cp + 1;
	cmdp->cp = cmdp->save_cmd;
	cmdp->save_cmd = p;
	cmdp->save_cmdlen = cmdp->cplen;
	cmdp->cplen = ((cmdp->save_cmd - cmdp->cp) - 1) - cnt;

	/*
	 * QUOTING NOTE:
	 *
	 * The "set tags" command historically used a backslash, not the
	 * user's literal next character, to escape whitespace.  Handle
	 * it here instead of complicating the argv_exp3() code.  Note,
	 * this isn't a particularly complex trap, and if backslashes were
	 * legal in set commands, this would have to be much more complicated.
	 */
	if (cmdp->cmd == &cmds[C_SET])
		for (p = cmdp->cp, len = cmdp->cplen; len > 0; --len, ++p)
			if (*p == '\\')
				*p = CH_LITERAL;

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
	 * Note, we also add the E_ADDR_ZERO flag to the command flags, for the
	 * case where the 0 address is only valid if it's a default address.
	 *
	 * Also, set a flag if we set the default addresses.  Some commands
	 * (ex: z) care if the user specified an address of if we just used
	 * the current cursor.
	 */
	switch (F_ISSET(cmdp, E_ADDR1 | E_ADDR2 | E_ADDR2_ALL | E_ADDR2_NONE)) {
	case E_ADDR1:				/* One address: */
		switch (cmdp->addrcnt) {
		case 0:				/* Default cursor/empty file. */
			cmdp->addrcnt = 1;
			F_SET(cmdp, E_ADDR_DEF);
			if (F_ISSET(cmdp, E_ADDR_ZERODEF)) {
				if (file_lline(sp, &lno))
					goto err;
				if (lno == 0) {
					cmdp->addr1.lno = 0;
					F_SET(cmdp, E_ADDR_ZERO);
				} else
					cmdp->addr1.lno = sp->lno;
			} else
				cmdp->addr1.lno = sp->lno;
			cmdp->addr1.cno = sp->cno;
			break;
		case 1:
			break;
		case 2:				/* Lose the first address. */
			cmdp->addrcnt = 1;
			cmdp->addr1 = cmdp->addr2;
		}
		break;
	case E_ADDR2_NONE:			/* Zero/two addresses: */
		if (cmdp->addrcnt == 0)		/* Default to nothing. */
			break;
		goto two_addr;
	case E_ADDR2_ALL:			/* Zero/two addresses: */
		if (cmdp->addrcnt == 0) {	/* Default entire/empty file. */
			cmdp->addrcnt = 2;
			F_SET(cmdp, E_ADDR_DEF);
			if (file_lline(sp, &cmdp->addr2.lno))
				goto err;
			if (F_ISSET(cmdp, E_ADDR_ZERODEF) &&
			    cmdp->addr2.lno == 0) {
				cmdp->addr1.lno = 0;
				F_SET(cmdp, E_ADDR_ZERO);
			} else
				cmdp->addr1.lno = 1;
			cmdp->addr1.cno = cmdp->addr2.cno = 0;
			F_SET(cmdp, E_ADDR2_ALL);
			break;
		}
		/* FALLTHROUGH */
	case E_ADDR2:				/* Two addresses: */
two_addr:	switch (cmdp->addrcnt) {
		case 0:				/* Default cursor/empty file. */
			cmdp->addrcnt = 2;
			F_SET(cmdp, E_ADDR_DEF);
			if (sp->lno == 1 &&
			    F_ISSET(cmdp, E_ADDR_ZERODEF)) {
				if (file_lline(sp, &lno))
					goto err;
				if (lno == 0) {
					cmdp->addr1.lno = cmdp->addr2.lno = 0;
					F_SET(cmdp, E_ADDR_ZERO);
				} else
					cmdp->addr1.lno =
					    cmdp->addr2.lno = sp->lno;
			} else
				cmdp->addr1.lno = cmdp->addr2.lno = sp->lno;
			cmdp->addr1.cno = cmdp->addr2.cno = sp->cno;
			break;
		case 1:				/* Default to first address. */
			cmdp->addrcnt = 2;
			cmdp->addr2 = cmdp->addr1;
			break;
		case 2:
			break;
		}
		break;
	default:
		if (cmdp->addrcnt)		/* Error. */
			goto usage;
	}

	/*
	 * !!!
	 * The ^D scroll command historically scrolled the value of the scroll
	 * option or to EOF.  It was an error if the cursor was already at EOF.
	 * (Leading addresses were permitted, but were then ignored.)
	 */
	if (cmdp->cmd == &cmds[C_SCROLL]) {
		cmdp->addrcnt = 2;
		cmdp->addr1.lno = sp->lno + 1;
		cmdp->addr2.lno = sp->lno + O_VAL(sp, O_SCROLL);
		cmdp->addr1.cno = cmdp->addr2.cno = sp->cno;
		if (file_lline(sp, &lno))
			goto err;
		if (lno != 0 && lno > sp->lno && cmdp->addr2.lno > lno)
			cmdp->addr2.lno = lno;
	}

	cmdp->flagoff = 0;
	for (p = cmdp->cmd->syntax; *p != '\0'; ++p) {
		/*
		 * The force flag is sensitive to leading whitespace, i.e.
		 * "next !" is different from "next!".  Handle it before
		 * skipping leading <blank>s.
		 */
		if (*p == '!') {
			if (cmdp->cplen > 0 && *cmdp->cp == '!') {
				++cmdp->cp;
				--cmdp->cplen;
				FL_SET(cmdp->iflags, E_C_FORCE);
			}
			continue;
		}

		/* Skip leading <blank>s. */
		for (; cmdp->cplen > 0; --cmdp->cplen, ++cmdp->cp)
			if (!isblank(*cmdp->cp))
				break;
		if (cmdp->cplen == 0)
			break;

		switch (*p) {
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
			for (; cmdp->cplen; --cmdp->cplen, ++cmdp->cp)
				switch (*cmdp->cp) {
				case '+':
					++cmdp->flagoff;
					break;
				case '-':
				case '^':
					--cmdp->flagoff;
					break;
				case '#':
					F_CLR(cmdp, E_OPTNUM);
					FL_SET(cmdp->iflags, E_C_HASH);
					exp->fdef |= E_C_HASH;
					break;
				case 'l':
					FL_SET(cmdp->iflags, E_C_LIST);
					exp->fdef |= E_C_LIST;
					break;
				case 'p':
					FL_SET(cmdp->iflags, E_C_PRINT);
					exp->fdef |= E_C_PRINT;
					break;
				default:
					goto end_case1;
				}
end_case1:		break;
		case '2':				/* -, ., +, ^ */
		case '3':				/* -, ., +, ^, = */
			for (; cmdp->cplen; --cmdp->cplen, ++cmdp->cp)
				switch (*cmdp->cp) {
				case '-':
					FL_SET(cmdp->iflags, E_C_DASH);
					break;
				case '.':
					FL_SET(cmdp->iflags, E_C_DOT);
					break;
				case '+':
					FL_SET(cmdp->iflags, E_C_PLUS);
					break;
				case '^':
					FL_SET(cmdp->iflags, E_C_CARAT);
					break;
				case '=':
					if (*p == '3') {
						FL_SET(cmdp->iflags, E_C_EQUAL);
						break;
					}
					/* FALLTHROUGH */
				default:
					goto end_case23;
				}
end_case23:		break;
		case 'b':				/* buffer */
			/*
			 * !!!
			 * Historically, "d #" was a delete with a flag, not a
			 * delete into the '#' buffer.  If the current command
			 * permits a flag, don't use one as a buffer.  However,
			 * the 'l' and 'p' flags were legal buffer names in the
			 * historic ex, and were used as buffers, not flags.
			 */
			if ((cmdp->cp[0] == '+' || cmdp->cp[0] == '-' ||
			    cmdp->cp[0] == '^' || cmdp->cp[0] == '#') &&
			    strchr(p, '1') != NULL)
				break;
			/*
			 * !!!
			 * Digits can't be buffer names in ex commands, or the
			 * command "d2" would be a delete into buffer '2', and
			 * not a two-line deletion.
			 */
			if (!isdigit(cmdp->cp[0])) {
				cmdp->buffer = *cmdp->cp;
				++cmdp->cp;
				--cmdp->cplen;
				FL_SET(cmdp->iflags, E_C_BUFFER);
			}
			break;
		case 'c':				/* count [01+a] */
			++p;
			/* Validate any signed value. */
			if (!isdigit(*cmdp->cp) && (*p != '+' ||
			    (*cmdp->cp != '+' && *cmdp->cp != '-')))
				break;
			/* If a signed value, set appropriate flags. */
			if (*cmdp->cp == '-')
				FL_SET(cmdp->iflags, E_C_COUNT_NEG);
			else if (*cmdp->cp == '+')
				FL_SET(cmdp->iflags, E_C_COUNT_POS);
			if ((nret =
			    nget_slong(&ltmp, cmdp->cp, &t, 10)) != NUM_OK) {
				ex_badaddr(sp, NULL, A_NOTSET, nret);
				goto err;
			}
			if (ltmp == 0 && *p != '0') {
				msgq(sp, M_ERR, "104|Count may not be zero");
				goto err;
			}
			cmdp->cplen -= (t - cmdp->cp);
			cmdp->cp = t;

			/*
			 * Counts as address offsets occur in commands taking
			 * two addresses.  Historic vi practice was to use
			 * the count as an offset from the *second* address.
			 *
			 * Set a count flag; some underlying commands (see
			 * join) do different things with counts than with
			 * line addresses.
			 */
			if (*p == 'a') {
				cmdp->addr1 = cmdp->addr2;
				cmdp->addr2.lno = cmdp->addr1.lno + ltmp - 1;
			} else
				cmdp->count = ltmp;
			FL_SET(cmdp->iflags, E_C_COUNT);
			break;
		case 'f':				/* file */
			if (argv_exp2(sp, cmdp, cmdp->cp, cmdp->cplen))
				goto err;
			goto arg_cnt_chk;
		case 'l':				/* line */
			/*
			 * Get a line specification.
			 *
			 * If the line was a search expression, we may have
			 * changed state during the call, and we're now
			 * searching the file.  Push ourselves onto the state
			 * stack.
			 */
			if (ex_line(sp, cmdp, &isaddr))
				goto err;
			switch (gp->cm_state) {
			case ES_PARSE:
				break;
			case ES_PARSE_ERROR:
				goto err;
			case ES_RUNNING:
				gp->cm_state = ES_PARSE_LINE;
				return (0);
			default:
				abort();
			}
line_restart:
			/* Line specifications are always required. */
			if (!isaddr) {
				p = msg_print(sp, cmdp->cp, &nf);
				msgq(sp, M_ERR,
				     "105|%s: bad line specification", p);
				if (nf)
					FREE_SPACE(sp, p, 0);
				goto err;
			}
			/*
			 * The target line should exist for these commands,
			 * but 0 is legal for them as well.
			 */
			if (cmdp->caddr.lno != 0 &&
			    !file_eline(sp, cmdp->caddr.lno)) {
				ex_badaddr(sp, NULL, A_EOF, NUM_OK);
				goto err;
			}
			cmdp->lineno = cmdp->caddr.lno;
			break;
		case 'S':				/* string, file exp. */
			if (cmdp->cplen != 0) {
				if (argv_exp1(sp, cmdp, cmdp->cp,
				    cmdp->cplen, cmdp->cmd == &cmds[C_BANG]))
					goto err;
				goto addr_verify;
			}
			/* FALLTHROUGH */
		case 's':				/* string */
			if (argv_exp0(sp, cmdp, cmdp->cp, cmdp->cplen))
				goto err;
			goto addr_verify;
		case 'W':				/* word string */
			/*
			 * QUOTING NOTE:
			 *
			 * Literal next characters escape the following
			 * character.  Quoting characters are stripped
			 * here since they are no longer useful.
			 *
			 * First there was the word.
			 */
			for (p = t = cmdp->cp;
			    cmdp->cplen > 0; --cmdp->cplen, ++cmdp->cp) {
				ch = *cmdp->cp;
				if (IS_ESCAPE(sp,
				    cmdp, ch) && cmdp->cplen > 1) {
					--cmdp->cplen;
					*p++ = *++cmdp->cp;
				} else if (isblank(ch)) {
					++cmdp->cp;
					--cmdp->cplen;
					break;
				} else
					*p++ = ch;
			}
			if (argv_exp0(sp, cmdp, t, p - t))
				goto err;

			/* Delete intervening whitespace. */
			for (; cmdp->cplen > 0;
			    --cmdp->cplen, ++cmdp->cp) {
				ch = *cmdp->cp;
				if (!isblank(ch))
					break;
			}
			if (cmdp->cplen == 0)
				goto usage;

			/* Followed by the string. */
			for (p = t = cmdp->cp; cmdp->cplen > 0;
			    --cmdp->cplen, ++cmdp->cp, ++p) {
				ch = *cmdp->cp;
				if (IS_ESCAPE(sp,
				    cmdp, ch) && cmdp->cplen > 1) {
					--cmdp->cplen;
					*p = *++cmdp->cp;
				} else
					*p = ch;
			}
			if (argv_exp0(sp, cmdp, t, p - t))
				goto err;
			goto addr_verify;
		case 'w':				/* word */
			if (argv_exp3(sp, cmdp, cmdp->cp, cmdp->cplen))
				goto err;
arg_cnt_chk:		if (*++p != 'N') {		/* N */
				/*
				 * If a number is specified, must either be
				 * 0 or that number, if optional, and that
				 * number, if required.
				 */
				tmp = *p - '0';
				if ((*++p != 'o' || exp->argsoff != 0) &&
				    exp->argsoff != tmp)
					goto usage;
			}
			goto addr_verify;
		default:
			msgq(sp, M_ERR,
			    "106|Internal syntax table error (%s: %s)",
			    cmdp->cmd->name, KEY_NAME(sp, *p));
		}
	}

	/* Skip trailing whitespace. */
	for (; cmdp->cplen > 0; --cmdp->cplen) {
		ch = *cmdp->cp++;
		if (!isblank(ch))
			break;
	}

	/*
	 * There shouldn't be anything left, and no more required fields,
	 * i.e neither 'l' or 'r' in the syntax string.
	 */
	if (cmdp->cplen != 0 || strpbrk(p, "lr")) {
usage:		msgq(sp, M_ERR, "107|Usage: %s", cmdp->cmd->usage);
		goto err;
	}

	/*
	 * Verify that the addresses are legal.  Check the addresses here,
	 * because this is a place where all ex addresses pass through.
	 * (They don't all pass through ep_line(), for instance.)  We're
	 * assuming that any non-existent line doesn't exist because it's
	 * past the end-of-file.  That's a pretty good guess.
	 *
	 * If it's a "default vi command", an address of zero is okay.
	 */
addr_verify:
	switch (cmdp->addrcnt) {
	case 2:
		/*
		 * Historic ex/vi permitted commands with counts to go past
		 * EOF.  So, for example, if the file only had 5 lines, the
		 * ex command "1,6>" would fail, but the command ">300"
		 * would succeed.  Since we don't want to have to make all
		 * of the underlying commands handle random line numbers,
		 * fix it here.
		 */
		if (cmdp->addr2.lno == 0) {
			if (!F_ISSET(cmdp, E_ADDR_ZERO) &&
			    (F_ISSET(sp, S_EX) ||
			    !F_ISSET(cmdp, E_USELASTCMD))) {
				ex_badaddr(sp, cmdp->cmd, A_ZERO, NUM_OK);
				goto err;
			}
		} else if (!file_eline(sp, cmdp->addr2.lno))
			if (FL_ISSET(cmdp->iflags, E_C_COUNT)) {
				if (file_lline(sp, &lno))
					goto err;
				cmdp->addr2.lno = lno;
			} else {
				ex_badaddr(sp, NULL, A_EOF, NUM_OK);
				goto err;
			}
		/* FALLTHROUGH */
	case 1:
		if (cmdp->addr1.lno == 0) {
			if (!F_ISSET(cmdp, E_ADDR_ZERO) &&
			    (F_ISSET(sp, S_EX) ||
			    !F_ISSET(cmdp, E_USELASTCMD))) {
				ex_badaddr(sp, cmdp->cmd, A_ZERO, NUM_OK);
				goto err;
			}
		} else if (!file_eline(sp, cmdp->addr1.lno)) {
			ex_badaddr(sp, NULL, A_EOF, NUM_OK);
			goto err;
		}
		break;
	}

	/*
	 * If doing a default command and there's nothing left on the line,
	 * vi just moves to the line.  For example, ":3" and ":'a,'b" just
	 * move to line 3 and line 'b, respectively, but ":3|" prints line 3.
	 *
	 * !!!
	 * In addition, IF THE LINE CHANGES, move to the first nonblank of
	 * the line.
	 *
	 * !!!
	 * This is done before the absolute mark gets set; historically,
	 * "/a/,/b/" did NOT set vi's absolute mark, but "/a/,/b/d" did.
	 */
	if ((F_ISSET(sp, S_VI) || F_ISSET(cmdp, E_NOPRDEF)) &&
	    F_ISSET(cmdp, E_USELASTCMD) && vi_address == 0) {
		switch (cmdp->addrcnt) {
		case 2:
			if (sp->lno !=
			    (cmdp->addr2.lno ? cmdp->addr2.lno : 1)) {
				sp->lno =
				    cmdp->addr2.lno ? cmdp->addr2.lno : 1;
				sp->cno = 0;
				(void)nonblank(sp, sp->lno, &sp->cno);
			}
			break;
		case 1:
			if (sp->lno !=
			    (cmdp->addr1.lno ? cmdp->addr1.lno : 1)) {
				sp->lno =
				    cmdp->addr1.lno ? cmdp->addr1.lno : 1;
				sp->cno = 0;
				(void)nonblank(sp, sp->lno, &sp->cno);
			}
			break;
		}
		cmdp->cp = cmdp->save_cmd;
		cmdp->cplen = cmdp->save_cmdlen;
		goto loop;
	}

	/*
	 * Set the absolute mark -- we have to set it for vi here, in case
	 * it's a compound command, e.g. ":5p|6" should set the absolute
	 * mark for vi.
	 */
	if (F_ISSET(cmdp, E_ABSMARK)) {
		cur.lno = sp->lno;
		cur.cno = sp->cno;
		F_CLR(cmdp, E_ABSMARK);
		if (mark_set(sp, ABSMARK1, &cur, 1))
			goto err;
	}

#if defined(DEBUG) && defined(COMLOG)
	ex_comlog(sp, cmdp);
#endif
	/* Clear autoprint flag. */
	F_CLR(cmdp, E_AUTOPRINT);

	/* Increment the command count if not called from vi. */
	if (F_ISSET(sp, S_EX))
		++sp->ccnt;

	/*
	 * If file state available, and not doing a global command,
	 * log the start of an action.
	 */
	if (sp->ep != NULL && !F_ISSET(sp, S_EX_GLOBAL))
		(void)log_cursor(sp);

	/*
	 * !!!
	 * There are two special commands for the purposes of this code: the
	 * default command (<carriage-return>) or the scrolling commands (^D
	 * and <EOF>) as the first non-<blank> characters  in the line.
	 *
	 * If this is the first command in the command line, we received the
	 * command from the ex command loop and we're talking to a tty, and
	 * and there's nothing else on the command line, and it's one of the
	 * special commands, we move back up to the previous line, and erase
	 * the prompt character with the output.  Since ex runs in canonical
	 * mode, we don't have to do anything else, a <newline> has already
	 * been echoed by the tty driver.  It's OK if vi calls us -- we won't
	 * be in ex mode so we'll do nothing.
	 *
	 * !!!
	 * Historically, ex didn't erase the line, so, if the displayed line
	 * was only a single character long, and <eof> was represented as ^D,
	 * the output wouldn't overwrite the user's input.  To fix this, we
	 * print the maxiumum character number of spaces.  Note, this won't
	 * help if the user entered extra prompt or <blank> characters before
	 * the command character.  We'd have to do a lot of work to make that
	 * work, and it's not worth the effort.
	 */
	if (cmdp->sep != SEP_NONE) {
		if (sp->ep != NULL &&
		    F_ISSET(sp, S_EX) && F_ISSET(gp, G_STDIN_TTY))
			if (cmdp->sep == SEP_NEED_NR &&
			    (F_ISSET(cmdp, E_USELASTCMD) ||
			    cmdp->cmd == &cmds[C_SCROLL]))
				gp->scr_exadjust(sp, EX_TERM_SCROLL);
		cmdp->sep = SEP_NONE;
	}

	/* Call the underlying function for the ex command. */
	if (cmdp->cmd->fn(sp, cmdp)) {
		if (!F_ISSET(gp, G_STDIN_TTY))
			F_SET(sp, S_EXIT_FORCE);
		goto err;
	}

#ifdef DEBUG
	/* Make sure no function left the temporary space locked. */
	if (F_ISSET(gp, G_TMP_INUSE)) {
		F_CLR(gp, G_TMP_INUSE);
		msgq(sp, M_ERR, "109|%s: temporary buffer not released",
		    cmdp->cmd->name);
	}
#endif
	/*
	 * If the state has changed, return for now, and continue this
	 * command later.
	 */
	if (gp->cm_state != ES_PARSE)
		return (0);

func_restart:
	/*
	 * Integrate any offset parsed by the underlying command, and make
	 * sure the referenced line exists.
	 *
	 * XXX
	 * May not match historic practice (which I've never been able to
	 * completely figure out.)  For example, the '=' command from vi
	 * mode often got the offset wrong, and complained it was too large,
	 * but didn't seem to have a problem with the cursor.  If anyone
	 * complains, ask them how it's supposed to work, they might know.
	 */
	if (sp->ep != NULL && cmdp->flagoff) {
		if (cmdp->flagoff < 0) {
			if (sp->lno <= -cmdp->flagoff) {
				msgq(sp, M_ERR,
				    "110|Flag offset before line 1");
				goto err;
			}
		} else {
			if (!NPFITS(MAX_REC_NUMBER, sp->lno, cmdp->flagoff)) {
				ex_badaddr(sp, NULL, A_NOTSET, NUM_OVER);
				goto err;
			}
			if (!file_eline(sp, sp->lno + cmdp->flagoff)) {
				msgq(sp, M_ERR,
				    "111|Flag offset past end-of-file");
				goto err;
			}
		}
		sp->lno += cmdp->flagoff;
	}

	/*
	 * If the command was successful may want to display a line based on
	 * the autoprint option or an explicit print flag.  (Make sure that
	 * there's a line to display.)  Also, the autoprint edit option is
	 * turned off for the duration of global commands.
	 */
	if (F_ISSET(sp, S_EX) && sp->ep != NULL && sp->lno != 0) {
		/*
		 * The print commands have already handled the `print' flags.
		 * If so, clear them.
		 */
		if (FL_ISSET(cmdp->iflags, E_CLRFLAG))
			FL_CLR(cmdp->iflags, E_C_HASH | E_C_LIST | E_C_PRINT);

		/* If hash set only because of the number option, discard it. */
		if (F_ISSET(cmdp, E_OPTNUM))
			FL_CLR(cmdp->iflags, E_C_HASH);

		/*
		 * If there was an explicit flag to display the new cursor line,
		 * or autoprint is set and a change was made, display the line.
		 * If any print flags were set use them, else default to print.
		 */
		F_INIT(cmdp,
		    FL_ISSET(cmdp->iflags, E_C_HASH | E_C_LIST | E_C_PRINT));
		if (!F_ISSET(cmdp,
		    E_C_HASH | E_C_LIST | E_C_PRINT | E_NOAUTO) &&
		    !F_ISSET(sp, S_EX_GLOBAL) && O_ISSET(sp, O_AUTOPRINT) &&
		    F_ISSET(cmdp, E_AUTOPRINT))
			FL_INIT(cmdp->iflags, E_C_PRINT);

		if (F_ISSET(cmdp, E_C_HASH | E_C_LIST | E_C_PRINT)) {
			memset(cmdp, 0, sizeof(EXCMD));
			cmdp->addrcnt = 2;
			cmdp->addr1.lno = cmdp->addr2.lno = sp->lno;
			cmdp->addr1.cno = cmdp->addr2.cno = sp->cno;
			(void)ex_print(sp,
			    &cmdp->addr1, &cmdp->addr2, cmdp->flags);
		}
	}

	/*
	 * If the command had an associated "+cmd", it has to be executed
	 * before we finish executing any more of this ex command.  For
	 * example, consider a .exrc file that contains the following lines:
	 *
	 *	:set all
	 *	:edit +25 file.c|s/abc/ABC/|1
	 *	:3,5 print
	 *
	 * This can happen more than once -- the historic vi simply hung or
	 * dropped core, of course.  Prepend the + command back into the
	 * current command and continue.  We may have to add up to two more
	 * characters, a <literal next> and a command separator.  We know
	 * that it will still fit because we discarded at least one space
	 * and the + character.
	 */
	if (cmdp->arg1_len != 0) {
		/* Add in a command separator. */
		*--cmdp->save_cmd = '\n';
		++cmdp->save_cmdlen;

		/*
		 * If the last character of the + command were a <literal next>
		 * character, it would be treated differently because of the
		 * append.  Quote it, if necessary.
		 */
		if (IS_ESCAPE(sp, cmdp, cmdp->arg1[cmdp->arg1_len - 1])) {
			*--cmdp->save_cmd = CH_LITERAL;
			++cmdp->save_cmdlen;
		}

		cmdp->save_cmd -= cmdp->arg1_len;
		cmdp->save_cmdlen += cmdp->arg1_len;
		memmove(cmdp->save_cmd, cmdp->arg1, cmdp->arg1_len);

		/*
		 * Any commands executed from a +cmd are executed starting at
		 * the last line, first column of the file -- NOT the first
		 * nonblank.)  The main file startup code doesn't know that a
		 * +cmd was set, however, so it may have put us at the top of
		 * the file.  (Note, this is safe because we must have switched
		 * files to get here.)
		 */
		F_SET(cmdp, E_MOVETOEND);
	}

	/* Update the current command. */
	cmdp->cp = cmdp->save_cmd;
	cmdp->cplen = cmdp->save_cmdlen;

	/*
	 * !!!
	 * If we've changed screens or underlying files, any current executing
	 * @ buffer or global command is discarded.  This is historic practice
	 * for globals, and necessary for @ buffers.
	 *
	 * If we've changed underlying files, it's not a problem, we continue
	 * with the rest of the ex command(s), operating on the new file.
	 * However, if we switch screens (either by exiting or by an explicit
	 * command), we have no way of knowing where to put output messages,
	 * and, since we don't control screens here, we could screw up the
	 * upper layers, (e.g. we could exit/reenter a screen multiple times)
	 * and the main event driver can't handle that correctly.  So, return
	 * and continue after we've got a new screen.
	 */
	if (F_ISSET(sp, S_EXIT | S_EXIT_FORCE | S_SSWITCH)) {
		if (exp->gatq.lh_first != NULL) {
			msgq(sp, M_ERR,
    "283|Ex command changed current file: global/@ command input discarded");
			if (ex_gatfree(sp))
				return (1);
		}
		if (cmdp->save_cmdlen != 0 || cmdp->arg1_len != 0) {
			gp->cm_state = ES_PARSE_EXIT;
			FL_SET(gp->ec_flags, EC_INTERRUPT);
			return (0);
		}
	}

	/*
	 * If we're in the middle of a global command, and we've executed
	 * some number of commands, check for interrupts.
	 */
	if (gcnt-- == 0) {
		gp->cm_state = ES_PARSE_EXIT;
		FL_SET(gp->ec_flags, EC_INTERRUPT);
		return (0);
	}
	goto loop;
	/* NOTREACHED */

err:	/*
	 * On error, we discard any keys we have left, as well as any keys that
	 * were mapped and are waiting.  The test of cmdp->save_cmdlen isn't
	 * necessarily correct.  If we fail early enough we don't know if the
	 * entire string was a single command or not.  Try and guess, as it's
	 * useful to know if any part of the command was discarded.
	 */
	if (cmdp->save_cmdlen == 0)
		for (; cmdp->cplen; --cmdp->cplen) {
			ch = *cmdp->cp++;
			if (IS_ESCAPE(sp, cmdp, ch) && cmdp->cplen > 1) {
				--cmdp->cplen;
				++cmdp->cp;
			} else if (ch == '\n' || ch == '|') {
				if (cmdp->cplen > 1)
					cmdp->save_cmdlen = 1;
				break;
			}
		}
	if (v_event_flush(sp, CH_MAPPED))
		msgq(sp, M_ERR, "091|Ex command failed: mapped keys discarded");
	if (cmdp->save_cmdlen != 0 || exp->gatq.lh_first != NULL) {
		if (ex_gatfree(sp))
			return (1);
		msgq(sp, M_ERR,
		    "112|Ex command failed: remaining command input discarded");
	}

ret:	gp->cm_state = ES_PARSE;
	return (0);
}

/*
 * ex_range --
 *	Get a line range for ex commands, or perform a vi ex address search.
 */
int
ex_range(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	GS *gp;
	EX_PRIVATE *exp;
	int isaddr;

	gp = sp->gp;
	exp = EXP(sp);

	switch (gp->cm_state) {
	case ES_PARSE:			/* Initial state, parsing a command. */
		break;
	case ES_PARSE_RANGE:		/* Restarting a range. */
		if (ex_line(sp, cmdp, &isaddr))
			return (1);
		switch (gp->cm_state) {
		case ES_PARSE:		/* Continue parse. */
			goto range_restart;
		case ES_PARSE_ERROR:	/* Error in the parse. */
		case ES_RUNNING:	/* Continue the search. */
			return (0);
		default:
			abort();
		}
		/* NOTREACHED */
	default:
		abort();
	}

	/*
	 * Parse comma or semi-colon delimited line specs.
	 *
	 * Semi-colon delimiters update the current address to be the last
	 * address.  For example, the command
	 *
	 *	:3;/pattern/cmdp->cp
	 *
	 * will search for pattern from line 3.  In addition, if cmdp->cp
	 * is not a valid command, the current line will be left at 3, not
	 * at the original address.
	 *
	 * Extra addresses are discarded, starting with the first.
	 *
	 * !!!
	 * If any addresses are missing, they default to the current line.
	 * This was historically true for both leading and trailing comma
	 * delimited addresses as well as for trailing semicolon delimited
	 * addresses.  For consistency, we make it true for leading semicolon
	 * addresses as well.
	 */
	exp = EXP(sp);
	cmdp->addrcnt = 0;
	cmdp->addr = ADDR_NONE;
	while (cmdp->cplen > 0)
		switch (*cmdp->cp) {
		case '%':		/* Entire file. */
			/* Vi ex address searches didn't permit % signs. */
			if (F_ISSET(cmdp, E_VISEARCH))
				goto ret;

			/* It's an error if the file is empty. */
			if (sp->ep == NULL) {
				gp->cm_state = ES_PARSE_ERROR;
				ex_badaddr(sp, NULL, A_EMPTY, NUM_OK);
				return (0);
			}
			/*
			 * !!!
			 * A percent character addresses all of the lines in
			 * the file.  Historically, it couldn't be followed by
			 * any other address.  We do it as a text substitution
			 * for simplicity.  POSIX 1003.2 is expected to follow
			 * this practice.
			 *
			 * If it's an empty file, the first line is 0, not 1.
			 */
			if (cmdp->addr == ADDR_FOUND) {
				gp->cm_state = ES_PARSE_ERROR;
				ex_badaddr(sp, NULL, A_COMBO, NUM_OK);
				return (0);
			}
			if (file_lline(sp, &cmdp->addr2.lno))
				return (1);
			cmdp->addr1.lno = cmdp->addr2.lno == 0 ? 0 : 1;
			cmdp->addr1.cno = cmdp->addr2.cno = 0;
			cmdp->addrcnt = 2;
			cmdp->addr = ADDR_FOUND;
			++cmdp->cp;
			--cmdp->cplen;
			break;
		case ',':               /* Comma delimiter. */
			/* Vi ex address searches didn't permit commas. */
			if (F_ISSET(cmdp, E_VISEARCH))
				goto ret;
			/* FALLTHROUGH */
		case ';':               /* Semi-colon delimiter. */
			if (sp->ep == NULL) {
				gp->cm_state = ES_PARSE_ERROR;
				ex_badaddr(sp, NULL, A_EMPTY, NUM_OK);
				return (0);
			}
			if (cmdp->addr != ADDR_FOUND)
				switch (cmdp->addrcnt) {
				case 0:
					cmdp->addr1.lno = sp->lno;
					cmdp->addr1.cno = sp->cno;
					cmdp->addrcnt = 1;
					break;
				case 2:
					cmdp->addr1 = cmdp->addr2;
					/* FALLTHROUGH */
				case 1:
					cmdp->addr2.lno = sp->lno;
					cmdp->addr2.cno = sp->cno;
					cmdp->addrcnt = 2;
					break;
				}
			if (*cmdp->cp == ';')
				switch (cmdp->addrcnt) {
				case 0:
					abort();
					break;
				case 1:
					sp->lno = cmdp->addr1.lno;
					sp->cno = cmdp->addr1.cno;
					break;
				case 2:
					sp->lno = cmdp->addr2.lno;
					sp->cno = cmdp->addr2.cno;
					break;
				}
			cmdp->addr = ADDR_NEED;
			/* FALLTHROUGH */
		case ' ':		/* Whitespace. */
		case '\t':		/* Whitespace. */
			++cmdp->cp;
			--cmdp->cplen;
			break;
		default:
			/*
			 * Get a line specification.
			 *
			 * If the line was a search expression, we may have
			 * changed state during the call, and we're now
			 * searching the file.  Push ourselves onto the state
			 * stack.
			 */
			if (ex_line(sp, cmdp, &isaddr))
				return (1);
			switch (gp->cm_state) {
			case ES_PARSE:
				break;
			case ES_PARSE_ERROR:
				return (0);
			case ES_RUNNING:
				return (0);
			default:
				abort();
			}
range_restart:
			if (!isaddr)
				goto ret;
			if (cmdp->addr == ADDR_FOUND) {
				gp->cm_state = ES_PARSE_ERROR;
				ex_badaddr(sp, NULL, A_COMBO, NUM_OK);
				return (0);
			}
			switch (cmdp->addrcnt) {
			case 0:
				cmdp->addr1 = cmdp->caddr;
				cmdp->addrcnt = 1;
				break;
			case 1:
				cmdp->addr2 = cmdp->caddr;
				cmdp->addrcnt = 2;
				break;
			case 2:
				cmdp->addr1 = cmdp->addr2;
				cmdp->addr2 = cmdp->caddr;
				break;
			}
			cmdp->addr = ADDR_FOUND;
			break;
		}

	/*
	 * !!!
	 * Vi ex address searches are indifferent to order or trailing
	 * semi-colons.
	 */
ret:	if (F_ISSET(cmdp, E_VISEARCH))
		return (0);

	if (cmdp->addr == ADDR_NEED)
		switch (cmdp->addrcnt) {
		case 0:
			cmdp->addr1.lno = sp->lno;
			cmdp->addr1.cno = sp->cno;
			cmdp->addrcnt = 1;
			break;
		case 2:
			cmdp->addr1 = cmdp->addr2;
			/* FALLTHROUGH */
		case 1:
			cmdp->addr2.lno = sp->lno;
			cmdp->addr2.cno = sp->cno;
			cmdp->addrcnt = 2;
			break;
		}

	if (cmdp->addrcnt == 2 && cmdp->addr2.lno < cmdp->addr1.lno) {
		gp->cm_state = ES_PARSE_ERROR;
		msgq(sp, M_ERR,
		    "113|The second address is smaller than the first");
		return (0);
	}
	return (0);
}

/*
 * ex_line --
 *	Get a single line address specifier.
 *
 * The way the "previous context" mark worked was that any "non-relative"
 * motion set it.  While ex/vi wasn't totally consistent about this, ANY
 * numeric address, search pattern, '$', or mark reference in an address
 * was considered non-relative, and set the value.  Which should explain
 * why we're hacking marks down here.  The problem was that the mark was
 * only set if the command was called, i.e. we have to set a flag and test
 * it later.
 *
 * XXX
 * This is probably still not exactly historic practice, although I think
 * it's fairly close.
 */
static int
ex_line(sp, cmdp, isaddrp)
	SCR *sp;
	EXCMD *cmdp;
	int *isaddrp;
{
	enum nresult nret;
	EX_PRIVATE *exp;
	GS *gp;
	MARK *mp;
	long total, val;
	int isneg;
	int (*sf) __P((SCR *, MARK *, MARK *, char *, char **, u_int));
	char *endp;

	gp = sp->gp;
	exp = EXP(sp);
	mp = &cmdp->caddr;

	*isaddrp = 0;
	F_CLR(cmdp, E_DELTA);

	switch (gp->cm_state) {
	case ES_PARSE:			/* Initial state, parsing a command. */
		break;
	case ES_PARSE_LINE:		/* Restarting after a search. */
	case ES_PARSE_RANGE:
		if (!FL_ISSET(sp->srch_flags, SEARCH_FOUND)) {
			gp->cm_state = ES_PARSE_ERROR;
			return (0);
		}
		gp->cm_state = ES_PARSE;
		goto line_restart;
	default:
		abort();
	}

	/* No addresses permitted until a file has been read in. */
	if (sp->ep == NULL && strchr("$0123456789'\\/?.+-^", *cmdp->cp)) {
		gp->cm_state = ES_PARSE_ERROR;
		ex_badaddr(sp, NULL, A_EMPTY, NUM_OK);
		return (0);
	}

	switch (*cmdp->cp) {
	case '$':				/* Last line in the file. */
		*isaddrp = 1;
		F_SET(cmdp, E_ABSMARK);

		mp->cno = 0;
		if (file_lline(sp, &mp->lno))
			return (1);
		++cmdp->cp;
		--cmdp->cplen;
		break;				/* Absolute line number. */
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		*isaddrp = 1;
		F_SET(cmdp, E_ABSMARK);

		if ((nret = nget_slong(&val, cmdp->cp, &endp, 10)) != NUM_OK) {
			gp->cm_state = ES_PARSE_ERROR;
			ex_badaddr(sp, NULL, A_NOTSET, nret);
			return (0);
		}
		if (!NPFITS(MAX_REC_NUMBER, 0, val)) {
			gp->cm_state = ES_PARSE_ERROR;
			ex_badaddr(sp, NULL, A_NOTSET, NUM_OVER);
			return (0);
		}
		mp->lno = val;
		mp->cno = 0;
		cmdp->cplen -= (endp - cmdp->cp);
		cmdp->cp = endp;
		break;
	case '\'':				/* Use a mark. */
		*isaddrp = 1;
		F_SET(cmdp, E_ABSMARK);

		if (cmdp->cplen == 1) {
			gp->cm_state = ES_PARSE_ERROR;
			msgq(sp, M_ERR, "114|No mark name supplied");
			return (0);
		}
		if (mark_get(sp, cmdp->cp[1], mp, M_ERR)) {
			gp->cm_state = ES_PARSE_ERROR;
			return (0);
		}
		cmdp->cp += 2;
		cmdp->cplen -= 2;
		break;
	case '\\':				/* Search: forward/backward. */
		/*
		 * !!!
		 * I can't find any difference between // and \/ or between
		 * ?? and \?.  Mark Horton doesn't remember there being any
		 * difference.  C'est la vie.
		 */
		if (cmdp->cplen < 2 ||
		    cmdp->cp[1] != '/' && cmdp->cp[1] != '?') {
			gp->cm_state = ES_PARSE_ERROR;
			msgq(sp, M_ERR, "115|\\ not followed by / or ?");
			return (0);
		}
		++cmdp->cp;
		--cmdp->cplen;
		sf = cmdp->cp[0] == '/' ? fsrch_setup : bsrch_setup;
		goto search;
	case '/':				/* Search forward. */
		sf = fsrch_setup;
		exp->run_func = fsrch;
		goto search;
	case '?':				/* Search backward. */
		sf = bsrch_setup;
		exp->run_func = bsrch;

search:		mp->lno = sp->lno;
		mp->cno = sp->cno;
		if (sf(sp, mp, mp,
		    cmdp->cp, &endp, SEARCH_MSG | SEARCH_PARSE | SEARCH_SET))
			return (1);

		/* Fix up the command pointers. */
		cmdp->cplen -= (endp - cmdp->cp);
		cmdp->cp = endp;

		/* Set up the search. */
		gp->cm_state = ES_RUNNING;
		return (0);

line_restart:	*isaddrp = 1;
		F_SET(cmdp, E_ABSMARK);
		break;
	case '.':				/* Current position. */
		*isaddrp = 1;
		mp->cno = sp->cno;

		/* If an empty file, then '.' is 0, not 1. */
		if (sp->lno == 1) {
			if (file_lline(sp, &mp->lno))
				return (1);
			if (mp->lno != 0)
				mp->lno = 1;
		} else
			mp->lno = sp->lno;

		/*
		 * !!!
		 * Historically, .<number> was the same as .+<number>, i.e.
		 * the '+' could be omitted.  (This feature is found in ed
		 * as well.)
		 */
		if (cmdp->cplen > 1 && isdigit(cmdp->cp[1]))
			*cmdp->cp = '+';
		else {
			++cmdp->cp;
			--cmdp->cplen;
		}
		break;
	}

	/* Skip trailing <blank>s. */
	for (; cmdp->cplen > 0 &&
	    isblank(cmdp->cp[0]); ++cmdp->cp, --cmdp->cplen);

	/*
	 * Evaluate any offset.  If no address yet found, the offset
	 * is relative to ".".
	 */
	total = 0;
	if (cmdp->cplen != 0 && (isdigit(cmdp->cp[0]) ||
	    cmdp->cp[0] == '+' || cmdp->cp[0] == '-' ||
	    cmdp->cp[0] == '^')) {
		if (!*isaddrp) {
			*isaddrp = 1;
			mp->lno = sp->lno;
			mp->cno = sp->cno;
		}
		/*
		 * Evaluate an offset, defined as:
		 *
		 *		[+-^<blank>]*[<blank>]*[0-9]*
		 *
		 * The rough translation is any number of signs, optionally
		 * followed by numbers, or a number by itself, all <blank>
		 * separated.
		 *
		 * !!!
		 * All address offsets were additive, e.g. "2 2 3p" was the
		 * same as "7p", or, "/ZZZ/ 2" was the same as "/ZZZ/+2".
		 * Note, however, "2 /ZZZ/" was an error.  It was also legal
		 * to insert signs without numbers, so "3 - 2" was legal, and
		 * equal to 4.
		 *
		 * !!!
		 * Offsets were historically permitted for any line address,
		 * e.g. the command "1,2 copy 2 2 2 2" copied lines 1,2 after
		 * line 8.
		 *
		 * !!!
		 * Offsets were historically permitted for search commands,
		 * and handled as addresses: "/pattern/2 2 2" was legal, and
		 * referenced the 6th line after pattern.
		 */
		F_SET(cmdp, E_DELTA);
		for (;;) {
			for (; cmdp->cplen > 0 && isblank(cmdp->cp[0]);
			    ++cmdp->cp, --cmdp->cplen);
			if (cmdp->cplen == 0 || !isdigit(cmdp->cp[0]) &&
			    cmdp->cp[0] != '+' && cmdp->cp[0] != '-' &&
			    cmdp->cp[0] != '^')
				break;
			if (!isdigit(cmdp->cp[0]) &&
			    !isdigit(cmdp->cp[1])) {
				total += cmdp->cp[0] == '+' ? 1 : -1;
				--cmdp->cplen;
				++cmdp->cp;
			} else {
				if (cmdp->cp[0] == '-' ||
				    cmdp->cp[0] == '^') {
					++cmdp->cp;
					--cmdp->cplen;
					isneg = 1;
				} else
					isneg = 0;

				/* Get a signed long, add it to the total. */
				if ((nret = nget_slong(&val,
				    cmdp->cp, &endp, 10)) != NUM_OK ||
				    (nret = NADD_SLONG(sp,
				    total, val)) != NUM_OK) {
					gp->cm_state = ES_PARSE_ERROR;
					ex_badaddr(sp, NULL, A_NOTSET, nret);
					return (0);
				}
				total += isneg ? -val : val;
				cmdp->cplen -= (endp - cmdp->cp);
				cmdp->cp = endp;
			}
		}
	}

	/*
	 * Any value less than 0 is an error.  Make sure that the new value
	 * will fit into a recno_t.
	 */
	if (*isaddrp && total != 0) {
		if (total < 0) {
			if (-total > mp->lno) {
				gp->cm_state = ES_PARSE_ERROR;
				msgq(sp, M_ERR,
			    "117|Reference to a line number less than 0");
				return (0);
			}
		} else
			if (!NPFITS(MAX_REC_NUMBER, mp->lno, total)) {
				gp->cm_state = ES_PARSE_ERROR;
				ex_badaddr(sp, NULL, A_NOTSET, NUM_OVER);
				return (0);
			}
		mp->lno += total;
	}
	return (0);
}

/*
 * ex_gat --
 *	Load up the next @ buffer or global command.
 */
static int
ex_gat(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	GAT *gatp, *tp;
	RANGE *rp;

	/*
	 * Lose any exhausted ranges/commands.  If no lines are left to be
	 * executed, restore the saved command.  If no saved command, set
	 * the line number, discard the gat structure, and move to the next
	 * one.  If no next one, we're done.
	 */
	while ((gatp = EXP(sp)->gatq.lh_first) != NULL) {
		while ((rp = gatp->rangeq.cqh_first) != (void *)&gatp->rangeq)
			if (rp->start > rp->stop) {
				CIRCLEQ_REMOVE(&gatp->rangeq, rp, q);
				free(rp);
			} else
				break;
		if (rp == (void *)&gatp->rangeq) {
			/* If there was a saved command, execute it. */
			if (gatp->save_cmdlen != 0) {
				cmdp->cp = gatp->save_cmd;
				cmdp->cplen = gatp->save_cmdlen;
				gatp->save_cmdlen = 0;
				return (0);
			}

			/*
			 * If it's a global command, fix up the last line,
			 * and turn off the global flag if there are no
			 * more global commands.
			 */
			if (F_ISSET(gatp, GAT_ISGLOBAL | GAT_ISV)) {
				if (gatp->range_lno != OOBLNO)
					if (file_eline(sp, gatp->range_lno))
						sp->lno = gatp->range_lno;
					else {
						if (file_lline(sp, &sp->lno))
							return (1);
						if (sp->lno == 0)
							sp->lno = 1;
					}
				for (tp = gatp->q.le_next;
				    tp != NULL; tp = tp->q.le_next)
					if (F_ISSET(tp, GAT_ISGLOBAL | GAT_ISV))
						break;
				if (tp == NULL)
					F_CLR(sp, S_EX_GLOBAL);
			}

			/* Free allocated memory. */
			if (gatp->save_cmd != NULL)
				free(gatp->save_cmd);
			free(gatp->cmd);
			LIST_REMOVE(gatp, q);
			free(gatp);
		} else
			break;
	}

	if (gatp == NULL)
		return (0);

	/*
	 * !!!
	 * Globals set the current line number, @ buffers don't.  Set the
	 * global execution flag.
	 */
	if (F_ISSET(gatp, GAT_ISGLOBAL | GAT_ISV)) {
		F_SET(sp, S_EX_GLOBAL);
		gatp->range_lno = sp->lno = rp->start;
	}
	++rp->start;

	/* Get a fresh copy of the command and point the parser at it. */
	memmove(gatp->cmd + gatp->cmd_len, gatp->cmd, gatp->cmd_len);
	cmdp->cp = gatp->cmd + gatp->cmd_len;
	cmdp->cplen = gatp->cmd_len;

	return (0);
}

/*
 * ex_gatfree --
 *	Discard the linked list of gat's.
 */
static int
ex_gatfree(sp)
	SCR *sp;
{
	GAT *gatp;
	RANGE *rp;

	while ((gatp = EXP(sp)->gatq.lh_first) != NULL) {
		while ((rp = gatp->rangeq.cqh_first) != (void *)&gatp->rangeq) {
			CIRCLEQ_REMOVE(&gatp->rangeq, rp, q);
			free(rp);
		}
		if (gatp->save_cmd != NULL)
			free(gatp->save_cmd);
		free(gatp->cmd);
		LIST_REMOVE(gatp, q);
		free(gatp);
	}
	return (0);
}

/*
 * ex_unknown --
 *	Display an unknown command name.
 */
static void
ex_unknown(sp, cmd, len)
	SCR *sp;
	char *cmd;
	size_t len;
{
	size_t blen;
	int nf;
	char *bp, *p;

	GET_SPACE_GOTO(sp, bp, blen, len + 1);
	bp[len] = '\0';
	memmove(bp, cmd, len);
	p = msg_print(sp, bp, &nf);
	msgq(sp, M_ERR, "100|The %s command is unknown", p);
	if (nf)
		FREE_SPACE(sp, p, 0);
	FREE_SPACE(sp, bp, blen);

binc_err:
	return;
}

/*
 * ex_is_abbrev -
 *	The vi text input routine needs to know if ex thinks this is
 *	an [un]abbreviate command, so it can turn off abbreviations.
 *	Usual ranting in the vi/v_txt_ev.c:txt_abbrev() routine.
 */
int
ex_is_abbrev(name, len)
	char *name;
	size_t len;
{
	EXCMDLIST const *cp;

	return ((cp = ex_comm_search(name, len)) != NULL &&
	    (cp == &cmds[C_ABBR] || cp == &cmds[C_UNABBREVIATE]));
}

/*
 * ex_is_unmap -
 *	The vi text input routine needs to know if ex thinks this is
 *	an unmap command, so it can turn off input mapping.  Usual
 *	ranting in the vi/v_txt_ev.c:txt_unmap() routine.
 */
int
ex_is_unmap(name, len)
	char *name;
	size_t len;
{
	EXCMDLIST const *cp;

	/*
	 * The command the vi input routines are really interested in
	 * is "unmap!", not just unmap.
	 */
	if (name[len - 1] != '!')
		return (0);
	--len;
	return ((cp = ex_comm_search(name, len)) != NULL &&
	    cp == &cmds[C_UNMAP]);
}

static EXCMDLIST const *
ex_comm_search(name, len)
	char *name;
	size_t len;
{
	EXCMDLIST const *cp;

	for (cp = cmds; cp->name != NULL; ++cp) {
		if (cp->name[0] > name[0])
			return (NULL);
		if (cp->name[0] != name[0])
			continue;
		if (!memcmp(name, cp->name, len))
			return (cp);
	}
	return (NULL);
}

void
ex_badaddr(sp, cp, ba, nret)
	SCR *sp;
	EXCMDLIST const *cp;
	enum badaddr ba;
	enum nresult nret;
{
	recno_t lno;

	switch (nret) {
	case NUM_OK:
		break;
	case NUM_ERR:
		msgq(sp, M_SYSERR, NULL);
		return;
	case NUM_OVER:
		msgq(sp, M_ERR, "254|Address value overflow");
		return;
	case NUM_UNDER:
		msgq(sp, M_ERR, "255|Address value underflow");
		return;
	}

	/*
	 * When encountering an address error, tell the user if there's no
	 * underlying file, that's the real problem.
	 */
	if (sp->ep == NULL) {
		ex_message(sp, cp->name, EXM_NORC);
		return;
	}

	switch (ba) {
	case A_COMBO:
		msgq(sp, M_ERR, "253|Illegal address combination");
		break;
	case A_EOF:
		if (file_lline(sp, &lno))
			return;
		if (lno != 0) {
			msgq(sp, M_ERR,
			    "119|Illegal address: only %lu lines in the file",
			    lno);
			break;
		}
		/* FALLTHROUGH */
	case A_EMPTY:
		msgq(sp, M_ERR, "118|Illegal address: the file is empty");
		break;
	case A_NOTSET:
		abort();
		/* NOTREACHED */
	case A_ZERO:
		msgq(sp, M_ERR,
		    "108|The %s command doesn't permit an address of 0",
		    cp->name);
		break;
	}
	return;
}

#if defined(DEBUG) && defined(COMLOG)
static void
ex_comlog(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	TRACE(sp, "ecmd: %s", cmdp->cmd->name);
	if (cmdp->addrcnt > 0) {
		TRACE(sp, " a1 %d", cmdp->addr1.lno);
		if (cmdp->addrcnt > 1)
			TRACE(sp, " a2: %d", cmdp->addr2.lno);
	}
	if (cmdp->lineno)
		TRACE(sp, " line %d", cmdp->lineno);
	if (cmdp->flags)
		TRACE(sp, " flags 0x%x", cmdp->flags);
	if (F_ISSET(&exc, E_BUFFER))
		TRACE(sp, " buffer %c", cmdp->buffer);
	if (cmdp->argc)
		for (cnt = 0; cnt < cmdp->argc; ++cnt)
			TRACE(sp, " arg %d: {%s}", cnt, cmdp->argv[cnt]->bp);
	TRACE(sp, "\n");
}
#endif
