/* This file contains movement functions which are screen-relative */

#include <sys/types.h>
#include <curses.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "extern.h"

static MARK rval;

/* This function repositions the current line to show on a given row */
/*ARGSUSED*/
MARK *m_z(m, cnt, key)
	MARK	*m;	/* the cursor */
	long	cnt;	/* the line number we're repositioning */
	int	key;	/* key struck after the z */
{
	long	newtop;
	int	i;

	/* Which line are we talking about? */
	if (cnt < 0 || cnt > nlines)
	{
		return NULL;
	}
	if (cnt)
	{
		m->lno = cnt;
		newtop = cnt;
	}
	else
	{
		newtop = m->lno;
	}

	/* allow a "window size" number to be entered */
	for (i = 0; key >= '0' && key <= '9'; key = getkey(0))
	{
		i = i * 10 + key - '0';
	}
	if (i > 0 && i <= LINES - 1)
	{
		LVAL(O_WINDOW) = i;
	}

	/* figure out which line will have to be at the top of the screen */
	switch (key)
	{
	  case '\n':
	  case '\r':
	  case '+':
		break;

	  case '.':
	  case 'z':
		newtop -= LINES / 2;
		break;

	  case '-':
		newtop -= LINES - 1;
		break;

	  default:
		return NULL;
	}

	/* make the new topline take effect */
	if (newtop >= 1)
	{
		curf->top = newtop;
	}
	else
	{
		curf->top = 1L;
	}

	/* The cursor doesn't move */
	rval = *m;
	return (&rval);
}
