/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: log.h,v 5.2 1992/11/11 18:32:36 bostic Exp $ (Berkeley) $Date: 1992/11/11 18:32:36 $
 */

/*
 * The log consists of records, where a record contains a type byte and
 * then a variable length byte string, as follows:
 *
 *	LOG_CURSOR		MARK
 *	LOG_LINE_APPEND 	recno_t		line
 *	LOG_LINE_DELETE		recno_t		line
 *	LOG_LINE_INSERT		recno_t		line
 *	LOG_LINE_RESET		recno_t		line
 *	LOG_MARK		key		MARK
 *	LOG_START
 *
 * We do before image physical logging.  This means that the editor layer
 * cannot modify records in place, even if it's deleting or overwriting
 * characters.  Since the smallest unit of logging is a line, we're using
 * up lots of space.  This may eventually have to be reduced, probably by
 * doing logical logging, which is a much cooler database phrase.
 */

#define	LOG_NOTYPE	0
#define	LOG_CURSOR	1
#define	LOG_LINE_APPEND	2
#define	LOG_LINE_DELETE	3
#define	LOG_LINE_INSERT	4
#define	LOG_LINE_RESET	5
#define	LOG_MARK	6
#define	LOG_START	7

int	log_backward __P((EXF *, MARK *, recno_t));
int	log_cursor __P((EXF *));
int	log_end __P((EXF *));
int	log_forward __P((EXF *, MARK *));
int	log_init __P((EXF *));
int	log_line __P((EXF *, recno_t, u_int));
int	log_mark __P((EXF *, u_int, MARK *));
