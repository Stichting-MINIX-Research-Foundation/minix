#	$eterna: Makefile,v 1.14 2009/05/22 21:51:39 mrg Exp $

SIMPLETESTS=	t1 t2 t3 t4 t5 t6 t7 t8 t9 t10
BIGFILETESTS=	partial4000 partial8000

BOZOHTTPD?=	../bozohttpd
BOZOHTTPD?=	../debug/bozohttpd-debug
WGET?=		wget

all:

clean:
	for a in $(SIMPLETESTS); do \
		rm -f tmp.$$a.out; \
	done

check: check-simple check-bigfile

check-simple:
.for a in $(SIMPLETESTS)
	echo "Running test $a"
	$(BOZOHTTPD) ./data < $(.CURDIR)/$a.in > tmp.$a.out || true
	$(.CURDIR)/html_cmp $(.CURDIR)/$a.out tmp.$a.out
.endfor

check-bigfile:
.for a in $(BIGFILETESTS)
	echo "Running test $a"
	$(.CURDIR)/test-bigfile "$a" "${BOZOHTTPD}" "${WGET}" "./data"
.endfor

.include <bsd.obj.mk>
