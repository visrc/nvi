# $Id: Makefile,v 8.4 1996/08/18 10:48:20 bostic Exp $ (Berkeley) $Date: 1996/08/18 10:48:20 $

ROFF=	groff
TBL=	tbl

edittut.ps: edittut.ms
	${TBL} edittut.ms | ${ROFF} -ms > $@
	chmod 444 $@

clean:
	rm -f edittut.ps
