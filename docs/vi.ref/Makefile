#	$Id: Makefile,v 8.20 1996/08/18 10:52:42 bostic Exp $ (Berkeley) $Date: 1996/08/18 10:52:42 $

MACROS=	-me
ROFF=	groff
TBL=	tbl

all: vi.ref.txt vi.ref.ps

vi.ref.txt: vi.ref index.so
	soelim vi.ref | ${TBL} | groff ${MACROS} -Tascii > $@
	rm -f index
	chmod 444 $@

vi.ref.ps: vi.ref index.so
	soelim vi.ref | ${TBL} | ${ROFF} ${MACROS} > $@
	rm -f index
	chmod 444 $@

index.so: vi.ref
	# Build index.so, side-effect of building the paper.
	soelim vi.ref | ${TBL} | ${ROFF} ${MACROS} > /dev/null
	sed -e 's/MINUSSIGN/\\-/' \
	    -e 's/DOUBLEQUOTE/""/' \
	    -e "s/SQUOTE/'/" \
	    -e 's/ /__SPACE/g' < index | \
	sort -u '-t	' +0 -1 +1n | awk -f merge.awk | \
	sed -e 's/__SPACE/ /g' > $@
	rm -f index
	chmod 444 $@

clean:
	rm -f vi.ref.ps vi.ref.txt index index.so
