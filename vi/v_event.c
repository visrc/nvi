/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: v_event.c,v 8.5 1996/12/05 21:04:24 bostic Exp $ (Berkeley) $Date: 1996/12/05 21:04:24 $";
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
#include "../ip_vi/ip.h"
#include "vi.h"

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

	ex_cinit(sp, &cmd, C_TAG, 0, OOBLNO, OOBLNO, 0);
	F_SET(&cmd, E_NEWSCREEN);
	argv_exp0(sp, &cmd, vp->ev.e_csp, vp->ev.e_len);
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
		{v_eedit,	0},			/* IPO_EDIT */
		{v_eeditsplit,	0},			/* IPO_EDITSPLIT */
		{v_emark,	V_ABS_L|V_MOVE},	/* IPO_MOUSE_MOVE */
		{v_equit,	0},			/* IPO_QUIT */
		{v_etag,	0},			/* IPO_TAG */
		{v_etagas,	0},			/* IPO_TAGAS */
		{v_etagsplit,	0},			/* IPO_TAGSPLIT */
		{v_ewq,		0},			/* IPO_WQ */
		{v_ewrite,	0},			/* IPO_WRITE */
		{v_ewriteas,	0},			/* IPO_WRITEAS */
	};

	switch (vp->ev.e_ipcom) {
	case IPO_EDIT:
		vp->kp = &vievents[0];
		break;
	case IPO_EDITSPLIT:
		vp->kp = &vievents[1];
		break;
	case IPO_MOUSE_MOVE:
		vp->kp = &vievents[2];
		break;
	case IPO_QUIT:
		vp->kp = &vievents[3];
		break;
	case IPO_TAG:
		vp->kp = &vievents[4];
		break;
	case IPO_TAGAS:
		vp->kp = &vievents[5];
		break;
	case IPO_TAGSPLIT:
		vp->kp = &vievents[6];
		break;
	case IPO_UNDO:
		vp->kp = &vikeys['u'];
		break;
	case IPO_WQ:
		vp->kp = &vievents[7];
		break;
	case IPO_WRITE:
		vp->kp = &vievents[8];
		break;
	case IPO_WRITEAS:
		vp->kp = &vievents[9];
		break;
	default:
		return (1);
	}
	return (0);
}
