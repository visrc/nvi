/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)api.c	8.3 (Berkeley) 4/13/95";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <bitstring.h>
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
#include <pathnames.h>

#include "common.h"
#include "vi.h"
#include "ex_define.h"
#include "ex_extern.h"

/*
 * api_gscr --
 *	Get the screen that called the interpreter.
 */
SCR *
api_gscr()
{
	extern GS *__global_list;
	return (__global_list->interpScr);
}

/*
 * api_ex_exec --
 *	Execute ex commands.
 */
int
api_ex_exec(sp, cmd, args, len)
	SCR *sp;
	char *cmd;
	size_t len;
{
	return (ex_run_str(sp, "API", cmd, len, 1));
}
/*
 * api_vi_exec --
 *	Execute vi commands.
 */
int
api_vi_exec(sp, cmd, len)
	SCR *sp;
	char *cmd;
	size_t len;
{
	/*
	 * XXX
	 * This may be hard.  We may want to do this by simply pushing
	 * the string onto the terminal stack, although that's not going
	 * to be optimal.
	 */
	return (0);
}

/*
 * api_gline --
 *	Store the pointer to line lno in lpp, its length in lenp.
 */
int
api_gline(sp, lno, lpp, lenp)
	SCR *sp;
	recno_t lno;
	char **lpp;
	size_t *lenp;
{
	return (db_get(sp, lno, 0, lpp, lenp));
}

/*
 * api_dline --
 *	Delete line lno.
 */
int
api_dline(sp, lno)
	SCR *sp;
	recno_t lno;
{
	return (db_delete(sp, lno));
}

/* 
 * api_aline --
 *	Append text pointed to by lp, of length len, after line lno.
 */
int
api_aline(sp, lno, lp, len)
	SCR *sp;
	recno_t lno;
	char *lp;
	size_t len;
{
	return (db_append(sp, 1, lno, lp, len));
}

/* 
 * api_iline --
 *	Insert text pointed to by lp, of length len, before line lno.
 */
int
api_iline(sp, lno, lp, len)
	SCR *sp;
	recno_t lno;
	char *lp;
	size_t len;
{
	return (db_insert(sp, lno, lp, len));
}

/* 
 * api_sline --
 *	Set line lno to text pointed to by lp, of length len.
 */
int
api_sline(sp, lno, lp, len)
	SCR *sp;
	recno_t lno;
	char *lp;
	size_t len;
{
	return (db_set(sp, lno, lp, len));
}

/*
 * api_lline --
 *	Return the line number of the last line in the file.
 */
int
api_lline(sp, lnop)
	SCR *sp;
	recno_t *lnop;
{
	return (db_last(sp, lnop));
}

/*
 * api_getcb --
 *	Get the cut buffer named key.
 */
int
api_getcb(sp, key, cbpp)
	SCR *sp;
	ARG_CHAR_T key;
	CB **cbpp;
{
	CB *cbp;

	CBNAME(sp, cbp, key);
	*cbpp = cbp;
	return (0);
}

/*
 * api_setcb --
 *	Set the cut buffer named key.
 */
int
api_setcb(sp, key, cbp)
	SCR *sp;
	ARG_CHAR_T key;
	CB *cbp;
{
	/*
	 * XXX
	 * This one may be hard, I'll have to think about it.
	 */
	return (0);
}

/*
 * api_getmark --
 *	Get the mark named key to contents of mp.
 */
int
api_getmark(sp, key, mp)
	SCR *sp;
	ARG_CHAR_T key;
	MARK *mp;
{
	return (mark_get(sp, key, mp, M_ERR));
}

/*
 * api_setmark --
 *	Set the mark named key to contents of mp.
 */
int
api_setmark(sp, key, mp)
	SCR *sp;
	ARG_CHAR_T key;
	MARK *mp;
{
	return (mark_set(sp, key, mp, 1));
}

/*
 * api_getcur --
 *	Get the cursor.
 */
int
api_getcur(sp, mp)
	SCR *sp;
	MARK *mp;
{
	mp->lno = sp->lno;
	mp->cno = sp->cno;
	return (0);
}

/*
 * api_setcur --
 *	Set the cursor.
 */
int
api_setcur(sp, mp)
	SCR *sp;
	MARK mp;
{
	sp->lno = mp.lno;
	sp->cno = mp.cno;
	return (0);
}

/*
 * api_getline --
 *	Get a line of text from the user.
 */
int
api_getline(sp, prompt)
	SCR *sp;
	char *prompt;
{
	/*
	 * XXX
	 * This one may be hard, I'll have to think about it.
	 */
	return (0);
}
	
/*
 * api_getmotion --
 *	Get a motion command from the user.
 */
int
api_getmotion(sp)
	SCR *sp;
{
	/*
	 * XXX
	 * This one may be hard, I'll have to think about it.
	 */
	return (0);
}

/*
 * Send a simple textual message, without varargs
 *
 */

void 
api_msgq(sp, mt, msg)
        SCR *sp;
	mtype_t mt;
	char *msg;
{
	msgq(sp, mt, "%s", msg);
}

/*
 * Interface to the mapping code so that we can remap keys.
 *
 */

int
api_map(sp, name, cmd)
	SCR *sp;
	CHAR_T *name;
	char *cmd;
{
	/*
	 * This is mostly a rip off of the mapping code from ex/ex_map.c
	 * Ripping it off is easier than building up an EXCMD structure
	 * with associated mallocs and tearing it down with free.
	 */

	CHAR_T *p;

	/*
	 * If the mapped string is #[0-9]* (and wasn't quoted) then store the
	 * function key mapping.  If the screen specific routine has been set,
	 * call it as well.  Note, the SEQ_FUNCMAP type is persistent across
	 * screen types, maybe the next screen type will get it right.
	 */
	if (name[0] == '#' && isdigit(name[1])) {
		for (p = name + 2; isdigit(*p); ++p);
		if (p[0] != '\0')
			goto nofunc;

		if (seq_set(sp, NULL, 0, name, strlen(name),
		    cmd, strlen(cmd), SEQ_COMMAND,
		    SEQ_FUNCMAP | SEQ_USERDEF))
			return (1);
		return (sp->gp->scr_fmap == NULL ? 0 :
		    sp->gp->scr_fmap(sp, SEQ_COMMAND, name, strlen(name),
		    cmd, strlen(cmd)));
	}

	/* Some single keys may not be remapped in command mode. */
nofunc:	if (name[1] == '\0')
		switch (KEY_VAL(sp, name[0])) {
		case K_COLON:
		case K_ESCAPE:
		case K_NL:
			msgq(sp, M_ERR,
			    "134|The %s character may not be remapped",
			    KEY_NAME(sp, name[0]));
			return (1);
		}
	return (seq_set(sp, NULL, 0, name, strlen(name),
	    cmd, strlen(cmd), SEQ_COMMAND, SEQ_USERDEF));
}

int 
api_unmap(sp, name)
	SCR *sp;
	CHAR_T *name;
{
	char command[32];

	sprintf(command, "unmap %s", name);
	return (api_ex_exec(sp, command, strlen(command)));
}

int 
api_opts_set(sp, name)
	SCR *sp;
	char *name;
{
	int status;
	ARGS **argv;

	/*
	 * Here we form a proper argv structure for the opts_set
	 * and hopefully it's getting de-allocated.
	 */

	argv = malloc(2 * sizeof(ARGS *));
	argv[0] = malloc(sizeof(ARGS));
	argv[1] = malloc(sizeof(ARGS));
	argv[0]->bp = strdup(name);
	argv[0]->blen = strlen(name) + 1;
	argv[0]->len = strlen(name);
	argv[0]->flags = A_ALLOCATED; 
	argv[1]->blen = 0;
	argv[1]->len = 0;
	argv[1]->flags = A_ALLOCATED; 

	status = opts_set(sp, argv, 0, NULL);
	free(argv[0]->bp);
	free(argv[1]);
	free(argv[0]);
	free(argv);
	return(status);
}

int 
api_opt_get(sp, target, value)
	SCR *sp;
	CHAR_T *target;
	CHAR_T **value;
{
	return (opt_get(sp, target, value));
}

/*
 * api_iscreen
 *	Create a new screen and return a string that is the pointer
 *	to it.
 */

int
api_iscreen(sp, screen, file)
	SCR *sp;
	char *screen;
	char *file;
{
	SCR *new;
	FREF *frp;
	int attach;

	if (!screen)
		return 1;

	if (screen_init(sp->gp, sp, &new)) {
		return (1);
	}

	if (vs_split(sp, new)) {
		screen_end(new);
		return (1);
	}

	if (file == NULL) {
		frp = sp->frp;
		if (sp->ep == NULL || F_ISSET(frp, FR_TMPFILE)) {
			if ((frp = file_add(sp, NULL)) == NULL)
				return (1);
			attach = 0;
		} else
			attach = 1;
	} else {
		if ((frp = file_add(sp, file)) == NULL)
			return (1);
		attach = 0;
		set_alt_name(sp, file);
	}	

	/* Get a backing file. */
	if (attach) {
		/* Copy file state, keep the screen and cursor the same. */
		new->ep = sp->ep;
		++new->ep->refcnt;

		new->frp = frp;
		new->frp->flags = sp->frp->flags;

		new->lno = sp->lno;
		new->cno = sp->cno;
	} else if (file_init(new, frp, NULL, 0)) {
		(void)vs_discard(new, NULL);
		(void)screen_end(new);
		return (1);
	}

	/* Set up the switch. */
	sp->nextdisp = new;
	F_SET(sp, S_SSWITCH);
	
	sprintf(screen, "%ld", new);
	return 0;
}

/*
 * api_escreen
 *	End a screen.
 */
int
api_escreen(sp)
	SCR *sp;
{
	SCR *new;

	return(vs_discard(sp, &new));
}

/*
 * api_swscreen --
 *    Switch to a new screen.
 */
int
api_swscreen(sp, new)
      SCR *sp, *new;
{
      sp->nextdisp = new;
      F_SET(sp, S_SSWITCH);
      return (0);
}

int 
api_fscreen(csp, name, screen)
	SCR *csp;
	CHAR_T *name, *screen;
{
	SCR *sp, *nsp;

	if (!screen)
		return (1);
	/* Search the displayed list first. */
	for (sp = csp->gp->dq.cqh_first;
	    sp != (void *)&csp->gp->hq; sp = sp->q.cqe_next)
		if (strcmp(sp->frp->name, name) == 0)
			break;
	if (sp != (void *)&csp->gp->dq) {
		sprintf(screen, "%ld", sp);
		return (0);
	}

	/* Search the hidden list first. */
	for (sp = csp->gp->hq.cqh_first;
	    sp != (void *)&csp->gp->hq; sp = sp->q.cqe_next)
		if (strcmp(sp->frp->name, name) == 0)
			break;
	if (sp == (void *)&csp->gp->hq) {
		return (1);
	}

	sprintf(screen, "%ld", sp);
	return (0);
}
