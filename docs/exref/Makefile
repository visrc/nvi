#	$Id: Makefile,v 6.5 1993/06/07 20:30:15 bostic Exp $ (Berkeley) $Date: 1993/06/07 20:30:15 $

DIR=	usd/13.ex
SRCS=	ex.rm
MACROS=	-msU
CLEANFILES=summary.*

paper.ps: ${SRCS} summary.ps
	${ROFF} ${SRCS} > ${.TARGET}

summary.ps: ex.summary
	${TBL} ex.summary | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
