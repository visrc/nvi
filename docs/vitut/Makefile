#	$Id: Makefile,v 8.6 1996/08/18 10:53:19 bostic Exp $ (Berkeley) $Date: 1996/08/18 10:53:19 $

MACROS=	-ms
ROFF=	groff
TBL=	tbl

vitut.ps: vi.in vi.chars summary.ps viapwh.ps
	${TBL} vi.in vi.chars | ${ROFF} ${MACROS} > $@
	chmod 444 $@

summary.ps: vi.summary
	${TBL} vi.summary | ${ROFF}  ${MACROS} > $@
	chmod 444 $@

viapwh.ps: vi.apwh.ms
	${TBL} vi.apwh.ms | ${ROFF} ${MACROS} > $@
	chmod 444 $@

clean:
	rm -f vitut.ps summary.ps viapwh.ps
