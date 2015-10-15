.PHONY: 	 clean install installwww
.SUFFIXES:	 .sgml .html .md5 .h .h.html
.SUFFIXES:	 .1       .3       .7       .8
.SUFFIXES:	 .1.html  .3.html  .7.html  .8.html

# Specify this if you want to hard-code the operating system to appear
# in the lower-left hand corner of -mdoc manuals.
#
# CFLAGS	+= -DOSNAME="\"OpenBSD 5.4\""

VERSION		 = 1.13.0
VDATE		 = 4 January 2014

# IFF your system supports multi-byte functions (setlocale(), wcwidth(),
# putwchar()) AND has __STDC_ISO_10646__ (that is, wchar_t is simply a
# UCS-4 value) should you define USE_WCHAR.  If you define it and your
# system DOESN'T support this, -Tlocale will produce garbage.
# If you don't define it, -Tlocale is a synonym for -Tacsii.
#
CFLAGS	 	+= -DUSE_WCHAR

# If your system has manpath(1), uncomment this.  This is most any
# system that's not OpenBSD or NetBSD.  If uncommented, manpage(1) and
# mandocdb(8) will use manpath(1) to get the MANPATH variable.
#CFLAGS		+= -DUSE_MANPATH

# If your system does not support static binaries, comment this,
# for example on Mac OS X.
STATIC		 = -static
# Linux requires -pthread to statically link with libdb.
#STATIC		+= -pthread

CFLAGS		+= -I/usr/local/include -g -DHAVE_CONFIG_H
CFLAGS     	+= -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings
PREFIX		 = /usr/local
WWWPREFIX	 = /var/www
HTDOCDIR	 = $(WWWPREFIX)/htdocs
CGIBINDIR	 = $(WWWPREFIX)/cgi-bin
BINDIR		 = $(PREFIX)/bin
INCLUDEDIR	 = $(PREFIX)/include/mandoc
LIBDIR		 = $(PREFIX)/lib/mandoc
MANDIR		 = $(PREFIX)/man
EXAMPLEDIR	 = $(PREFIX)/share/examples/mandoc
INSTALL		 = install
INSTALL_PROGRAM	 = $(INSTALL) -m 0755
INSTALL_DATA	 = $(INSTALL) -m 0444
INSTALL_LIB	 = $(INSTALL) -m 0644
INSTALL_SOURCE	 = $(INSTALL) -m 0644
INSTALL_MAN	 = $(INSTALL_DATA)

DBLIB		 = -L/usr/local/lib -lsqlite3
DBBIN		 = mandocdb manpage apropos

all: mandoc preconv demandoc $(DBBIN)

TESTSRCS	 = test-fgetln.c \
		   test-getsubopt.c \
		   test-mmap.c \
		   test-ohash.c \
		   test-strlcat.c \
		   test-strlcpy.c \
		   test-strnlen.c \
		   test-strptime.c

SRCS		 = Makefile \
		   NEWS \
		   TODO \
		   apropos.1 \
		   apropos.c \
		   arch.c \
		   arch.in \
		   att.c \
		   att.in \
		   cgi.c \
		   chars.c \
		   chars.in \
		   compat_fgetln.c \
		   compat_getsubopt.c \
		   compat_ohash.c \
		   compat_ohash.h \
		   compat_strcasestr.c \
		   compat_strlcat.c \
		   compat_strlcpy.c \
		   compat_strnlen.c \
		   compat_strsep.c \
		   config.h.post \
		   config.h.pre \
		   configure \
		   demandoc.1 \
		   demandoc.c \
		   eqn.7 \
		   eqn.c \
		   eqn_html.c \
		   eqn_term.c \
		   example.style.css \
		   external.png \
		   gmdiff \
		   html.c \
		   html.h \
		   index.css \
		   index.sgml \
		   lib.c \
		   lib.in \
		   libman.h \
		   libmandoc.h \
		   libmdoc.h \
		   libroff.h \
		   main.c \
		   main.h \
		   man.7 \
		   man.c \
		   man-cgi.css \
		   man.h \
		   man_hash.c \
		   man_html.c \
		   man_macro.c \
		   man_term.c \
		   man_validate.c \
		   mandoc.1 \
		   mandoc.3 \
		   mandoc.c \
		   mandoc.h \
		   mandoc_char.7 \
		   mandocdb.8 \
		   mandocdb.c \
		   manpage.c \
		   manpath.c \
		   manpath.h \
		   mansearch.c \
		   mansearch.h \
		   mdoc.7 \
		   mdoc.c \
		   mdoc.h \
		   mdoc_argv.c \
		   mdoc_hash.c \
		   mdoc_html.c \
		   mdoc_macro.c \
		   mdoc_man.c \
		   mdoc_term.c \
		   mdoc_validate.c \
		   msec.c \
		   msec.in \
		   out.c \
		   out.h \
		   preconv.1 \
		   preconv.c \
		   predefs.in \
		   read.c \
		   roff.7 \
		   roff.c \
		   st.c \
		   st.in \
		   style.css \
		   tbl.3 \
		   tbl.7 \
		   tbl.c \
		   tbl_data.c \
		   tbl_html.c \
		   tbl_layout.c \
		   tbl_opts.c \
		   tbl_term.c \
		   term.c \
		   term.h \
		   term_ascii.c \
		   term_ps.c \
		   tree.c \
		   vol.c \
		   vol.in \
		   $(TESTSRCS)

LIBMAN_OBJS	 = man.o \
		   man_hash.o \
		   man_macro.o \
		   man_validate.o

LIBMDOC_OBJS	 = arch.o \
		   att.o \
		   lib.o \
		   mdoc.o \
		   mdoc_argv.o \
		   mdoc_hash.o \
		   mdoc_macro.o \
		   mdoc_validate.o \
		   st.o \
		   vol.o

LIBROFF_OBJS	 = eqn.o \
		   roff.o \
		   tbl.o \
		   tbl_data.o \
		   tbl_layout.o \
		   tbl_opts.o

LIBMANDOC_OBJS	 = $(LIBMAN_OBJS) \
		   $(LIBMDOC_OBJS) \
		   $(LIBROFF_OBJS) \
		   chars.o \
		   mandoc.o \
		   msec.o \
		   read.o

COMPAT_OBJS	 = compat_fgetln.o \
		   compat_getsubopt.o \
		   compat_ohash.o \
		   compat_strcasestr.o \
		   compat_strlcat.o \
		   compat_strlcpy.o \
		   compat_strnlen.o \
		   compat_strsep.o

arch.o: arch.in
att.o: att.in
chars.o: chars.in
lib.o: lib.in
msec.o: msec.in
roff.o: predefs.in
st.o: st.in
vol.o: vol.in

$(LIBMAN_OBJS): libman.h
$(LIBMDOC_OBJS): libmdoc.h
$(LIBROFF_OBJS): libroff.h
$(LIBMANDOC_OBJS): mandoc.h mdoc.h man.h libmandoc.h config.h
$(COMPAT_OBJS): config.h compat_ohash.h

MANDOC_HTML_OBJS = eqn_html.o \
		   html.o \
		   man_html.o \
		   mdoc_html.o \
		   tbl_html.o
$(MANDOC_HTML_OBJS): html.h

MANDOC_MAN_OBJS  = mdoc_man.o

MANDOC_TERM_OBJS = eqn_term.o \
		   man_term.o \
		   mdoc_term.o \
		   term.o \
		   term_ascii.o \
		   term_ps.o \
		   tbl_term.o
$(MANDOC_TERM_OBJS): term.h

MANDOC_OBJS	 = $(MANDOC_HTML_OBJS) \
		   $(MANDOC_MAN_OBJS) \
		   $(MANDOC_TERM_OBJS) \
		   main.o \
		   out.o \
		   tree.o
$(MANDOC_OBJS): main.h mandoc.h mdoc.h man.h config.h out.h

MANDOCDB_OBJS	 = mandocdb.o manpath.o
$(MANDOCDB_OBJS): mansearch.h mandoc.h mdoc.h man.h config.h manpath.h

PRECONV_OBJS	 = preconv.o
$(PRECONV_OBJS): config.h

APROPOS_OBJS	 = apropos.o mansearch.o manpath.o
$(APROPOS_OBJS): config.h manpath.h mansearch.h

MANPAGE_OBJS	 = manpage.o mansearch.o manpath.o
$(MANPAGE_OBJS): config.h manpath.h mansearch.h

DEMANDOC_OBJS	 = demandoc.o
$(DEMANDOC_OBJS): config.h

INDEX_MANS	 = apropos.1.html \
		   demandoc.1.html \
		   mandoc.1.html \
		   preconv.1.html \
		   mandoc.3.html \
		   tbl.3.html \
		   eqn.7.html \
		   man.7.html \
		   mandoc_char.7.html \
		   mdoc.7.html \
		   roff.7.html \
		   tbl.7.html \
		   mandocdb.8.html

$(INDEX_MANS): mandoc

INDEX_OBJS	 = $(INDEX_MANS) \
		   man.h.html \
		   mandoc.h.html \
		   mdoc.h.html \
		   mdocml.tar.gz \
		   mdocml.md5

www: index.html

clean:
	rm -f libmandoc.a $(LIBMANDOC_OBJS)
	rm -f apropos $(APROPOS_OBJS)
	rm -f mandocdb $(MANDOCDB_OBJS)
	rm -f preconv $(PRECONV_OBJS)
	rm -f manpage $(MANPAGE_OBJS)
	rm -f demandoc $(DEMANDOC_OBJS)
	rm -f mandoc $(MANDOC_OBJS)
	rm -f config.h config.log $(COMPAT_OBJS)
	rm -f mdocml.tar.gz
	rm -f index.html $(INDEX_OBJS)
	rm -rf *.dSYM

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(EXAMPLEDIR)
	mkdir -p $(DESTDIR)$(LIBDIR)
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	mkdir -p $(DESTDIR)$(MANDIR)/man3
	mkdir -p $(DESTDIR)$(MANDIR)/man7
	$(INSTALL_PROGRAM) mandoc preconv demandoc $(DESTDIR)$(BINDIR)
	$(INSTALL_LIB) libmandoc.a $(DESTDIR)$(LIBDIR)
	$(INSTALL_LIB) man.h mdoc.h mandoc.h $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL_MAN) mandoc.1 preconv.1 demandoc.1 $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_MAN) mandoc.3 tbl.3 $(DESTDIR)$(MANDIR)/man3
	$(INSTALL_MAN) man.7 mdoc.7 roff.7 eqn.7 tbl.7 mandoc_char.7 $(DESTDIR)$(MANDIR)/man7
	$(INSTALL_DATA) example.style.css $(DESTDIR)$(EXAMPLEDIR)

installcgi: all
	mkdir -p $(DESTDIR)$(CGIBINDIR)
	mkdir -p $(DESTDIR)$(HTDOCDIR)
	#$(INSTALL_PROGRAM) man.cgi $(DESTDIR)$(CGIBINDIR)
	$(INSTALL_DATA) example.style.css $(DESTDIR)$(HTDOCDIR)/man.css
	$(INSTALL_DATA) man-cgi.css $(DESTDIR)$(HTDOCDIR)

installwww: www
	mkdir -p $(PREFIX)/snapshots
	mkdir -p $(PREFIX)/binaries
	$(INSTALL_DATA) index.html external.png index.css $(PREFIX)
	$(INSTALL_DATA) $(INDEX_MANS) style.css $(PREFIX)
	$(INSTALL_DATA) mandoc.h.html man.h.html mdoc.h.html $(PREFIX)
	$(INSTALL_DATA) mdocml.tar.gz $(PREFIX)/snapshots
	$(INSTALL_DATA) mdocml.md5 $(PREFIX)/snapshots
	$(INSTALL_DATA) mdocml.tar.gz $(PREFIX)/snapshots/mdocml-$(VERSION).tar.gz
	$(INSTALL_DATA) mdocml.md5 $(PREFIX)/snapshots/mdocml-$(VERSION).md5

libmandoc.a: $(COMPAT_OBJS) $(LIBMANDOC_OBJS)
	$(AR) rs $@ $(COMPAT_OBJS) $(LIBMANDOC_OBJS)

mandoc: $(MANDOC_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(MANDOC_OBJS) libmandoc.a

mandocdb: $(MANDOCDB_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(MANDOCDB_OBJS) libmandoc.a $(DBLIB)

preconv: $(PRECONV_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(PRECONV_OBJS)

manpage: $(MANPAGE_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(MANPAGE_OBJS) libmandoc.a $(DBLIB)

apropos: $(APROPOS_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(APROPOS_OBJS) libmandoc.a $(DBLIB)

demandoc: $(DEMANDOC_OBJS) libmandoc.a
	$(CC) $(LDFLAGS) -o $@ $(DEMANDOC_OBJS) libmandoc.a

mdocml.md5: mdocml.tar.gz
	md5 mdocml.tar.gz >$@

mdocml.tar.gz: $(SRCS)
	mkdir -p .dist/mdocml-$(VERSION)/
	$(INSTALL_SOURCE) $(SRCS) .dist/mdocml-$(VERSION)
	chmod 755 .dist/mdocml-$(VERSION)/configure
	( cd .dist/ && tar zcf ../$@ mdocml-$(VERSION) )
	rm -rf .dist/

index.html: $(INDEX_OBJS)

config.h: configure config.h.pre config.h.post $(TESTSRCS)
	rm -f config.log
	CC="$(CC)" CFLAGS="$(CFLAGS)" VERSION="$(VERSION)" ./configure

.h.h.html:
	highlight -I $< >$@

.1.1.html .3.3.html .7.7.html .8.8.html:
	./mandoc -Thtml -Wall,stop -Ostyle=style.css,man=%N.%S.html,includes=%I.html $< >$@

.sgml.html:
	validate --warn $<
	sed -e "s!@VERSION@!$(VERSION)!" -e "s!@VDATE@!$(VDATE)!" $< >$@
