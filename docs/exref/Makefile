# $Id: Makefile,v 8.8 1996/10/10 19:39:00 bostic Exp $ (Berkeley) $Date: 1996/10/10 19:39:00 $

ROFF=	groff
TBL=	tbl

all: exref.ps summary.ps

exref.ps: ex.rm
	${TBL} ex.rm | ${ROFF} -ms > $@
	chmod 444 $@

summary.ps: ex.summary
	${TBL} ex.summary | ${ROFF} -ms > $@
	chmod 444 $@

clean:
	rm -f exref.ps summary.ps
