#	$Id: Makefile,v 8.1 1993/06/08 12:51:59 bostic Exp $ (Berkeley) $Date: 1993/06/08 12:51:59 $

DIR=	usd/13.ex
SRCS=	ex.rm
MACROS=	-msU
CLEANFILES=summary.*

paper.ps: ${SRCS} summary.ps
	${ROFF} ${SRCS} > ${.TARGET}

summary.ps: ex.summary
	${TBL} ex.summary | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
