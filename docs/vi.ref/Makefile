#	$Id: Makefile,v 8.4 1994/05/23 15:22:55 bostic Exp $ (Berkeley) $Date: 1994/05/23 15:22:55 $

DIR=	usd/13.viref
SRCS=	vi.ref set.opt.roff ex.cmd.roff vi.cmd.roff
MACROS=	-man
CLEANFILES+=vi.ref.txt vi.ref.ps vi.ref.roff

paper.ps: vi.ref.roff
	${TBL} vi.ref.roff | ${ROFF} > ${.TARGET}

vi.ref.txt: vi.ref.roff
	${TBL} vi.ref.roff | groff -man -Tascii ${PAGES} > $@

vi.ref.ps: vi.ref.roff
	${TBL} vi.ref.roff | groff -man ${PAGES} > $@

vi.ref.roff: ${SRCS}
	cpp -DREFERENCE -I. ${.CURDIR}/vi.ref | sed -e '/^#/d' -e '/^$$/d' > $@

.include <bsd.doc.mk>
