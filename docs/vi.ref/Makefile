#	$Id: Makefile,v 8.19 1996/06/27 19:40:47 bostic Exp $ (Berkeley) $Date: 1996/06/27 19:40:47 $

MACROS=	-me
ROFF=	groff
TBL=	tbl

all: vi.ref.txt vi.ref.ps

vi.ref.txt: vi.ref index.so
	soelim vi.ref | ${TBL} | groff ${MACROS} -Tascii > $@

vi.ref.ps: vi.ref index.so
	soelim vi.ref | ${TBL} | ${ROFF} ${MACROS} > $@

index.so: vi.ref
	# Build index.so, side-effect of building the paper.
	soelim vi.ref | ${TBL} | ${ROFF} ${MACROS} > /dev/null
	sed -e 's/MINUSSIGN/\\-/' \
	    -e 's/DOUBLEQUOTE/""/' \
	    -e "s/SQUOTE/'/" \
	    -e 's/ /__SPACE/g' < index | \
	sort -u '-t	' +0 -1 +1n | awk -f merge.awk | \
	sed -e 's/__SPACE/ /g' > index.so
	rm -f index

clean:
	rm -f vi.ref.ps vi.ref.txt index index.so
