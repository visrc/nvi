#	$Id: Makefile,v 8.21 2001/01/28 15:28:48 skimo Exp $ (Berkeley) $Date: 2001/01/28 15:28:48 $

MACROS=	-me
ROFF=	groff
TBL=	tbl

all: vi.ref.txt vi.ref.ps ../html/vi.ref.html

vi.ref.txt: vi.ref index.so
	soelim vi.ref | ${TBL} | groff ${MACROS} -Tascii > $@
	rm -f index
	chmod 444 $@

vi.ref.ps: vi.ref index.so
	soelim vi.ref | ${TBL} | ${ROFF} ${MACROS} > $@
	rm -f index
	chmod 444 $@

../html/vi.ref.html: vi.ref index.so
	${TBL} $< | ${ROFF} -ms -Thtml | \
	    sed 's/<img src="$<.*png">//' > $@
	rm "$<"*png
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
	rm -f vi.ref.ps vi.ref.txt index index.so ../html/vi.ref.html
