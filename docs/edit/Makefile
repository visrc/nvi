# $Id: Makefile,v 8.2 1996/06/27 18:48:28 bostic Exp $ (Berkeley) $Date: 1996/06/27 18:48:28 $

ROFF=	groff
TBL=	tbl

edittut.ps: edittut.ms
	${TBL} edittut.ms | ${ROFF} -ms > $@

clean:
	rm -f edittut.ps
