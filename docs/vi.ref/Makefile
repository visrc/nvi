#	$Id: Makefile,v 8.3 1994/04/29 07:34:44 bostic Exp $ (Berkeley) $Date: 1994/04/29 07:34:44 $

DIR=	usd/13.viref
SRCS=	vi.ref set.opt.roff ex.cmd.roff vi.cmd.roff
MACROS=	-man
CLEANFILES+=vi.ref.txt vi.ref.ps vi.ref.roff

paper.ps: vi.ref.roff
	${TBL} vi.ref.roff | ${ROFF} > ${.TARGET}

vi.ref.txt: vi.ref.roff
	${TBL} vi.ref.roff | groff -man -Tascii > $@

vi.ref.ps: vi.ref.roff
	${TBL} vi.ref.roff | groff -man > $@

vi.ref.roff: ${SRCS}
	cpp -DREFERENCE -I. ${.CURDIR}/vi.ref | sed -e '/^#/d' -e '/^$$/d' > $@

.include <bsd.doc.mk>
