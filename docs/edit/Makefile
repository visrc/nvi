#	$Id: Makefile,v 6.2 1990/06/30 15:09:20 bostic Exp $ (Berkeley) $Date: 1990/06/30 15:09:20 $

DIR=	usd/14.edit
SRCS=	edittut.ms
MACROS=	-msU

paper.${PRINTER}: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

# index for versatec is different from the one in edit.tut
# because the fonts are different and entries reference page
# rather than section numbers.  if you have a typesetter
# you should just use the index in edit.tut, and ignore editvindex.

editvindex:
	${TROFF} ${MACROS} -n22 edit.vindex

.include <bsd.doc.mk>
