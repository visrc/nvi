# $Id: Makefile,v 8.6 1996/08/18 10:46:59 bostic Exp $ (Berkeley) $Date: 1996/08/18 10:46:59 $

ROFF=	groff
TBL=	tbl

all: exref.ps summary.ps

exref.ps: ex.rm
	${TBL} ex.rm | ${ROFF} -ms > $@
	chmod 444 exref.ps

summary.ps: ex.summary
	${TBL} ex.summary | ${ROFF} > $@
	chmod 444 summary.ps

clean:
	rm -f exref.ps summary.ps
