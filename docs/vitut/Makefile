#	$Id: Makefile,v 6.3 1990/06/30 15:09:28 bostic Exp $ (Berkeley) $Date: 1990/06/30 15:09:28 $

DIR=	usd/15.vi
SRCS=	vi.in vi.chars
MACROS=	-msU
CLEANFILES+=summary.* viapwh.*

paper.${PRINTER}: ${SRCS} summary.${PRINTER} viapwh.${PRINTER}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

summary.${PRINTER}: vi.summary
	${TBL} vi.summary | ${ROFF}  > ${.TARGET}

viapwh.${PRINTER}: vi.apwh.ms
	${ROFF} vi.viapwh > ${.TARGET}

.include <bsd.doc.mk>
