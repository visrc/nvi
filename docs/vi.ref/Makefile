#	$Id: Makefile,v 8.11 1994/06/27 12:35:16 bostic Exp $ (Berkeley) $Date: 1994/06/27 12:35:16 $

DIR=	usd/13.viref
SRCS=	vi.ref ex.cmd.roff set.opt.roff vi.cmd.roff ref.so
MACROS=	-me
CLEANFILES+=vi.ref.txt vi.ref.ps vi.ref.roff index index.so

paper.ps: vi.ref.roff index.so
	soelim vi.ref.roff | ${TBL} | ${ROFF} ${PAGES} > ${.TARGET}

vi.ref.txt: vi.ref.roff index.so
	soelim vi.ref.roff | ${TBL} | ${ROFF} -Tascii ${PAGES} > $@

vi.ref.ps: vi.ref.roff index.so
	soelim vi.ref.roff | ${TBL} | ${ROFF} ${PAGES} > $@

vi.ref.roff: ${SRCS}
	cpp -DREFERENCE -I. ${.CURDIR}/vi.ref | sed -e '/^#/d' -e '/^$$/d' > $@

index.so: vi.ref.roff
	# Fails the first time, but builds index.so.
	soelim vi.ref.roff | ${TBL} | ${ROFF} ${PAGES} > ${.TARGET}
	sed -e 's/MINUSSIGN/\\-/' \
	    -e 's/DOUBLEQUOTE/""/' \
	    -e "s/SQUOTE/'/" \
	    -e 's/ /__SPACE/g' < index | \
	sort -u '-t	' +0 -1 +1n | awk -f merge.awk | \
	sed -e 's/__SPACE/ /g' > index.so
	rm -f index

.include <bsd.doc.mk>
