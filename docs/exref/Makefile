#	$Id: Makefile,v 8.3 1994/05/17 16:30:46 mckusick Exp $ (Berkeley) $Date: 1994/05/17 16:30:46 $

DIR=	usd/12.exref
SRCS=	ex.rm ex.summary
MACROS=	-ms
CLEANFILES=summary.*

paper.ps: ${SRCS} summary.ps
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

summary.ps: ex.summary
	${TBL} ex.summary | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
