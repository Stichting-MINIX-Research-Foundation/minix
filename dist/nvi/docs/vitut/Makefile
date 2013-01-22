#	Id: Makefile,v 8.10 2001/01/28 15:50:08 skimo Exp (Berkeley) Date: 2001/01/28 15:50:08

MACROS=	-ms
ROFF=	groff
TBL=	tbl

all: vitut.ps summary.ps viapwh.ps ../html/vitut.html ../html/vi-summary.html \
	../html/viapwh.html

vitut.ps: vi.in vi.chars
	${TBL} vi.in vi.chars | ${ROFF} ${MACROS} > $@
	chmod 444 $@

summary.ps: vi.summary
	${TBL} vi.summary | ${ROFF}  ${MACROS} > $@
	chmod 444 $@

viapwh.ps: vi.apwh.ms
	${TBL} vi.apwh.ms | ${ROFF} ${MACROS} > $@
	chmod 444 $@

../html/vitut.html: vi.in vi.chars
	test -d ../html || mkdir ../html
	${TBL} $+ | ${ROFF} -ms -Thtml | \
	    sed 's/<img src=".*png">//' > $@
	rm -f *png
	chmod 444 $@

../html/vi-summary.html: vi.summary
	${TBL} $< | ${ROFF} -ms -Thtml | \
	    sed 's/<img src="$<.*png">//' > $@
	rm -f "$<"*png
	chmod 444 $@

../html/viapwh.html: vi.apwh.ms
	${TBL} $< | ${ROFF} -ms -Thtml | \
	    sed 's/<img src="$<.*png">//' > $@
	rm -f "$<"*png
	chmod 444 $@

clean:
	rm -f vitut.ps summary.ps viapwh.ps ../html/vitut.html \
	../html/vi-summary.html ../html/viapwh.html
