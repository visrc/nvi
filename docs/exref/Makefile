#	$Id: Makefile,v 6.2 1990/06/30 15:09:32 bostic Exp $ (Berkeley) $Date: 1990/06/30 15:09:32 $

DIR=	usd/16.ex
SRCS=	ex.rm
MACROS=	-msU
CLEANFILES=summary.*

paper.${PRINTER}: ${SRCS} summary.${PRINTER}
	${ROFF} ${SRCS} > ${.TARGET}

summary.${PRINTER}: ex.summary
	${TBL} ex.summary | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
