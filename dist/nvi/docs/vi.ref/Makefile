#	Id: Makefile,v 8.25 2001/08/20 09:40:50 skimo Exp (Berkeley) Date: 2001/08/20 09:40:50

MACROS=	-me
ROFF=	groff
TBL=	tbl

all: vi.ref.txt vi.ref.ps ../html/vi.ref.html

vi.ref.txt: vi.texi
	-makeinfo --no-headers vi.texi > $@ 2>/dev/null
	chmod 444 $@

vi.ref.ps: vi.texi
	texi2dvi -c vi.texi
	dvips -o $@ vi.dvi
	rm -f vi.dvi
	chmod 444 $@

../html/vi.ref.html: vi.texi
	test -d ../html || mkdir ../html
	-makeinfo --html --no-split vi.texi --output=$@ 2>/dev/null
	chmod 444 $@

clean:
	rm -f vi.ref.ps vi.ref.txt index index.so ../html/vi.ref.html
