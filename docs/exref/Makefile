#	$Id: Makefile,v 8.4 1994/05/23 14:19:13 mckusick Exp $ (Berkeley) $Date: 1994/05/23 14:19:13 $

DIR=	usd/12.exref
SRCS=	ex.rm
MACROS=	-ms
CLEANFILES=summary.*

paper.ps: ${SRCS} summary.ps
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

summary.ps: ex.summary
	${TBL} ex.summary | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
