#	$Id: Makefile,v 8.2 1994/04/22 08:51:41 bostic Exp $ (Berkeley) $Date: 1994/04/22 08:51:41 $

DIR=	usd/11.vitut
SRCS=	vi.in vi.chars vi.summary vi.apwh.ms
MACROS=	-msU
CLEANFILES+=summary.* viapwh.*

paper.ps: ${SRCS} summary.ps viapwh.ps
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

summary.ps: vi.summary
	${TBL} vi.summary | ${ROFF}  > ${.TARGET}

viapwh.ps: vi.apwh.ms
	${ROFF} vi.apwh.ms > ${.TARGET}

.include <bsd.doc.mk>
