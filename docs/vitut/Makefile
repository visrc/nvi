#	$Id: Makefile,v 6.5 1993/06/05 17:25:16 bostic Exp $ (Berkeley) $Date: 1993/06/05 17:25:16 $

DIR=	usd/12.vi
SRCS=	vi.in vi.chars
MACROS=	-msU
CLEANFILES+=summary.* viapwh.*

paper.ps: ${SRCS} summary.ps viapwh.ps
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

summary.ps: vi.summary
	${TBL} vi.summary | ${ROFF}  > ${.TARGET}

viapwh.ps: vi.apwh.ms
	${ROFF} vi.viapwh > ${.TARGET}

.include <bsd.doc.mk>
