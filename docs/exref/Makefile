#	$Id: Makefile,v 6.3 1993/06/05 17:21:41 bostic Exp $ (Berkeley) $Date: 1993/06/05 17:21:41 $

DIR=	usd/13.ex
SRCS=	ex.rm
MACROS=	-msU
CLEANFILES=summary.*

paper.ps ${SRCS} summary.${PRINTER}
	${ROFF} ${SRCS} > ${.TARGET}

summary.ps: ex.summary
	${TBL} ex.summary | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
