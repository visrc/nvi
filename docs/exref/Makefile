# $Id: Makefile,v 8.7 1996/08/18 10:48:04 bostic Exp $ (Berkeley) $Date: 1996/08/18 10:48:04 $

ROFF=	groff
TBL=	tbl

all: exref.ps summary.ps

exref.ps: ex.rm
	${TBL} ex.rm | ${ROFF} -ms > $@
	chmod 444 $@

summary.ps: ex.summary
	${TBL} ex.summary | ${ROFF} > $@
	chmod 444 $@

clean:
	rm -f exref.ps summary.ps
