# $Id: Makefile,v 8.3 1996/08/18 10:46:25 bostic Exp $ (Berkeley) $Date: 1996/08/18 10:46:25 $

ROFF=	groff
TBL=	tbl

edittut.ps: edittut.ms
	${TBL} edittut.ms | ${ROFF} -ms > $@
	chmod 444 edittut.ps

clean:
	rm -f edittut.ps
