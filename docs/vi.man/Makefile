#	$Id: Makefile,v 8.3 1994/04/29 07:34:21 bostic Exp $ (Berkeley) $Date: 1994/04/29 07:34:21 $

REF=	../vi.ref
SRCS=	vi.man ${REF}/vi.cmd.roff ${REF}/ex.cmd.roff ${REF}/set.opt.roff
DOCS=	vi.0.txt vi.0.ps

all: ${DOCS}

vi.0.txt: vi.1
	groff -man -Tascii < vi.1 > $@
vi.0.ps: vi.1
	groff -man < vi.1 > $@
vi.1: ${SRCS}
	cpp -DMANUAL -I../vi.ref vi.man | sed -e '/^#/d' -e '/^$$/d' > $@

clean:
	rm -f ${DOCS} vi.1
