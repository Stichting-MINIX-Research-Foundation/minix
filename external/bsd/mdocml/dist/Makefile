.SUFFIXES:	.html .xml .sgml .1 .3 .7 .md5 .tar.gz 
.SUFFIXES:	.1.txt .3.txt .7.txt
.SUFFIXES:	.1.xhtml .3.xhtml .7.xhtml
.SUFFIXES:	.1.sgml .3.sgml .7.sgml 
.SUFFIXES:	.h .h.html 
.SUFFIXES:	.1.ps .3.ps .7.ps
.SUFFIXES:	.1.pdf .3.pdf .7.pdf

PREFIX		= /usr/local
BINDIR		= $(PREFIX)/bin
INCLUDEDIR	= $(PREFIX)/include
LIBDIR		= $(PREFIX)/lib
MANDIR		= $(PREFIX)/man
EXAMPLEDIR	= $(PREFIX)/share/examples/mandoc
INSTALL		= install
INSTALL_PROGRAM	= $(INSTALL) -m 0755
INSTALL_DATA	= $(INSTALL) -m 0444
INSTALL_LIB	= $(INSTALL) -m 0644
INSTALL_MAN	= $(INSTALL_DATA)

VERSION	   = 1.10.9
VDATE	   = 07 January 2010

VFLAGS	   = -DVERSION="\"$(VERSION)\""
WFLAGS     = -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings
CFLAGS    += -g $(WFLAGS) $(VFLAGS) -DHAVE_CONFIG_H

# Specify this if you want to hard-code the operating system to appear
# in the lower-left hand corner of -mdoc manuals.
# CFLAGS += -DOSNAME="\"OpenBSD 4.5\""
CFLAGS += -UOSNAME -DOSNAME="\"Minix\""

LINTFLAGS += $(VFLAGS)

ROFFLNS    = roff.ln tbl.ln tbl_opts.ln tbl_layout.ln tbl_data.ln

ROFFSRCS   = roff.c tbl.c tbl_opts.c tbl_layout.c tbl_data.c

ROFFOBJS   = roff.o tbl.o tbl_opts.o tbl_layout.o tbl_data.o

MANDOCLNS  = mandoc.ln

MANDOCSRCS = mandoc.c

MANDOCOBJS = mandoc.o

MDOCLNS	   = mdoc_macro.ln mdoc.ln mdoc_hash.ln mdoc_strings.ln \
	     mdoc_argv.ln mdoc_validate.ln \
	     lib.ln att.ln arch.ln vol.ln msec.ln st.ln

MDOCOBJS   = mdoc_macro.o mdoc.o mdoc_hash.o mdoc_strings.o \
	     mdoc_argv.o mdoc_validate.o lib.o att.o \
	     arch.o vol.o msec.o st.o

MDOCSRCS   = mdoc_macro.c mdoc.c mdoc_hash.c mdoc_strings.c \
	     mdoc_argv.c mdoc_validate.c lib.c att.c \
	     arch.c vol.c msec.c st.c

MANLNS	   = man_macro.ln man.ln man_hash.ln man_validate.ln \
	     man_argv.ln

MANOBJS	   = man_macro.o man.o man_hash.o man_validate.o \
	     man_argv.o
MANSRCS	   = man_macro.c man.c man_hash.c man_validate.c \
	     man_argv.c

MAINLNS	   = main.ln mdoc_term.ln chars.ln term.ln tree.ln \
	     compat.ln man_term.ln html.ln mdoc_html.ln \
	     man_html.ln out.ln term_ps.ln term_ascii.ln \
	     tbl_term.ln tbl_html.ln

MAINOBJS   = main.o mdoc_term.o chars.o term.o tree.o compat.o \
	     man_term.o html.o mdoc_html.o man_html.o out.o \
	     term_ps.o term_ascii.o tbl_term.o tbl_html.o

MAINSRCS   = main.c mdoc_term.c chars.c term.c tree.c compat.c \
	     man_term.c html.c mdoc_html.c man_html.c out.c \
	     term_ps.c term_ascii.c tbl_term.c tbl_html.c

LLNS	   = llib-llibmdoc.ln llib-llibman.ln llib-lmandoc.ln \
	     llib-llibmandoc.ln llib-llibroff.ln

LNS	   = $(MAINLNS) $(MDOCLNS) $(MANLNS) \
	     $(MANDOCLNS) $(ROFFLNS)

LIBS	   = libmdoc.a libman.a libmandoc.a libroff.a

OBJS	   = $(MDOCOBJS) $(MAINOBJS) $(MANOBJS) \
	     $(MANDOCOBJS) $(ROFFOBJS)

SRCS	   = $(MDOCSRCS) $(MAINSRCS) $(MANSRCS) \
	     $(MANDOCSRCS) $(ROFFSRCS)

DATAS	   = arch.in att.in lib.in msec.in st.in \
	     vol.in chars.in

HEADS	   = mdoc.h libmdoc.h man.h libman.h term.h \
	     libmandoc.h html.h chars.h out.h main.h roff.h \
	     mandoc.h libroff.h

GSGMLS	   = mandoc.1.sgml mdoc.3.sgml mdoc.7.sgml \
	     mandoc_char.7.sgml man.7.sgml man.3.sgml roff.7.sgml \
	     roff.3.sgml tbl.7.sgml

SGMLS	   = index.sgml

XHTMLS	   = mandoc.1.xhtml mdoc.3.xhtml \
	     man.3.xhtml mdoc.7.xhtml man.7.xhtml mandoc_char.7.xhtml \
	     roff.7.xhtml roff.3.xhtml tbl.7.xhtml

HTMLS	   = ChangeLog.html index.html man.h.html mdoc.h.html \
	     mandoc.h.html roff.h.html mandoc.1.html mdoc.3.html \
	     man.3.html mdoc.7.html man.7.html mandoc_char.7.html \
	     roff.7.html roff.3.html tbl.7.html

PSS	   = mandoc.1.ps mdoc.3.ps man.3.ps mdoc.7.ps man.7.ps \
	     mandoc_char.7.ps roff.7.ps roff.3.ps tbl.7.ps

PDFS	   = mandoc.1.pdf mdoc.3.pdf man.3.pdf mdoc.7.pdf man.7.pdf \
	     mandoc_char.7.pdf roff.7.pdf roff.3.pdf tbl.7.pdf

XSLS	   = ChangeLog.xsl

TEXTS	   = mandoc.1.txt mdoc.3.txt man.3.txt mdoc.7.txt man.7.txt \
	     mandoc_char.7.txt ChangeLog.txt \
	     roff.7.txt roff.3.txt tbl.7.txt

EXAMPLES   = example.style.css

XMLS	   = ChangeLog.xml

STATICS	   = index.css style.css external.png

MD5S	   = mdocml-$(VERSION).md5 

TARGZS	   = mdocml-$(VERSION).tar.gz

MANS	   = mandoc.1 mdoc.3 mdoc.7 mandoc_char.7 man.7 \
	     man.3 roff.7 roff.3 tbl.7

BINS	   = mandoc

TESTS	   = test-strlcat.c test-strlcpy.c

CONFIGS	   = config.h.pre config.h.post

DOCLEAN	   = $(BINS) $(LNS) $(LLNS) $(LIBS) $(OBJS) $(HTMLS) \
	     $(TARGZS) tags $(MD5S) $(XMLS) $(TEXTS) $(GSGMLS) \
	     config.h config.log $(PSS) $(PDFS) $(XHTMLS)

DOINSTALL  = $(SRCS) $(HEADS) Makefile $(MANS) $(SGMLS) $(STATICS) \
	     $(DATAS) $(XSLS) $(EXAMPLES) $(TESTS) $(CONFIGS)

all:	$(BINS)

lint:	$(LLNS)

clean:
	rm -f $(DOCLEAN)

dist:	mdocml-$(VERSION).tar.gz

www:	all $(GSGMLS) $(HTMLS) $(XHTMLS) $(TEXTS) $(MD5S) $(TARGZS) $(PSS) $(PDFS)

ps:	$(PSS)

pdf:	$(PDFS)

installwww: www
	$(INSTALL_DATA) $(HTMLS) $(XHTMLS) $(PSS) $(PDFS) $(TEXTS) $(STATICS) $(DESTDIR)$(PREFIX)/
	$(INSTALL_DATA) mdocml-$(VERSION).tar.gz $(DESTDIR)$(PREFIX)/snapshots/
	$(INSTALL_DATA) mdocml-$(VERSION).md5 $(DESTDIR)$(PREFIX)/snapshots/
	$(INSTALL_DATA) mdocml-$(VERSION).tar.gz $(DESTDIR)$(PREFIX)/snapshots/mdocml.tar.gz
	$(INSTALL_DATA) mdocml-$(VERSION).md5 $(DESTDIR)$(PREFIX)/snapshots/mdocml.md5

install:
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(EXAMPLEDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	mkdir -p $(DESTDIR)$(MANDIR)/man7
	$(INSTALL_PROGRAM) mandoc $(DESTDIR)$(BINDIR)
	$(INSTALL_MAN) mandoc.1 $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_MAN) man.7 mdoc.7 roff.7 tbl.7 mandoc_char.7 $(DESTDIR)$(MANDIR)/man7
	$(INSTALL_DATA) example.style.css $(DESTDIR)$(EXAMPLEDIR)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/mandoc
	rm -f $(DESTDIR)$(MANDIR)/man1/mandoc.1
	rm -f $(DESTDIR)$(MANDIR)/man7/mdoc.7
	rm -f $(DESTDIR)$(MANDIR)/man7/roff.7
	rm -f $(DESTDIR)$(MANDIR)/man7/tbl.7
	rm -f $(DESTDIR)$(MANDIR)/man7/man.7
	rm -f $(DESTDIR)$(MANDIR)/man7/mandoc_char.7
	rm -f $(DESTDIR)$(EXAMPLEDIR)/example.style.css

$(OBJS): config.h

$(LNS): config.h

man_macro.ln man_macro.o: man_macro.c libman.h

lib.ln lib.o: lib.c lib.in libmdoc.h

att.ln att.o: att.c att.in libmdoc.h

arch.ln arch.o: arch.c arch.in libmdoc.h

vol.ln vol.o: vol.c vol.in libmdoc.h

chars.ln chars.o: chars.c chars.in chars.h

msec.ln msec.o: msec.c msec.in libmdoc.h

st.ln st.o: st.c st.in libmdoc.h

mdoc_macro.ln mdoc_macro.o: mdoc_macro.c libmdoc.h

mdoc_term.ln mdoc_term.o: mdoc_term.c term.h mdoc.h

mdoc_strings.ln mdoc_strings.o: mdoc_strings.c libmdoc.h

man_hash.ln man_hash.o: man_hash.c libman.h

mdoc_hash.ln mdoc_hash.o: mdoc_hash.c libmdoc.h

mdoc.ln mdoc.o: mdoc.c libmdoc.h

man.ln man.o: man.c libman.h

main.ln main.o: main.c mdoc.h man.h roff.h

compat.ln compat.o: compat.c 

term.ln term.o: term.c term.h man.h mdoc.h chars.h

term_ps.ln term_ps.o: term_ps.c term.h main.h

term_ascii.ln term_ascii.o: term_ascii.c term.h main.h

html.ln html.o: html.c html.h chars.h

mdoc_html.ln mdoc_html.o: mdoc_html.c html.h mdoc.h

man_html.ln man_html.o: man_html.c html.h man.h out.h

out.ln out.o: out.c out.h

mandoc.ln mandoc.o: mandoc.c libmandoc.h

tree.ln tree.o: tree.c man.h mdoc.h

mdoc_argv.ln mdoc_argv.o: mdoc_argv.c libmdoc.h

man_argv.ln man_argv.o: man_argv.c libman.h

man_validate.ln man_validate.o: man_validate.c libman.h

mdoc_validate.ln mdoc_validate.o: mdoc_validate.c libmdoc.h

libmdoc.h: mdoc.h

ChangeLog.xml:
	cvs2cl --xml --xml-encoding iso-8859-15 -t --noxmlns -f $@

ChangeLog.txt:
	cvs2cl -t -f $@

ChangeLog.html: ChangeLog.xml ChangeLog.xsl
	xsltproc -o $@ ChangeLog.xsl ChangeLog.xml

mdocml-$(VERSION).tar.gz: $(DOINSTALL)
	mkdir -p .dist/mdocml/mdocml-$(VERSION)/
	cp -f $(DOINSTALL) .dist/mdocml/mdocml-$(VERSION)/
	( cd .dist/mdocml/ && tar zcf ../../$@ mdocml-$(VERSION)/ )
	rm -rf .dist/

llib-llibmdoc.ln: $(MDOCLNS)
	$(LINT) -Clibmdoc $(MDOCLNS)

llib-llibman.ln: $(MANLNS)
	$(LINT) -Clibman $(MANLNS)

llib-llibmandoc.ln: $(MANDOCLNS)
	$(LINT) -Clibmandoc $(MANDOCLNS)

llib-llibroff.ln: $(ROFFLNS)
	$(LINT) -Clibroff $(ROFFLNS)

llib-lmandoc.ln: $(MAINLNS) llib-llibmdoc.ln llib-llibman.ln llib-llibmandoc.ln llib-llibroff.ln
	$(LINT) -Cmandoc $(MAINLNS) llib-llibmdoc.ln llib-llibman.ln llib-llibmandoc.ln llib-llibroff.ln

libmdoc.a: $(MDOCOBJS)
	$(AR) rs $@ $(MDOCOBJS)

libman.a: $(MANOBJS)
	$(AR) rs $@ $(MANOBJS)

libmandoc.a: $(MANDOCOBJS)
	$(AR) rs $@ $(MANDOCOBJS)

libroff.a: $(ROFFOBJS)
	$(AR) rs $@ $(ROFFOBJS)

mandoc: $(MAINOBJS) libroff.a libmdoc.a libman.a libmandoc.a
	$(CC) $(CFLAGS) -o $@ $(MAINOBJS) libroff.a libmdoc.a libman.a libmandoc.a

.sgml.html:
	validate --warn $<
	sed -e "s!@VERSION@!$(VERSION)!" -e "s!@VDATE@!$(VDATE)!" $< > $@

.1.1.txt .3.3.txt .7.7.txt:
	./mandoc -Tascii -Wall,stop $< | col -b > $@

.1.1.sgml .3.3.sgml .7.7.sgml:
	./mandoc -Thtml -Wall,stop -Ostyle=style.css,man=%N.%S.html,includes=%I.html $< > $@

.1.1.ps .3.3.ps .7.7.ps:
	./mandoc -Tps -Wall,stop $< > $@

.1.1.xhtml .3.3.xhtml .7.7.xhtml:
	./mandoc -Txhtml -Wall,stop -Ostyle=style.css,man=%N.%S.xhtml,includes=%I.html $< > $@

.1.1.pdf .3.3.pdf .7.7.pdf:
	./mandoc -Tpdf -Wall,stop $< > $@

.tar.gz.md5:
	md5 $< > $@

.h.h.html:
	highlight -I $< >$@

config.h: config.h.pre config.h.post
	rm -f config.log
	( cat config.h.pre; \
	echo; \
	if $(CC) $(CFLAGS) -Werror -c test-strlcat.c >> config.log 2>&1; then \
		echo '#define HAVE_STRLCAT'; \
		rm test-strlcat.o; \
	fi; \
	if $(CC) $(CFLAGS) -Werror -c test-strlcpy.c >> config.log 2>&1; then \
		echo '#define HAVE_STRLCPY'; \
		rm test-strlcpy.o; \
	fi; \
	echo; \
	cat config.h.post \
	) > $@
