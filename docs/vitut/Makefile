#	$Id: Makefile,v 6.4 1993/06/05 17:20:57 bostic Exp $ (Berkeley) $Date: 1993/06/05 17:20:57 $

DIR=	usd/12.vi
SRCS=	vi.in vi.chars
MACROS=	-msU
CLEANFILES+=summary.* viapwh.*

paper.ps: ${SRCS} summary.${PRINTER} viapwh.${PRINTER}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

summary.ps: vi.summary
	${TBL} vi.summary | ${ROFF}  > ${.TARGET}

viapwh.ps: vi.apwh.ms
	${ROFF} vi.viapwh > ${.TARGET}

.include <bsd.doc.mk>
