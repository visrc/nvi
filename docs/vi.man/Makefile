# $Id: Makefile,v 8.8 2001/01/28 15:28:48 skimo Exp $ (Berkeley) $Date: 2001/01/28 15:28:48 $

ROFF=	groff

all: vi.0 vi.0.ps ../html/vi.0.html

vi.0: vi.1
	${ROFF} -man -Tascii < vi.1 > $@
	chmod 444 $@

vi.0.ps: vi.1
	${ROFF} -man < vi.1 > $@
	chmod 444 $@

../html/vi.0.html: vi.1
	${TBL} $< | ${ROFF} -ms -Thtml | \
	    sed 's/<img src="$<.*png">//' > $@
	rm "$<"*png
	chmod 444 $@

clean:
	rm -f vi.0 vi.0.ps ../html/vi.0.html
