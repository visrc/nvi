/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: v_event.c,v 8.3 1996/12/04 19:25:00 bostic Exp $ (Berkeley) $Date: 1996/12/04 19:25:00 $";
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
#include "vi.h"

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

	ex_cinit(&cmd, C_QUIT, 0, OOBLNO, OOBLNO, 0, NULL);
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

	ex_cinit(&cmd, C_WQ, 0, OOBLNO, OOBLNO, 0, NULL);
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

	ex_cinit(&cmd, C_WRITE, 0, OOBLNO, OOBLNO, 0, NULL);
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

	ex_cinit(&cmd, C_WRITE, 0, OOBLNO, OOBLNO, 0, NULL);
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
		{v_emark,	V_ABS_L|V_MOVE},	/* E_MOVE */
		{v_equit,	0},			/* E_QUIT */
		{v_ewq,		0},			/* E_WQ */
		{v_ewrite,	0},			/* E_WRITE */
		{v_ewriteas,	0},			/* E_WRITEAS */
	};

	switch (vp->ev.e_event) {
	case E_MOVE:
		vp->kp = &vievents[0];
		break;
	case E_QUIT:
		vp->kp = &vievents[1];
		break;
	case E_WQ:
		vp->kp = &vievents[2];
		break;
	case E_WRITE:
		vp->kp = &vievents[4];
		break;
	case E_WRITEAS:
		vp->kp = &vievents[5];
		break;
	default:
		return (1);
	}
	return (0);
}
