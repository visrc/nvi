#	$Id: Makefile,v 8.1 1994/08/31 13:27:45 bostic Exp $ (Berkeley) $Date: 1994/08/31 13:27:45 $

FILES=	../common/*.c ../ex/*.c ../sex/*.c ../vi/*.c ../svi/*.c ../xaw/*.c
english: dump ${FILES}
	./dump ${FILES} |\
	sed -e '/|/!d' \
	    -e 's/|/ "/' \
	    -e 's/^"//' |\
	sort -n > vi_$@.base

catalog:
	@for i in *.base; do						\
		awk '{							\
			if (++nline > $$1) {				\
				print "DUPLICATE MESSAGE NUMBERS";	\
				exit;					\
			}						\
			for (; nline < $$1; ++nline)			\
				print "";				\
			print $0;					\
		}' < $$i | tee /tmp/_f |				\
		sed -e '/"/s/^[^"]*"//'					\
		    -e '/"/s/"$$//' > `basename $$i .base`;		\
	done

check:
	@for i in *.base; do						\
		f=`basename $$i .base`;					\
		(echo "UNUSED MESSAGE NUMBERS (this is okay):";		\
		awk '{while (++nline < $$1) print nline}' < $$i;	\
		echo =========================;				\
		echo "DUPLICATE MESSAGE NUMBERS (this is NOT okay):";	\
		for j in						\
		    `sed '/^$$/d' < vi_english.base | sort -u |		\
		    awk '{print $$1}' | uniq -d`; do			\
			egrep $$j $$i;					\
		done;							\
		echo =========================;				\
		echo "DUPLICATE MESSAGES (this is okay):";		\
		sed '/^$$/d' < $$f | sort | uniq -c |			\
		awk '$$1 != 1 { print $$0 }' | sort -n;			\
		echo =========================) > $$f.check;		\
	done

dump: dump.c
	${CC} -O -o dump dump.c

clean:
	rm -f dump dump.o vi_english vi_english.base *.check
