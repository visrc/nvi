# $Id: Makefile,v 8.9 2001/01/28 15:28:47 skimo Exp $ (Berkeley) $Date: 2001/01/28 15:28:47 $

ROFF=	groff
TBL=	tbl

ALL= exref.ps summary.ps ../html/exref.html ../html/ex-summary.html

all: $(ALL)

../html/exref.html: ex.rm
	${TBL} $< | ${ROFF} -ms -Thtml | \
	    sed 's/<img src="$<.*png">//' > $@
	rm "$<"*png
	chmod 444 $@

../html/ex-summary.html: ex.summary
	${TBL} $< | ${ROFF} -ms -Thtml | \
	    sed 's/<img src="$<.*png">//' > $@
	rm "$<"*png
	chmod 444 $@

exref.ps: ex.rm
	${TBL} $< | ${ROFF} -ms > $@
	chmod 444 $@

summary.ps: ex.summary
	${TBL} $< | ${ROFF} -ms > $@
	chmod 444 $@

clean:
	rm -f $(ALL)
