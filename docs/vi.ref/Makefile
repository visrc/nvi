#	$Id: Makefile,v 8.6 1994/05/25 23:45:53 bostic Exp $ (Berkeley) $Date: 1994/05/25 23:45:53 $

DIR=	usd/13.viref
SRCS=	vi.ref ex.cmd.roff set.opt.roff vi.cmd.roff ref.so
MACROS=	-me
CLEANFILES+=vi.ref.txt vi.ref.ps vi.ref.roff __1 __2 __3 __4

paper.ps: index.so vi.ref.roff
	soelim vi.ref.roff | ${TBL} | ${ROFF} > ${.TARGET}

vi.ref.txt: index.so vi.ref.roff
	soelim vi.ref.roff | ${TBL} | groff -me -Tascii ${PAGES} > $@

vi.ref.ps: index.so vi.ref.roff
	soelim vi.ref.roff | ${TBL} | groff -me ${PAGES} > $@

vi.ref.roff: ${SRCS}
	cpp -DREFERENCE -I. ${.CURDIR}/vi.ref | sed -e '/^#/d' -e '/^$$/d' > $@

.include <bsd.doc.mk>

index.so: index
	@sed -e 's/MINUSSIGN/\\-/' \
	    -e 's/DOUBLEQUOTE/"/' \
	    -e 's/ /__SPACE/g' < index | \
	sort -u '-t	' +0 -1 +1n | awk -f merge.awk | \
	sed -e 's/__SPACE/ /g' > __1; \
	x=`wc -l __1 | awk '{ print $$1 }'`; \
	x=`expr \( $$x + 2 \) / 3`; \
	sed -e "1,$$x w __2" -e "d" < __1; \
	y=`expr $$x + $$x`; \
	x=`expr $$x + 1`; \
	sed -e 's/^/	/' -e "$$x,$$y w __3" -e "d" < __1; \
	y=`expr $$y + 1`; \
	sed -e 's/^/	/' -e "$$y,$$ w __4" -e "d" < __1; \
	paste __2 __3 __4 > $@; \
	rm -f __1 __2 __3 __4
