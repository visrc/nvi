/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vi.c,v 5.1 1992/04/19 10:54:29 bostic Exp $ (Berkeley) $Date: 1992/04/19 10:54:29 $";
#endif /* not lint */

#include <sys/types.h>
#include <termios.h>
#include <stdio.h>
#include <ctype.h>

#include "vi.h"
#include "curses.h"
#include "options.h"
#include "tty.h"
#include "vcmd.h"
#include "extern.h"

static int keymodes[] = {0, WHEN_REP1, WHEN_CUT, WHEN_MARK, WHEN_CHAR};

void
vi()
{
	REG int			key;	/* keystroke from user */
	long			count;	/* numeric argument to some functions */
	REG VIKEYS	*keyptr;/* pointer to vikeys[] element */
	MARK			tcurs;	/* temporary cursor */
	int			prevkey;/* previous key, if d/c/y/</>/! */
	MARK			range;	/* start of range for d/c/y/</>/! */
	char			text[132];
	int			dotkey;	/* last "key" of a change */
	int			dotpkey;/* last "prevkey" of a change */
	int			dotkey2;/* last extra "getkey()" of a change */
	int			dotcnt;	/* last "count" of a change */
	REG int			i;
	char *p;

	/* Tell the redraw() function to start from scratch. */
	iredraw();

#ifdef lint
	/* lint says that "range" might be used before it is set.  This
	 * can't really happen due to the way "range" and "prevkey" are used,
	 * but lint doesn't know that.  This line is here ONLY to keep lint
	 * happy.
	 */
	range = 0L;
#endif

	/* safeguard against '.' with no previous command */
	dotkey = 0;

	/* Repeatedly handle VI commands */
	for (count = 0, prevkey = '\0'; mode == MODE_VI; )
	{
		/* if we've moved off the undoable line, then we can't undo it at all */
		if (markline(cursor) != U_line)
		{
			U_line = 0L;
		}

		/* report any changes from the previous command */
		if (rptlines >= LVAL(O_REPORT))
		{
			redraw(cursor, FALSE);
			msg("%ld line%s %s", rptlines, (rptlines==1?"":"s"), rptlabel);
		}
		rptlines = 0L;

		do
		{
			key = getkey(WHEN_VICMD);
		} while (key < 0 || key > 127);

		/* Convert a doubled-up operator such as "dd" into "d_" */
		if (prevkey && key == prevkey)
		{
			key = '_';
		}

		/* look up the structure describing this command */
		keyptr = &vikeys[key];

		/* '&' and uppercase operators always act like doubled */
		if (!prevkey && keyptr->args == CURSOR_MOVED
			&& (key == '&' || isupper(key)))
		{
			range = cursor;
			prevkey = key;
			key = '_';
			keyptr = &vikeys[key];
		}

		/* if we're in the middle of a v/V command, reject commands
		 * that aren't operators or movement commands
		 */
		if (V_from && !(keyptr->flags & VIZ))
		{
			bell();
			prevkey = 0;
			count = 0;
			continue;
		}

		/* if we're in the middle of a d/c/y/</>/! command, reject
		 * anything but movement.
		 */
		if (prevkey && !(keyptr->flags & (MVMT|PTMV)))
		{
			bell();
			prevkey = 0;
			count = 0;
			continue;
		}

		/* set the "dot" variables, if we're supposed to */
		if (((keyptr->flags & SDOT)
			|| (prevkey && vikeys[prevkey].flags & SDOT))
		    && !V_from
		)
		{
			dotkey = key;
			dotpkey = prevkey;
			dotkey2 = '\0';
			dotcnt = count;

			/* remember the line before any changes are made */
			if (U_line != markline(cursor))
			{
				U_line = markline(cursor);
				strcpy(U_text, fetchline(U_line, NULL));
			}
		}

		/* if this is "." then set other vars from the "dot" vars */
		if (key == '.')
		{
			key = dotkey;
			keyptr = &vikeys[key];
			prevkey = dotpkey;
			if (prevkey)
			{
				range = cursor;
			}
			if (count == 0)
			{
				count = dotcnt;
			}
			doingdot = TRUE;

			/* remember the line before any changes are made */
			if (U_line != markline(cursor))
			{
				U_line = markline(cursor);
				strcpy(U_text, fetchline(U_line, NULL));
			}
		}
		else
		{
			doingdot = FALSE;
		}

		/* process the key as a command */
		tcurs = cursor;
		switch (keyptr->args & ARGSMASK)
		{
		  case ZERO:
			if (count == 0)
			{
				tcurs = cursor & ~(BLKSIZE - 1);
				break;
			}
			/* else fall through & treat like other digits... */

		  case DIGIT:
			count = count * 10 + key - '0';
			break;

		  case KEYWORD:
			/* if not on a keyword, fail */
			pfetch(markline(cursor));
			key = markidx(cursor);
			if (!isalnum(ptext[key]))
			{
				tcurs = MARK_UNSET;
				break;
			}

			/* find the start of the keyword */
			while (key > 0 && isalnum(ptext[key - 1]))
			{
				key--;
			}
			tcurs = (cursor & ~(BLKSIZE - 1)) + key;

			/* copy it into a buffer, and NUL-terminate it */
			i = 0;
			do
			{
				text[i++] = ptext[key++];
			} while (isalnum(ptext[key]));
			text[i] = '\0';

			/* call the function */
			tcurs = (*keyptr->func)(text, tcurs, count);
			count = 0L;
			break;

		  case 0:
			if (keyptr->func)
			{
				(*keyptr->func)();
			}
			else
			{
				bell();
			}
			count = 0L;
			break;
	
		  case CURSOR:

			tcurs = (*keyptr->func)(cursor, count, key, prevkey);
			count = 0L;
			break;

		  case CURSOR_CNT_KEY:
			if (doingdot)
			{
				tcurs = (*keyptr->func)(cursor, count, dotkey2);
			}
			else
			{
				/* get a key */
				i = getkey(KEYMODE(keyptr->args));
				if (i == '\033') /* ESC */
				{
					count = 0;
					tcurs = MARK_UNSET;
					break; /* exit from "case CURSOR_CNT_KEY" */
				}
				else if (i == ('V' & 0x1f))
				{
					i = getkey(0);
				}

				/* if part of an SDOT command, remember it */
				 if (keyptr->flags & SDOT
				 || (prevkey && vikeys[prevkey].flags & SDOT))
				{
					dotkey2 = i;
				}

				/* do it */
				tcurs = (*keyptr->func)(cursor, count, i);
			}
			count = 0L;
			break;
	
		  case CURSOR_MOVED:
			if (V_from)
			{
				range = cursor;
				tcurs = V_from;
				count = 0L;
				prevkey = key;
				key = (V_linemd ? 'V' : 'v');
				keyptr = &vikeys[key];
			}
			else
			{
				prevkey = key;
				range = cursor;
#ifndef CRUNCH
				force_lnmd = FALSE;
#endif
			}
			break;

		  case CURSOR_EOL:
			prevkey = key;
			/* a zero-length line needs special treatment */
			pfetch(markline(cursor));
			if (plen == 0)
			{
				/* act on a zero-length section of text */
				range = tcurs = cursor;
				key = ' ';
			}
			else
			{
				/* act like CURSOR_MOVED with '$' movement */
				range = cursor;
				tcurs = m_rear(cursor, 1L);
				key = '$';
			}
			count = 0L;
			keyptr = &vikeys[key];
			break;

		  case CURSOR_TEXT:
		  	do
		  	{	
				if ((p =
				    gb(key, GB_BS|GB_ESC|GB_OFF)) != NULL) {
					/* reassure user that <CR> was hit */
					qaddch('\r');
					refresh();

					/* call the function with the text */
					p[0] = key;
					tcurs = (*keyptr->func)(cursor, p);
				}
				else
				{
					if (exwrote || mode == MODE_COLON)
						iredraw();
					mode = MODE_VI;
				}
			} while (mode == MODE_COLON);
			count = 0L;
			break;
		}

		/* if that command took us out of vi mode, then exit the loop
		 * NOW, without tweaking the cursor or anything.  This is very
		 * important when mode == MODE_QUIT.
		 */
		if (mode != MODE_VI)
		{
			break;
		}

		/* now move the cursor, as appropriate */
		if (keyptr->args == CURSOR_MOVED)
		{
			/* the < and > keys have FRNT,
			 * but it shouldn't be applied yet
			 */
			tcurs = adjmove(cursor, tcurs, 0);
		}
		else
		{
			tcurs = adjmove(cursor, tcurs, (int)keyptr->flags);
		}

		/* was that the end of a d/c/y/</>/! command? */
		if (prevkey && ((keyptr->flags & MVMT)
					       || V_from
				) && count == 0L)
		{
			/* turn off the hilight */
			V_from = 0L;

			/* if the movement command failed, cancel operation */
			if (tcurs == MARK_UNSET)
			{
				prevkey = 0;
				count = 0;
				continue;
			}

			/* make sure range=front and tcurs=rear.  Either way,
			 * leave cursor=range since that's where we started.
			 */
			cursor = range;
			if (tcurs < range)
			{
				range = tcurs;
				tcurs = cursor;
			}

			/* The 'w' and 'W' destinations should never take us
			 * to the front of a line.  Instead, they should take
			 * us only to the end of the preceding line.
			 */
			if ((keyptr->flags & (MVMT|NREL|LNMD|FRNT|INCL)) == MVMT
			  && markline(range) < markline(tcurs)
			  && (markline(tcurs) > nlines || tcurs == m_front(tcurs, 0L)))
			{
				tcurs = (tcurs & ~(BLKSIZE - 1)) - BLKSIZE;
				pfetch(markline(tcurs));
				tcurs += plen;
			}

			/* adjust for line mode & inclusion of last char/line */
			i = (keyptr->flags | vikeys[prevkey].flags);
#ifndef CRUNCH
			if (force_lnmd)
			{
				i |= (INCL|LNMD);
			}
#endif
			switch (i & (INCL|LNMD))
			{
			  case INCL:
				tcurs++;
				break;

			  case INCL|LNMD:
				tcurs += BLKSIZE;
				/* fall through... */

			  case LNMD:
				range &= ~(BLKSIZE - 1);
				tcurs &= ~(BLKSIZE - 1);
				break;
			}

			/* run the function */
			tcurs = (*vikeys[prevkey].func)(range, tcurs);
			if (mode == MODE_VI)
			{
				(void)adjmove(cursor, cursor, 0);
				cursor = adjmove(cursor, tcurs, (int)vikeys[prevkey].flags);
			}

			/* cleanup */
			prevkey = 0;
		}
		else if (!prevkey)
		{
			cursor = tcurs;
		}
	}
}

/* This function adjusts the MARK value that they return; here we make sure
 * it isn't past the end of the line, and that the column hasn't been
 * *accidentally* changed.
 */
MARK adjmove(old, new, flags)
	MARK		old;	/* the cursor position before the command */
	REG MARK	new;	/* the cursor position after the command */
	int		flags;	/* various flags regarding cursor mvmt */
{
	static int	colno;	/* the column number that we want */
	REG char	*text;	/* used to scan through the line's text */
	REG int		i;

#ifdef DEBUG
	watch();
#endif

	/* if the command failed, bag it! */
	if (new == MARK_UNSET)
	{
		bell();
		return old;
	}

	/* if this is a non-relative movement, set the '' mark */
	if (flags & NREL)
	{
		mark[26] = old;
	}

	/* make sure it isn't past the end of the file */
	if (markline(new) < 1)
	{
		new = MARK_FIRST;
	}
	else if (markline(new) > nlines)
	{
		new = MARK_LAST;
	}

	/* fetch the new line */
	pfetch(markline(new));

	/* move to the front, if we're supposed to */
	if (flags & FRNT)
	{
		new = m_front(new, 1L);
	}

	/* change the column#, or change the mark to suit the column# */
	if (!(flags & NCOL))
	{
		/* change the column# */
		i = markidx(new);
		if (i == BLKSIZE - 1)
		{
			new &= ~(BLKSIZE - 1);
			if (plen > 0)
			{
				new += plen - 1;
			}
			colno = BLKSIZE * 8; /* one heck of a big colno */
		}
		else if (plen > 0)
		{
			if (i >= plen)
			{
				new = (new & ~(BLKSIZE - 1)) + plen - 1;
			}
			colno = idx2col(new, ptext, FALSE);
		}
		else
		{
			new &= ~(BLKSIZE - 1);
			colno = 0;
		}
	}
	else
	{
		/* adjust the mark to get as close as possible to column# */
		for (i = 0, text = ptext; i <= colno && *text; text++)
		{
			if (*text == '\t' && !ISSET(O_LIST))
			{
				i += LVAL(O_TABSTOP) - (i % LVAL(O_TABSTOP));
			}
			else if (UCHAR(*text) < ' ' || *text == 127)
			{
				i += 2;
			}
			else
			{
				i++;
			}
		}
		if (text > ptext)
		{
			text--;
		}
		new = (new & ~(BLKSIZE - 1)) + (int)(text - ptext);
	}

	return new;
}
