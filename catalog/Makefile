#	$Id: Makefile,v 8.7 1994/10/09 19:07:10 bostic Exp $ (Berkeley) $Date: 1994/10/09 19:07:10 $

CAT=	vi_english vi_german
FILES=	../common/*.c ../ex/*.c ../sex/*.c ../vi/*.c ../svi/*.c ../xaw/*.c

all: dump ${CAT}

${CAT}: vi_english.base
	@for i in *.base; do						\
		echo "... $$i";						\
		awk '{							\
			if (++nline > $$1) {				\
				print "DUPLICATE MESSAGE NUMBERS";	\
				exit;					\
			}						\
			for (; nline < $$1; ++nline)			\
				print "";				\
			print $0;					\
		}' < $$i |						\
		sed -e '/"/s/^[^"]*"//'					\
		    -e '/"/s/"$$//' > `basename $$i .base`;		\
	done

check: ${CAT}
	@for i in *.base; do						\
		echo "... $$i";						\
		f=`basename $$i .base`;					\
		(echo "UNUSED MESSAGE NUMBERS (this is okay):";		\
		awk '{while (++nline < $$1) print nline}' < $$i;	\
		echo =========================;				\
		echo "MISSING ERROR MESSAGES (this is NOT okay):";	\
		awk '{print $$1}' < $$i > __ck1;			\
		awk '{print $$1}' < vi_english.base > __ck2;		\
		join -v 2 __ck1 __ck2;					\
		rm -f __ck1 __ck2;					\
		echo =========================;				\
		echo "DUPLICATE MESSAGE NUMBERS (this is NOT okay):";	\
		for j in						\
		    `sed '/^$$/d' < $$i | sort -u |			\
		    awk '{print $$1}' | uniq -d`; do			\
			egrep $$j $$i;					\
		done;							\
		echo =========================;				\
		echo "DUPLICATE MESSAGES (this is okay):";		\
		sed '/^$$/d' < $$f | sort | uniq -c |			\
		awk '$$1 != 1 { print $$0 }' | sort -n;			\
		echo =========================) > $$f.check;		\
	done

vi_english.base: dump ${FILES}
	./dump ${FILES} |\
	sed -e '/|/!d' \
	    -e 's/|/ "/' \
	    -e 's/^"//' |\
	sort -n > $@

dump: dump.c
	${CC} -O -o dump dump.c

clean:
	rm -f dump dump.o ${CAT} vi_english.base *.check __ck1 __ck2
