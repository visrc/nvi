#	$Id: Makefile,v 8.23 2001/08/18 20:43:47 skimo Exp $ (Berkeley) $Date: 2001/08/18 20:43:47 $

MACROS=	-me
ROFF=	groff
TBL=	tbl

all: vi.ref.txt vi.ref.ps ../html/vi.ref.html

vi.ref.txt: vi.texi
	makeinfo --no-headers vi.ref > $@ 2>/dev/null
	chmod 444 $@

vi.ref.ps: vi.texi
	texi2dvi vi.texi
	dvips -o $@ vi.dvi
	rm -f vi.dvi
	chmod 444 $@

../html/vi.ref.html: vi.texi
	makeinfo --html vi.ref --output=$@ 2>/dev/null
	chmod 444 $@

clean:
	rm -f vi.ref.ps vi.ref.txt index index.so ../html/vi.ref.html
