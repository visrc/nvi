/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_bang.c,v 8.10 1993/11/02 18:46:47 bostic Exp $ (Berkeley) $Date: 1993/11/02 18:46:47 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "sex/sex_screen.h"

/*
 * ex_bang -- :[line [,line]] ! command
 *	Pass the rest of the line after the ! character to
 *	the program named by the SHELL environment variable.
 */
int
ex_bang(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	register int ch, len, modified;
	register char *p, *t;
	enum filtertype ftype;
	recno_t lno;
	MARK rm;
	int rval;
	char *com;

	/* Make sure we got something. */
	if (cmdp->argv[0][0] == '\0') {
		msgq(sp, M_ERR, "Usage: %s", cmdp->cmd->usage);
		return (1);
	}

	/* Figure out how much space we could possibly need. */
	modified = 0;
	len = strlen(cmdp->argv[0]) + 1;
	for (p = cmdp->argv[0]; (p = strpbrk(p, "!%#\\")) != NULL; ++p)
		switch (*p) {
		case '!':
			if (sp->lastbcomm == NULL) {
				msgq(sp, M_ERR,
				    "No previous command to replace \"!\".");
				return (1);
			}
			len += strlen(sp->lastbcomm);
			modified = 1;
			break;
		case '%':
			if (F_ISSET(sp->frp, FR_NONAME)) {
				msgq(sp, M_ERR,
				    "No filename to substitute for %%.");
				return (1);
			}
			len += sp->frp->nlen;
			modified = 1;
			break;
		case '#':
			if (F_ISSET(sp->frp, FR_NONAME)) {
				msgq(sp, M_ERR,
				    "No filename to substitute for #.");
				return (1);
			}
			len += sp->frp->nlen;
			modified = 1;
			break;
		case '\\':
			if (*p)
				++p;
			break;
		}

	/* Allocate space. */
	if ((com = malloc(len)) == NULL) {
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		return (1);
	}

	/* Fill it in. */
	if (modified) {
		for (p = cmdp->argv[0], t = com; (ch = *p) != '\0'; ++p)
			switch (ch) {
			case '!':
				len = strlen(sp->lastbcomm);
				memmove(t, sp->lastbcomm, len);
				t += len;
				break;
			case '%':
				memmove(t, sp->frp->fname, sp->frp->nlen);
				t += sp->frp->nlen;
				break;
			case '#':
				memmove(t, sp->frp->fname, sp->frp->nlen);
				t += sp->frp->nlen;
				break;
			case '\\':
				if (*p)
					ch = *++p;
				/* FALLTHROUGH */
			default:
				*t++ = ch;
			}
		*t = '\0';
	} else
		memmove(com, cmdp->argv[0], len);

	/* Swap commands. */
	if (sp->lastbcomm != NULL)
		free(sp->lastbcomm);
	sp->lastbcomm = com;

	if (modified)
		(void)ex_printf(EXCOOKIE, "%s\n", com);

	/*
	 * If addresses were specified, pipe lines from the file through
	 * the command.
	 */
	if (cmdp->addrcnt != 0) {
		/*
		 * !!!
		 * Historical vi permitted "!!" in an empty file, for no
		 * immediately apparent reason.  When it happens, we end
		 * up here with line addresses of 0,1 and a bad attitude.
		 */
		ftype = FILTER;
		if (cmdp->addr1.lno == 0 && cmdp->addr2.lno == 1) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno == 0) {
				cmdp->addr1.lno = cmdp->addr2.lno = 0;
				ftype = FILTER_READ;
			}
		}
		if (filtercmd(sp, ep,
		    &cmdp->addr1, &cmdp->addr2, &rm, com, ftype))
			return (1);
		sp->lno = rm.lno;
		F_SET(sp, S_AUTOPRINT);
		return (0);
	}

	/*
	 * If no addresses were specified, run the command.  If the file
	 * has been modified and autowrite is set, write the file back.
	 * If the file has been modified, autowrite is not set and the
	 * warn option is set, tell the user about the file.
	 */
	if (F_ISSET(ep, F_MODIFIED)) {
		if (O_ISSET(sp, O_AUTOWRITE)) {
			if (file_write(sp, ep, NULL, NULL, NULL, FS_ALL))
				return (1);
		} else if (O_ISSET(sp, O_WARN))
			msgq(sp, M_ERR, "Modified since last write.");
		if (sex_refresh(sp, ep))
			return (1);
	}

	/* Run the command. */
	rval = ex_exec_process(sp, O_STR(sp, O_SHELL), com, 1);

	/* Ex terminates with a bang. */
	if (F_ISSET(sp, S_MODE_EX))
		(void)fprintf(stdout, "!\n");
	return (rval);
}
