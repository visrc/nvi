# $Id: Makefile,v 8.5 2001/01/28 15:28:47 skimo Exp $ (Berkeley) $Date: 2001/01/28 15:28:47 $

ROFF=	groff
TBL=	tbl

all: edittut.ps ../html/edittut.html

../html/%.html: %.ms
	${TBL} $< | ${ROFF} -ms -Thtml | \
	    sed 's/<img src="$<.*png">//' > $@
	rm "$<"*png
	chmod 444 $@

%.ps: %.ms
	${TBL} $< | ${ROFF} -ms > $@
	chmod 444 $@

clean:
	rm -f edittut.ps ../html/edittut.html
