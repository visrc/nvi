/*-
 * Copyright (c) 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_bsd.c,v 8.8 1995/10/31 20:22:07 bostic Exp $ (Berkeley) $Date: 1995/10/31 20:22:07 $";
#endif /* not lint */

#ifdef BSD_CURSES_INTERFACE
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "../vi/vi.h"
#include "cl.h"

static char	*ke;				/* Keypad on. */
static char	*ks;				/* Keypad off. */
static char	*vb;				/* Visible bell string. */

/*
 * We want const if we can get it, there are large, read-only structures in
 * here.
 */
#ifndef	__STDC__
#define	const
#endif

/*
 * HP's support the entire System V curses package except for the tigetstr
 * and tigetnum functions.  Cthulu only knows why.
 */
#ifndef TIGETONLY
/*
 * beep --
 *
 * PUBLIC: void beep __P((void));
 */
void
beep()
{
	(void)write(1, "\007", 1);	/* '\a' */
}

/*
 * flash --
 *	Flash the screen.
 *
 * PUBLIC: void flash __P((void));
 */
void
flash()
{
	if (vb != NULL) {
		(void)tputs(vb, 1, cl_putchar);
		(void)fflush(stdout);
	} else
		beep();
}

/*
 * keypad --
 *	Put the keypad/cursor arrows into or out of application mode.
 *
 * PUBLIC: int keypad __P((void *, int));
 */
int
keypad(a, on)
	void *a;
	int on;
{
	char *p, *sbp, sbuf[128];

	sbp = sbuf;
	if ((p = tgetstr(on ? "ks" : "ke", &sbp)) != NULL) {
		(void)tputs(p, 0, cl_putchar);
		(void)fflush(stdout);
	}
	return (0);
}

/*
 * newterm --
 *	Create a new curses screen.
 *
 * PUBLIC: void *newterm __P((const char *, FILE *, FILE *));
 */
void *
newterm(a, b, c)
	const char *a;
	FILE *b, *c;
{
	return (initscr());
}

/*
 * setupterm --
 *	Set up terminal.
 *
 * PUBLIC: void setupterm __P((char *, int, int *));
 */
void
setupterm(ttype, fileno, errp)
	char *ttype;
	int fileno, *errp;
{
	static char buf[2048];
	char *p;

	if ((*errp = tgetent(buf, ttype)) > 0) {
		if (ke != NULL)
			free(ke);
		ke = ((p = tigetstr("ke")) == NULL) ? NULL : strdup(p);
		if (ks != NULL)
			free(ks);
		ks = ((p = tigetstr("ks")) == NULL) ? NULL : strdup(p);
		if (vb != NULL)
			free(vb);
		vb = ((p = tigetstr("vb")) == NULL) ? NULL : strdup(p);
	}
}
#endif /* TIGETONLY */

/* Terminfo-to-termcap translation table. */
typedef struct _tl {
	char *terminfo;			/* Terminfo name. */
	char *termcap;			/* Termcap name. */
} TL;
static const TL list[] = {
	"cols",		"co",		/* Terminal columns. */
	"cup",		"cm",		/* Cursor up. */
	"cuu1",		"up",		/* Cursor up. */
	"el",		"ce",		/* Clear to end-of-line. */
	"kcub1",  	"kl",		/* Cursor left. */
	"kcud1",	"kd",		/* Cursor down. */
	"kcuf1",	"kr",		/* Cursor right. */
	"kcuu1",  	"ku",		/* Cursor up. */
	"kdch1",	"kD",		/* Delete character. */
	"kdl1",		"kL",		/* Delete line. */
	"ked",		"kS",		/* Delete to end of screen. */
	"kel",		"kE",		/* Delete to eol. */
	"khome",	"kh",		/* Go to sol. */
	"kich1",	"kI",		/* Insert at cursor. */
	"kil1",		"kA",		/* Insert line. */
	"kind",		"kF",		/* Scroll down. */
	"kll",		"kH",		/* Go to eol. */
	"knp",		"kN",		/* Page down. */
	"kpp",		"kP",		/* Page up. */
	"kri",		"kR",		/* Scroll up. */
	"lines",	"li",		/* Terminal lines. */
	"rmso",		"se",		/* Standout end. */
	"smso",		"so",		/* Standout begin. */
};

/*
 * !!!
 * Historically, the 4BSD termcap code didn't support functions keys greater
 * than 9.  This was silently enforced -- asking for key k12 would return the
 * value for k1.  We try and get around this by using the tables specified in
 * the terminfo(TI_ENV) man page from the 3rd Edition SVID.  This assumes the
 * implementors of any System V compatibility code or an extended termcap used
 * those codes.
 */
static const char codes[] = {
/*  0-10 */ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ';',
/* 11-19 */ '1', '2', '3', '4', '5', '6', '7', '8', '9',
/* 20-63 */ 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
};

/*
 * lcmp --
 *	list comparison routine for bsearch.
 */
static int
lcmp(a, b)
	const void *a, *b;
{
	return (strcmp(a, ((TL *)b)->terminfo));
}

/*
 * tigetstr --
 *
 * PUBLIC: char *tigetstr __P((char *));
 */
char *
tigetstr(name)
	char *name;
{
	static char sbuf[256];
	TL *tlp;
	int n;
	char *p, keyname[3];

	if ((tlp = bsearch(name,
	    list, sizeof(list) / sizeof(TL), sizeof(TL), lcmp)) == NULL) {
		if (name[0] == 'k' &&
		    name[1] == 'f' && (n = atoi(name + 2)) <= 63) {
			keyname[0] = n <= 10 ? 'k' : 'F';
			keyname[1] = codes[n];
			keyname[2] = '\0';
			name = keyname;
		}
	} else
		name = tlp->termcap;

	p = sbuf;
	return (tgetstr(name, &p));
}

/*
 * tigetnum --
 *
 * PUBLIC: int tigetnum __P((char *));
 */
int
tigetnum(name)
	char *name;
{
	return (tgetnum(name));
}
#endif /* BSD_CURSES_INTERFACE */
