#	$Id: Makefile,v 8.3 1994/05/17 16:28:44 mckusick Exp $ (Berkeley) $Date: 1994/05/17 16:28:44 $

DIR=	usd/11.vitut
SRCS=	vi.in vi.chars vi.summary vi.apwh.ms
MACROS=	-ms
CLEANFILES+=summary.* viapwh.*

paper.ps: ${SRCS} summary.ps viapwh.ps
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

summary.ps: vi.summary
	${TBL} vi.summary | ${ROFF}  > ${.TARGET}

viapwh.ps: vi.apwh.ms
	${TBL} vi.apwh.ms | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
