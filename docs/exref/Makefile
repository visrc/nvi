# $Id: Makefile,v 8.5 1996/06/27 19:33:50 bostic Exp $ (Berkeley) $Date: 1996/06/27 19:33:50 $

ROFF=	groff
TBL=	tbl

all: exref.ps summary.ps

exref.ps: ex.rm
	${TBL} ex.rm | ${ROFF} -ms > $@

summary.ps: ex.summary
	${TBL} ex.summary | ${ROFF} > $@

clean:
	rm -f exref.ps summary.ps
