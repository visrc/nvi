/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: v_event.c,v 8.1 1996/12/04 10:03:09 bostic Exp $ (Berkeley) $Date: 1996/12/04 10:03:09 $";
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
 * This array maps events to vi command functions.
 */
static VIKEYS const vievents[] = {
/* 0	E_MOVE */
	{v_mmark,	V_ABS_L|V_MOVE},
/* 1	E_QUIT */
	{v_equit},
/* 2	E_WQ */
	{v_ewq},
/* 3	E_WRESIZE */
	{NULL},
/* 4	E_WRITE */
	{v_ewrite},
/* 5	E_WRITEAS */
	{v_ewriteas},
};

/*
 * v_event --
 *	Find the event associated with a fucntion.
 */
int
v_event(sp, vp)
	SCR *sp;
	VICMD *vp;
{
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
	case E_WRESIZE:
		vp->kp = &vievents[3];
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
