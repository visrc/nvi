/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	$Id: log.h,v 5.1 1992/11/11 09:29:09 bostic Exp $ (Berkeley) $Date: 1992/11/11 09:29:09 $
 */

/*
 * The log consists of sets of records, as follows:
 *
 *	LOG_CURSOR		MARK
 *	LOG_LINE_APPEND 	recno_t
 *	LOG_LINE_DELETE		recno_t		line
 *	LOG_LINE_INSERT		recno_t
 *	LOG_LINE_RESET		recno_t		line
 *	LOG_MARK		key		MARK
 *	LOG_START
 *
 * We do before image physical logging.  This means that the editor layer
 * cannot modify records in place, even if it's deleting or overwriting
 * characters.  Since the smallest unit of logging is a line, we're using
 * up lots of space.  This may eventually have to be reduced.
 *
 * The log consists of sets of records, where a set is one or more data
 * records followed by a flag record.  The order in the database is the
 * reverse of the list above, making it simpler to read the log from the
 * end toward the beginning.
 */

#define	LOG_NOTYPE	0
#define	LOG_CURSOR	1
#define	LOG_LINE_APPEND	2
#define	LOG_LINE_DELETE	3
#define	LOG_LINE_INSERT	4
#define	LOG_LINE_RESET	5
#define	LOG_MARK	6
#define	LOG_START	7

int	log_cursor __P((EXF *));
int	log_init __P((EXF *));
int	log_line __P((EXF *, recno_t, u_int));
int	log_mark __P((EXF *, u_int, MARK *));
int	log_undo __P((EXF *, MARK *));
