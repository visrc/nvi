#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "extern.h"

static enum { NOTSET, CBACK, CFORW } csearchdir = NOTSET;
static int lastkey;

/*
 * v_repeatch -- [count];
 *	Repeate the last F, f, T or t search.
 */
int
v_repeatch(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	vp->character = lastkey;

	switch(csearchdir) {
	case NOTSET:
		bell();
		if (ISSET(O_VERBOSE))
			msg("No previous F, f, T or t search.");
		return (1);
	case CBACK:
		return (v_rch(vp, cp, rp));
	case CFORW:
		return (v_fch(vp, cp, rp));
	}
	/* NOTREACHED */
}

/*
 * v_rrepeatch -- [count],
 *	Repeat the last F, f, T or t search in the reverse direction.
 */
int
v_rrepeatch(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	int rval;

	vp->character = lastkey;

	switch(csearchdir) {
	case NOTSET:
		bell();
		if (ISSET(O_VERBOSE))
			msg("No previous F, f, T or t search.");
		return (1);
	case CBACK:
		rval = v_fch(vp, cp, rp);
		csearchdir = CBACK;
		break;
	case CFORW:
		rval = v_rch(vp, cp, rp);
		csearchdir = CFORW;
		break;
	}
	return (rval);
}
	
/*
 * v_tfch -- [count]tc
 *	Search this line for a character after the cursor and position to
 *	its left.
 */
int
v_tfch(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	if (v_fch(vp, cp, rp))
		return (1);
	--rp->cno;
	return (0);
}

/*
 * v_fch -- [count]fc
 *	Search this line for a character after the cursor.
 */
int
v_fch(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	size_t len;
	u_long cnt;
	int key;
	char *ep, *p, *sp;

	EGETLINE(p, cp->lno, len);

	lastkey = key = vp->character;
	csearchdir = CFORW;

	p += cp->cno - 1;
	sp = p;
	ep = p + len;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;) {
		while (++p < ep)
			if (*p == key)
				break;
		if (p == ep) {
			bell();
			if (ISSET(O_VERBOSE))
				msg("Character not found.");
			return (1);
		}
	}
	rp->lno = cp->lno;
	rp->cno = p - sp;
	return (0);
}
	
/*
 * v_trch -- [count]Tc
 *	Search this line for a character before the cursor and position to
 *	its left.
 */
int
v_trch(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	if (v_rch(vp, cp, rp))
		return (1);
	if (rp->cno > 0)
		--rp->cno;
	return (0);
}

/*
 * v_rch -- [count]fc
 *	Search this line for a character before the cursor.
 */
int
v_rch(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	size_t len;
	u_long cnt;
	int key;
	char *ep, *p, *sp;

	GETLINE(p, cp->lno, len);

	lastkey = key = vp->character;
	csearchdir = CBACK;

	sp = p;
	p += cp->cno;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;) {
		while (--p < ep)
			if (*p == key)
				break;
		if (p == sp) {
			bell();
			if (ISSET(O_VERBOSE))
				msg("Character not found.");
			return (1);
		}
	}
	rp->lno = cp->lno;
	rp->cno = p - sp;
	return (0);
}
