#	$Id: Makefile,v 8.4 1994/07/15 15:32:11 bostic Exp $ (Berkeley) $Date: 1994/07/15 15:32:11 $

SRCS=	vi.man
DOCS=	vi.0 vi.0.ps

all: ${DOCS}

vi.0: vi.1
	groff -man -Tascii < vi.1 > $@
vi.0.ps: vi.1
	groff -man < vi.1 > $@

clean:
	rm -f ${DOCS}
