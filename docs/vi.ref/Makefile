#	$Id: Makefile,v 8.1 1994/04/20 15:45:39 bostic Exp $ (Berkeley) $Date: 1994/04/20 15:45:39 $

SRCS=	vi.ref set.opt.roff
DOCS=	vi.ref.txt vi.ref.ps

all: ${DOCS}

vi.ref.txt: ${SRCS}
	cpp -DREFERENCE -I. vi.ref | \
	    sed -e '/^#/d' -e '/^$$/d' | groff -man -Tascii > $@
vi.ref.ps: ${SRCS}
	cpp -DREFERENCE -I. vi.ref | \
	    sed -e '/^#/d' -e '/^$$/d' | groff -man > $@

clean:
	rm -f ${DOCS}

