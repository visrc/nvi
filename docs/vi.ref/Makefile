#	$Id: Makefile,v 8.5 1994/05/25 13:13:45 bostic Exp $ (Berkeley) $Date: 1994/05/25 13:13:45 $

DIR=	usd/13.viref
SRCS=	vi.ref ex.cmd.roff set.opt.roff vi.cmd.roff ref.so
MACROS=	-me
CLEANFILES+=vi.ref.txt vi.ref.ps vi.ref.roff

paper.ps: vi.ref.roff
	${TBL} vi.ref.roff | ${ROFF} > ${.TARGET}

vi.ref.txt: vi.ref.roff
	${TBL} vi.ref.roff | groff -me -Tascii ${PAGES} > $@

vi.ref.ps: vi.ref.roff
	${TBL} vi.ref.roff | groff -me ${PAGES} > $@

vi.ref.roff: ${SRCS}
	cpp -DREFERENCE -I. ${.CURDIR}/vi.ref | sed -e '/^#/d' -e '/^$$/d' > $@

.include <bsd.doc.mk>
