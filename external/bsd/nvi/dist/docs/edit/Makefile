# Id: Makefile,v 8.6 2001/01/28 15:50:07 skimo Exp  (Berkeley) Date: 2001/01/28 15:50:07 

ROFF=	groff
TBL=	tbl

all: edittut.ps ../html/edittut.html

../html/%.html: %.ms
	test -d ../html || mkdir ../html
	${TBL} $< | ${ROFF} -ms -Thtml | \
	    sed 's/<img src="$<.*png">//' > $@
	rm -f "$<"*png
	chmod 444 $@

%.ps: %.ms
	${TBL} $< | ${ROFF} -ms > $@
	chmod 444 $@

clean:
	rm -f edittut.ps ../html/edittut.html
