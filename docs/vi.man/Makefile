# $Id: Makefile,v 8.6 1996/06/27 19:35:39 bostic Exp $ (Berkeley) $Date: 1996/06/27 19:35:39 $

ROFF=	groff

all: vi.0 vi.0.ps

vi.0: vi.1
	${ROFF} -man -Tascii < vi.1 > $@
vi.0.ps: vi.1
	${ROFF} -man < vi.1 > $@

clean:
	rm -f vi.0 vi.0.ps
