/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: seq.h,v 5.9 1993/03/25 15:00:27 bostic Exp $ (Berkeley) $Date: 1993/03/25 15:00:27 $
 */

int	 abbr_save __P((SCR *, FILE *));

int	 map_save __P((SCR *, FILE *));

int	 seq_delete __P((SCR *, u_char *, enum seqtype));
int	 seq_dump __P((SCR *, enum seqtype, int));
SEQ	*seq_find __P((SCR *, u_char *, size_t, enum seqtype, int *));
void	 seq_init __P((SCR *));
int	 seq_save __P((SCR *, FILE *, u_char *, enum seqtype));
int	 seq_set __P((SCR *, u_char *, u_char *, u_char *, enum seqtype, int));
