#	$Id: Makefile,v 8.10 1994/06/02 11:49:34 bostic Exp $ (Berkeley) $Date: 1994/06/02 11:49:34 $

DIR=	usd/13.viref
SRCS=	vi.ref ex.cmd.roff set.opt.roff vi.cmd.roff ref.so
MACROS=	-me
CLEANFILES+=vi.ref.txt vi.ref.ps vi.ref.roff index.so

paper.ps: vi.ref.roff
	soelim vi.ref.roff | ${TBL} | ${ROFF} ${PAGES} > ${.TARGET}
	sed -e 's/MINUSSIGN/\\-/' \
	    -e 's/DOUBLEQUOTE/""/' \
	    -e "s/SQUOTE/'/" \
	    -e 's/ /__SPACE/g' < index | \
	sort -u '-t	' +0 -1 +1n | awk -f merge.awk | \
	sed -e 's/__SPACE/ /g' > index.so
	rm -f index

vi.ref.txt: vi.ref.roff
	soelim vi.ref.roff | ${TBL} | groff -me -Tascii ${PAGES} > $@

vi.ref.ps: vi.ref.roff
	soelim vi.ref.roff | ${TBL} | groff -me ${PAGES} > $@

vi.ref.roff: ${SRCS}
	cpp -DREFERENCE -I. ${.CURDIR}/vi.ref | sed -e '/^#/d' -e '/^$$/d' > $@

.include <bsd.doc.mk>
