#	$Id: Makefile,v 8.5 1994/07/16 17:56:42 bostic Exp $ (Berkeley) $Date: 1994/07/16 17:56:42 $

SRCS=	vi.1
DOCS=	vi.0 vi.0.ps

all: ${DOCS}

vi.0: vi.1
	groff -man -Tascii < vi.1 > $@
vi.0.ps: vi.1
	groff -man < vi.1 > $@

clean:
	rm -f ${DOCS}
