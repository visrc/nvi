/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: vi.h,v 5.2 1992/04/18 19:40:45 bostic Exp $ (Berkeley) $Date: 1992/04/18 19:40:45 $
 */

MARK	v_again __P((MARK, MARK));
MARK	v_at __P((MARK, long, int));
MARK	v_change __P((MARK, MARK));
MARK	v_delete __P((MARK, MARK));
MARK	v_errlist __P((MARK));
MARK	v_ex __P((MARK, char *));
MARK	v_filter __P((MARK, MARK));
MARK	v_increment __P((char *, MARK, long));
MARK	v_insert __P((MARK, long, int));
MARK	v_join __P((MARK, long));
MARK	v_keyword __P((char *, MARK, long));
MARK	v_mark __P((MARK, long, int));
MARK	v_overtype __P((MARK));
MARK	v_paste __P((MARK, long, int));
MARK	v_quit __P((void));
MARK	v_redraw __P((void));
MARK	v_replace __P((MARK, long, int));
MARK	v_selcut __P((MARK, long, int));
MARK	v_shiftl __P((MARK, MARK));
MARK	v_shiftr __P((MARK, MARK));
MARK	v_start __P((MARK, long, int));
MARK	v_status __P((void));
MARK	v_subst __P((MARK, long));
MARK	v_switch __P((void));
MARK	v_tag __P((char *, MARK, long));
MARK	v_ulcase __P((MARK, long));
MARK	v_undo __P((MARK));
MARK	v_undoine __P((MARK));
MARK	v_xchar __P((MARK, long, int));
MARK	v_xit __P((MARK, long, int));
MARK	v_yank __P((MARK, MARK));
