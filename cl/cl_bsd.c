#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int	 first = 1;			/* Initialization flag. */
static char	*ke;				/* Keypad on. */
static char	*ks;				/* Keypad off. */
static char	*vb;				/* Visible bell string. */

static int	bsd_putchar __P((int));
static void	ti_init __P((void));

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
	if (first)
		ti_init();

	if (vb != NULL) {
		(void)tputs(vb, 1, bsd_putchar);
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

	if (first)
		ti_init();

	sbp = sbuf;
	if ((p = tgetstr(on ? "ks" : "ke", &sbp)) != NULL) {
		(void)tputs(p, 0, bsd_putchar);
		(void)fflush(stdout);
	}
	return (0);
}

/*
 * bsd_putchar --
 *	Function version of putchar, for tputs.
 */
static int
bsd_putchar(ch)
	int ch;
{
	return (putchar(ch));
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
setupterm(a, b, errp)
	char *a;
	int b;
	int *errp;
{
	*errp = 1;
	if (first)
		ti_init();
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
	if (first)
		ti_init();

	return (tgetnum(name));
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
	static char sbuf[128];
	char *p;

	if (first)
		ti_init();

	p = sbuf;
	return (tgetstr(name, &p));
}

/*
 * ti_init --
 */
static void
ti_init()
{
	static char buf[2048];
	char *p;

	/*
	 * XXX
	 * We don't want to use the options value, because we're not running
	 * at that level.  This might fail on unusually configured systems.
	 */
	if ((p = getenv("TERM")) == NULL) {
		(void)fprintf(stderr, "Environmental variable TERM not set.\n");
		exit (1);
	}

	if (tgetent(buf, p) <= 0) {
		(void)fprintf(stderr, "%s: tgetent failed.\n", p);
		exit (1);
	}

	/* Reset first, so don't call back into this routine. */
	first = 0;

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
