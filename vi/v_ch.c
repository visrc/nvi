/* move3.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains movement functions that perform character searches */
#include <sys/types.h>
#include <stddef.h>

#include "vi.h"
#include "exf.h"
#include "vcmd.h"
#include "extern.h"

static MARK	*(*prevfwdfn)();/* function to search in same direction */
static MARK	*(*prevrevfn)();/* function to search in opposite direction */
static char	prev_key;	/* sought cvhar from previous [fFtT] */
static MARK rval;

MARK	*m__ch(m, cnt, cmd)
	MARK	*m;	/* current position */
	long	cnt;
	int	cmd;	/* command: either ',' or ';' */
{
	MARK	*(*tmp)();

	if (!prevfwdfn)
	{
		msg("No previous f, F, t, or T command");
		return NULL;
	}

	if (cmd == ',')
	{
		m = (*prevrevfn)(m, cnt, prev_key);

		/* Oops! we didn't want to change the prev*fn vars! */
		tmp = prevfwdfn;
		prevfwdfn = prevrevfn;
		prevrevfn = tmp;

		rval = *m;
		return (&rval);
	}
	else
	{
		return (*prevfwdfn)(m, cnt, prev_key);
	}
}

/* move forward within this line to next occurrence of key */
MARK	*m_fch(m, cnt, key)
	MARK	*m;	/* where to search from */
	long	cnt;
	int	key;	/* what to search for */
{
	REG char	*text;

	SETDEFCNT(1);

	prevfwdfn = m_fch;
	prevrevfn = m_Fch;
	prev_key = key;

	text = file_line(curf, m->lno, NULL);
	while (cnt-- > 0)
	{
		do
		{
			++m->cno;
			text++;
		} while (*text && *text != key);
	}
	if (!*text)
	{
		return NULL;
	}
	rval = *m;
	return (&rval);
}

/* move backward within this line to previous occurrence of key */
MARK	*m_Fch(m, cnt, key)
	MARK	*m;	/* where to search from */
	long	cnt;
	int	key;	/* what to search for */
{
	REG char	*stext, *text;

	SETDEFCNT(1);

	prevfwdfn = m_Fch;
	prevrevfn = m_fch;
	prev_key = key;

	stext = text = file_line(curf, m->lno, NULL);
	while (cnt-- > 0)
	{
		do
		{
			m->cno--;
			text--;
		} while (text >= stext && *text != key);
	}
	if (text < stext)
	{
		return NULL;
	}
	rval = *m;
	return (&rval);
}

/* move forward within this line almost to next occurrence of key */
MARK	*m_tch(m, cnt, key)
	MARK	*m;	/* where to search from */
	long	cnt;
	int	key;	/* what to search for */
{
	size_t len;

	/* skip the adjacent char */
	(void)file_line(curf, m->lno, &len);
	if (len <= m->cno)
	{
		return NULL;
	}
	m->cno++;

	m = m_fch(m, cnt, key);
	if (m == NULL)
	{
		return NULL;
	}

	prevfwdfn = m_tch;
	prevrevfn = m_Tch;

	--m->cno;
	rval = *m;
	return (&rval);
}

/* move backward within this line almost to previous occurrence of key */
MARK	*m_Tch(m, cnt, key)
	MARK	*m;	/* where to search from */
	long	cnt;
	int	key;	/* what to search for */
{
	/* skip the adjacent char */
	if (m->cno == 1)
	{
		return NULL;
	}
	m->cno--;

	m = m_Fch(m, cnt, key);
	if (m == NULL)
	{
		return NULL;
	}

	prevfwdfn = m_Tch;
	prevrevfn = m_tch;

	++m->cno;
	rval = *m;
	return (&rval);
}
