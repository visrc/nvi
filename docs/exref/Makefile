#	$Id: Makefile,v 8.2 1994/04/22 08:55:08 bostic Exp $ (Berkeley) $Date: 1994/04/22 08:55:08 $

DIR=	usd/12.exref
SRCS=	ex.rm ex.summary
MACROS=	-msU
CLEANFILES=summary.*

paper.ps: ${SRCS} summary.ps
	${ROFF} ${SRCS} > ${.TARGET}

summary.ps: ex.summary
	${TBL} ex.summary | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
