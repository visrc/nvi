#	$Id: Makefile,v 8.5 1996/06/27 19:54:31 bostic Exp $ (Berkeley) $Date: 1996/06/27 19:54:31 $

MACROS=	-ms
ROFF=	groff
TBL=	tbl

vitut.ps: vi.in vi.chars summary.ps viapwh.ps
	${TBL} vi.in vi.chars | ${ROFF} ${MACROS} > $@

summary.ps: vi.summary
	${TBL} vi.summary | ${ROFF}  ${MACROS} > $@

viapwh.ps: vi.apwh.ms
	${TBL} vi.apwh.ms | ${ROFF} ${MACROS} > $@

clean:
	rm -f vitut.ps summary.ps viapwh.ps
