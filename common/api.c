/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: api.c,v 8.2 1995/02/16 13:09:06 bostic Exp $ (Berkeley) $Date: 1995/02/16 13:09:06 $";
#endif /* not lint */

#include "api.h"

/*
 * api_ex_exec --
 *	Execute ex commands.
 */
int
api_ex_exec(sp, cmd, len)
	SCR *sp;
	char *cmd;
	size_t len;
{
	return (ex_cmd(sp, cmd, len, 0));
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
	return ((lpp = file_gline(sp, lno, lenp)) == NULL ? 1 : 0);
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
	return (file_dline(sp, lno));
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
	return (file_aline(sp, 0, lno, lp, len));
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
	return (file_iline(sp, lno, lp, len));
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
	return (file_sline(sp, lno, lp, len));
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
	return (file_lline(sp, lnop));
}

/*
 * api_getcb --
 *	Get the cut buffer named key.
 */
int
api_getcb(sp, key, cbpp)
	SCR *sp;
	ARGCHAR_T key;
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
	ARGCHAR_T key;
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
int
api_getmark(sp, key, mp)
	SCR *sp;
	ARGCHAR_T key;
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
	ARGCHAR_T key;
	MARK *mp;
{
	return (mark_set(sp, key, mp, 1));
}

/*
 * api_iscreen --
 *	Create a new screen.
 */
int
api_iscreen(sp, nspp)
	SCR *sp, **nspp;
{
	SCR *bot, *top;

	if (svi_split(sp, &top, &bot))
		return (1);
	*nspp = sp == top ? bot : top;
	return (0);
}

/*
 * api_escreen --
 *	End a screen.
 */
int
api_escreen(sp)
	SCR *sp;
{
	F_SET(sp, S_EXIT);
	return (0);
}

/*
 * api_swscreen --
 *	Switch to a new screen.
 */
int
api_swscreen(sp, new)
	SCR *sp, *new;
{
	sp->nextdisp = new;
	F_SET(sp, S_SSWITCH);
	return (0);
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
 *	Get the cursor.
 */
int
api_setcur(sp, mp)
	SCR *sp;
	MARK *mp;
{
	sp->lno = mp->lno;
	sp->cno = mp->cno;
	return (0);
}

/*
 * api_getch --
 *	Get a character from the user.
 */
int
api_getch(chp, mt, flags)
	IKEY *ikeyp;
	enum maptype mapt;
{
	return (term_key(sp, ikeyp, flags));
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
