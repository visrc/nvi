#	$Id: Makefile,v 8.1 1994/04/20 14:54:24 bostic Exp $ (Berkeley) $Date: 1994/04/20 14:54:24 $

SRCS=	vi.1 ../vi.ref/set.opt.roff
DOCS=	vi.0.txt vi.0.ps

all: ${DOCS}

vi.0.txt: ${SRCS}
	cpp -DMANUAL -I../vi.ref vi.1 | \
	    sed -e '/^#/d' -e '/^$$/d' | groff -man -Tascii > $@
vi.0.ps: ${SRCS}
	cpp -DMANUAL -I../vi.ref vi.1 | \
	    sed -e '/^#/d' -e '/^$$/d' | groff -man > $@

clean:
	rm -f ${DOCS}
