/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: v_event.c,v 8.13 1996/12/17 19:49:19 bostic Exp $ (Berkeley) $Date: 1996/12/17 19:49:19 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "../ip/ip.h"
#include "vi.h"

/*
 * v_ec_settop --
 *	Scrollbar position.
 */
static int
v_ec_settop(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	SMAP *smp;

	/*
	 * We want to scroll the screen, without changing the cursor position.
	 * So, we fill the screen map and then flush it to the screen.  Then,
	 * set the VIP_S_REFRESH flag so the main vi loop doesn't update the
	 * screen.  When the next real command happens, the refresh code will
	 * notice that the screen map is way wrong and fix it.
	 *
	 * XXX
	 * There may be a serious performance problem here -- we're doing no
	 * optimization whatsoever, which means that we're copying the entire
	 * screen out to the X11 screen code on each change.
	 */
	if (vs_sm_fill(sp, vp->ev.e_lno, P_TOP))
		return (1);
	for (smp = HMAP; smp <= TMAP; ++smp) {
                SMAP_FLUSH(smp);
		if (vs_line(sp, smp, NULL, NULL))
			return (1);
        }

	F_SET(VIP(sp), VIP_S_REFRESH);

	return (sp->gp->scr_refresh(sp, 0));
}

/*
 * v_eedit --
 *	Edit command.
 */
static int
v_eedit(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0);
	argv_exp0(sp, &cmd, vp->ev.e_csp, vp->ev.e_len);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_eeditopt --
 *	Set an option value.
 */
static int
v_eeditopt(sp, vp)
	SCR *sp;
	VICMD *vp;
{

	return (api_opts_set(sp,
	    vp->ev.e_str1, vp->ev.e_str2, vp->ev.e_val1, vp->ev.e_val1));
}

/*
 * v_eeditsplit --
 *	Edit in a split screen.
 */
static int
v_eeditsplit(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0);
	F_SET(&cmd, E_NEWSCREEN);
	argv_exp0(sp, &cmd, vp->ev.e_csp, vp->ev.e_len);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_etag --
 *	Tag command.
 */
static int
v_etag(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	EXCMD cmd;

	if (v_curword(sp))
		return (1);

	ex_cinit(sp, &cmd, C_TAG, 0, OOBLNO, OOBLNO, 0);
	argv_exp0(sp, &cmd, VIP(sp)->keyw, strlen(VIP(sp)->keyw));
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_etagas --
 *	Tag on the supplied string.
 */
static int
v_etagas(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_TAG, 0, OOBLNO, OOBLNO, 0);
	argv_exp0(sp, &cmd, vp->ev.e_csp, vp->ev.e_len);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_etagsplit --
 *	Tag in a split screen.
 */
static int
v_etagsplit(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	EXCMD cmd;

	if (v_curword(sp))
		return (1);

	ex_cinit(sp, &cmd, C_TAG, 0, OOBLNO, OOBLNO, 0);
	F_SET(&cmd, E_NEWSCREEN);
	argv_exp0(sp, &cmd, VIP(sp)->keyw, strlen(VIP(sp)->keyw));
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_equit --
 *	Quit command.
 */
static int
v_equit(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_QUIT, 0, OOBLNO, OOBLNO, 0);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_erepaint --
 *	Repaint selected lines from the screen.
 *
 * PUBLIC: int v_erepaint __P((SCR *, EVENT *));
 */
int
v_erepaint(sp, evp)
	SCR *sp;
	EVENT *evp;
{
	SMAP *smp;

	for (; evp->e_flno <= evp->e_tlno; ++evp->e_flno) {
		smp = HMAP + evp->e_flno - 1;
		SMAP_FLUSH(smp);
		if (vs_line(sp, smp, NULL, NULL))
			return (1);
	}
	return (0);
}

/*
 * v_ewq --
 *	Write and quit command.
 */
static int
v_ewq(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_WQ, 0, OOBLNO, OOBLNO, 0);

	cmd.addr1.lno = 1;
	if (db_last(sp, &cmd.addr2.lno))
		return (1);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_ewrite --
 *	Write command.
 */
static int
v_ewrite(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_WRITE, 0, OOBLNO, OOBLNO, 0);

	cmd.addr1.lno = 1;
	if (db_last(sp, &cmd.addr2.lno))
		return (1);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_ewriteas --
 *	Write command.
 */
static int
v_ewriteas(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	EXCMD cmd;

	ex_cinit(sp, &cmd, C_WRITE, 0, OOBLNO, OOBLNO, 0);
	argv_exp0(sp, &cmd, vp->ev.e_csp, vp->ev.e_len);

	cmd.addr1.lno = 1;
	if (db_last(sp, &cmd.addr2.lno))
		return (1);
	return (v_exec_ex(sp, vp, &cmd));
}

/*
 * v_event --
 *	Find the event associated with a fucntion.
 */
int
v_event(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	/* This array maps events to vi command functions. */
	static VIKEYS const vievents[] = {
#define	V_C_SETTOP	 0				/* VI_C_SETTOP */
		{v_ec_settop,	0},
#define	V_EEDIT		 1				/* VI_EDIT */
		{v_eedit,	0},
#define	V_EEDITOPT	 2				/* VI_EDITOPT */
		{v_eeditopt,	0},
#define	V_EEDITSPLIT	 3				/* VI_EDITSPLIT */
		{v_eeditsplit,	0},
#define	V_EMARK		 4				/* VI_MOUSE_MOVE */
		{v_emark,	V_ABS_L|V_MOVE},
#define	V_EQUIT		 5				/* VI_QUIT */
		{v_equit,	0},
#define	V_ESEARCH	 6				/* VI_SEARCH */
		{v_esearch,	V_ABS_L|V_MOVE},
#define	V_ETAG		 7				/* VI_TAG */
		{v_etag,	0},
#define	V_ETAGAS	 8				/* VI_TAGAS */
		{v_etagas,	0},
#define	V_ETAGSPLIT	 9				/* VI_TAGSPLIT */
		{v_etagsplit,	0},
#define	V_EWQ		10				/* VI_WQ */
		{v_ewq,		0},
#define	V_EWRITE	11				/* VI_WRITE */
		{v_ewrite,	0},
#define	V_EWRITEAS	12				/* VI_WRITEAS */
		{v_ewriteas,	0},
	};

	switch (vp->ev.e_ipcom) {
	case VI_C_BOL:
		vp->kp = &vikeys['0'];
		break;
	case VI_C_BOTTOM:
		vp->kp = &vikeys['G'];
		break;
	case VI_C_DEL:
		vp->kp = &vikeys['x'];
		break;
	case VI_C_DOWN:
		F_SET(vp, VC_C1SET);
		vp->count = vp->ev.e_lno;
		vp->kp = &vikeys['\012'];
		break;
	case VI_C_EOL:
		vp->kp = &vikeys['$'];
		break;
	case VI_C_INSERT:
		vp->kp = &vikeys['i'];
		break;
	case VI_C_LEFT:
		vp->kp = &vikeys['\010'];
		break;
	case VI_C_PGDOWN:
		F_SET(vp, VC_C1SET);
		vp->count = vp->ev.e_lno;
		vp->kp = &vikeys['\006'];
		break;
	case VI_C_PGUP:
		F_SET(vp, VC_C1SET);
		vp->count = vp->ev.e_lno;
		vp->kp = &vikeys['\002'];
		break;
	case VI_C_RIGHT:
		vp->kp = &vikeys['\040'];
		break;
	case VI_C_SEARCH:
		vp->kp = &vievents[V_ESEARCH];
		break;
	case VI_C_SETTOP:
		vp->kp = &vievents[V_C_SETTOP];
		break;
	case VI_C_TOP:
		F_SET(vp, VC_C1SET);
		vp->count = 1;
		vp->kp = &vikeys['G'];
		break;
	case VI_C_UP:
		F_SET(vp, VC_C1SET);
		vp->count = vp->ev.e_lno;
		vp->kp = &vikeys['\020'];
		break;
	case VI_EDIT:
		vp->kp = &vievents[V_EEDIT];
		break;
	case VI_EDITOPT:
		vp->kp = &vievents[V_EEDITOPT];
		break;
	case VI_EDITSPLIT:
		vp->kp = &vievents[V_EEDITSPLIT];
		break;
	case VI_MOUSE_MOVE:
		vp->kp = &vievents[V_EMARK];
		break;
	case VI_QUIT:
		vp->kp = &vievents[V_EQUIT];
		break;
	case VI_TAG:
		vp->kp = &vievents[V_ETAG];
		break;
	case VI_TAGAS:
		vp->kp = &vievents[V_ETAGAS];
		break;
	case VI_TAGSPLIT:
		vp->kp = &vievents[V_ETAGSPLIT];
		break;
	case VI_UNDO:
		vp->kp = &vikeys['u'];
		break;
	case VI_WQ:
		vp->kp = &vievents[V_EWQ];
		break;
	case VI_WRITE:
		vp->kp = &vievents[V_EWRITE];
		break;
	case VI_WRITEAS:
		vp->kp = &vievents[V_EWRITEAS];
		break;
	default:
		return (1);
	}
	return (0);
}
