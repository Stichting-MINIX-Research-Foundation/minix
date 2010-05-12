
SHELL=/bin/sh

# To assist in cross-compiling
CC=exec cc
AR=ar
ARFLAGS=cr
RANLIB=ranlib
LDFLAGS=

BIGFILES=#-D_FILE_OFFSET_BITS=64
CFLAGS=-Wall -Winline -O -g $(BIGFILES) -Dlstat=stat -D_POSIX_SOURCE=1

# Where you want it installed when you do 'make install'
PREFIX=/usr
PREFIX_BIN=$(PREFIX)/bin
PREFIX_LIB=$(PREFIX)/lib
PREFIX_MAN=$(PREFIX)/man
PREFIX_INC=$(PREFIX)/include


OBJS= blocksort.o  \
      huffman.o    \
      crctable.o   \
      randtable.o  \
      compress.o   \
      decompress.o \
      bzlib.o

all: all_notest

all_notest: libbz2.a bzip2 bzip2recover 
	chmem =8000000 bzip2

bzip2: libbz2.a bzip2.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o bzip2 bzip2.o -L. -lbz2

bzip2recover: bzip2recover.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o bzip2recover bzip2recover.o

libbz2.a: $(OBJS)
	rm -f libbz2.a
	$(AR) $(ARFLAGS) libbz2.a $(OBJS)
	@if ( test -f $(RANLIB) -o -f /usr/bin/ranlib -o \
		-f /bin/ranlib -o -f /usr/ccs/bin/ranlib ) ; then \
		echo $(RANLIB) libbz2.a ; \
		$(RANLIB) libbz2.a ; \
	fi

check: test
test: bzip2 test_nodep

test_nodep:
	@cat words1
	./bzip2 -1  < sample1.ref > sample1.rb2
	./bzip2 -2  < sample2.ref > sample2.rb2
	./bzip2 -3  < sample3.ref > sample3.rb2
	./bzip2 -d  < sample1.bz2 > sample1.tst
	./bzip2 -d  < sample2.bz2 > sample2.tst
	./bzip2 -ds < sample3.bz2 > sample3.tst
	cmp sample1.bz2 sample1.rb2 
	cmp sample2.bz2 sample2.rb2
	cmp sample3.bz2 sample3.rb2
	cmp sample1.tst sample1.ref
	cmp sample2.tst sample2.ref
	cmp sample3.tst sample3.ref
	@cat words3

install: bzip2 bzip2recover
	if ( test ! -d $(PREFIX_BIN) ) ; then mkdir -p $(PREFIX_BIN) ; fi
	if ( test ! -d $(PREFIX_LIB) ) ; then mkdir -p $(PREFIX_LIB) ; fi
	if ( test ! -d $(PREFIX_MAN) ) ; then mkdir -p $(PREFIX_MAN) ; fi
	if ( test ! -d $(PREFIX_MAN)/man1 ) ; then mkdir -p $(PREFIX_MAN)/man1 ; fi
	if ( test ! -d $(PREFIX_INC) ) ; then mkdir -p $(PREFIX_INC) ; fi
	install -S 8M -o bin bzip2 $(PREFIX_BIN)/bzip2
	install -S 4M -o bin bzip2 $(PREFIX_BIN)/bunzip2
	install -S 2450k -o bin bzip2 $(PREFIX_BIN)/smallbunzip2
	install -o bin bzip2 $(PREFIX_BIN)/bzcat
	install -o bin bzip2recover $(PREFIX_BIN)/bzip2recover
	chmod a+x $(PREFIX_BIN)/bzip2
	chmod a+x $(PREFIX_BIN)/bunzip2
	chmod a+x $(PREFIX_BIN)/bzcat
	chmod a+x $(PREFIX_BIN)/bzip2recover
	install -o bin bzip2.1 $(PREFIX_MAN)/man1
	chmod a+r $(PREFIX_MAN)/man1/bzip2.1
	install -o bin bzlib.h $(PREFIX_INC)
	chmod a+r $(PREFIX_INC)/bzlib.h
	install -o bin libbz2.a $(PREFIX_LIB)
	chmod a+r $(PREFIX_LIB)/libbz2.a
	install -o bin bzgrep $(PREFIX_BIN)/bzgrep
	ln -f $(PREFIX_BIN)/bzgrep $(PREFIX_BIN)/bzegrep
	ln -f $(PREFIX_BIN)/bzgrep $(PREFIX_BIN)/bzfgrep
	chmod a+x $(PREFIX_BIN)/bzgrep
	install -o bin bzmore $(PREFIX_BIN)/bzmore
	ln -f $(PREFIX_BIN)/bzmore $(PREFIX_BIN)/bzless
	chmod a+x $(PREFIX_BIN)/bzmore
	install -o bin bzdiff $(PREFIX_BIN)/bzdiff
	ln -f $(PREFIX_BIN)/bzdiff $(PREFIX_BIN)/bzcmp
	chmod a+x $(PREFIX_BIN)/bzdiff
	install -o bin bzgrep.1 bzmore.1 bzdiff.1 $(PREFIX_MAN)/man1
	chmod a+r $(PREFIX_MAN)/man1/bzgrep.1
	chmod a+r $(PREFIX_MAN)/man1/bzmore.1
	chmod a+r $(PREFIX_MAN)/man1/bzdiff.1
	echo ".so man1/bzgrep.1" > $(PREFIX_MAN)/man1/bzegrep.1
	echo ".so man1/bzgrep.1" > $(PREFIX_MAN)/man1/bzfgrep.1
	echo ".so man1/bzmore.1" > $(PREFIX_MAN)/man1/bzless.1
	echo ".so man1/bzdiff.1" > $(PREFIX_MAN)/man1/bzcmp.1

clean: 
	rm -f *.o libbz2.a bzip2 bzip2recover \
	sample1.rb2 sample2.rb2 sample3.rb2 \
	sample1.tst sample2.tst sample3.tst

blocksort.o: blocksort.c
	@cat words0
	$(CC) $(CFLAGS) -c blocksort.c
huffman.o: huffman.c
	$(CC) $(CFLAGS) -c huffman.c
crctable.o: crctable.c
	$(CC) $(CFLAGS) -c crctable.c
randtable.o: randtable.c
	$(CC) $(CFLAGS) -c randtable.c
compress.o: compress.c
	$(CC) $(CFLAGS) -c compress.c
decompress.o: decompress.c
	$(CC) $(CFLAGS) -c decompress.c
bzlib.o: bzlib.c
	$(CC) $(CFLAGS) -c bzlib.c
bzip2.o: bzip2.c
	$(CC) $(CFLAGS) -c bzip2.c
bzip2recover.o: bzip2recover.c
	$(CC) $(CFLAGS) -c bzip2recover.c


distclean: clean
	rm -f manual.ps manual.html manual.pdf

DISTNAME=bzip2-1.0.3
dist: check manual
	rm -f $(DISTNAME)
	ln -sf . $(DISTNAME)
	tar cvf $(DISTNAME).tar \
	   $(DISTNAME)/blocksort.c \
	   $(DISTNAME)/huffman.c \
	   $(DISTNAME)/crctable.c \
	   $(DISTNAME)/randtable.c \
	   $(DISTNAME)/compress.c \
	   $(DISTNAME)/decompress.c \
	   $(DISTNAME)/bzlib.c \
	   $(DISTNAME)/bzip2.c \
	   $(DISTNAME)/bzip2recover.c \
	   $(DISTNAME)/bzlib.h \
	   $(DISTNAME)/bzlib_private.h \
	   $(DISTNAME)/Makefile \
	   $(DISTNAME)/LICENSE \
	   $(DISTNAME)/bzip2.1 \
	   $(DISTNAME)/bzip2.1.preformatted \
	   $(DISTNAME)/bzip2.txt \
	   $(DISTNAME)/words0 \
	   $(DISTNAME)/words1 \
	   $(DISTNAME)/words2 \
	   $(DISTNAME)/words3 \
	   $(DISTNAME)/sample1.ref \
	   $(DISTNAME)/sample2.ref \
	   $(DISTNAME)/sample3.ref \
	   $(DISTNAME)/sample1.bz2 \
	   $(DISTNAME)/sample2.bz2 \
	   $(DISTNAME)/sample3.bz2 \
	   $(DISTNAME)/dlltest.c \
	   $(DISTNAME)/manual.html \
	   $(DISTNAME)/manual.pdf \
	   $(DISTNAME)/manual.ps \
	   $(DISTNAME)/README \
	   $(DISTNAME)/README.COMPILATION.PROBLEMS \
	   $(DISTNAME)/README.XML.STUFF \
	   $(DISTNAME)/CHANGES \
	   $(DISTNAME)/libbz2.def \
	   $(DISTNAME)/libbz2.dsp \
	   $(DISTNAME)/dlltest.dsp \
	   $(DISTNAME)/makefile.msc \
	   $(DISTNAME)/Y2K_INFO \
	   $(DISTNAME)/unzcrash.c \
	   $(DISTNAME)/spewG.c \
	   $(DISTNAME)/mk251.c \
	   $(DISTNAME)/bzdiff \
	   $(DISTNAME)/bzdiff.1 \
	   $(DISTNAME)/bzmore \
	   $(DISTNAME)/bzmore.1 \
	   $(DISTNAME)/bzgrep \
	   $(DISTNAME)/bzgrep.1 \
	   $(DISTNAME)/Makefile-libbz2_so \
	   $(DISTNAME)/bz-common.xsl \
	   $(DISTNAME)/bz-fo.xsl \
	   $(DISTNAME)/bz-html.xsl \
	   $(DISTNAME)/bzip.css \
	   $(DISTNAME)/entities.xml \
	   $(DISTNAME)/manual.xml \
	   $(DISTNAME)/format.pl \
	   $(DISTNAME)/xmlproc.sh
	gzip -v $(DISTNAME).tar

# For rebuilding the manual from sources on my SuSE 9.1 box

MANUAL_SRCS= 	bz-common.xsl bz-fo.xsl bz-html.xsl bzip.css \
		entities.xml manual.xml 

manual: manual.html manual.ps manual.pdf

manual.ps: $(MANUAL_SRCS)
	./xmlproc.sh -ps manual.xml

manual.pdf: $(MANUAL_SRCS)
	./xmlproc.sh -pdf manual.xml

manual.html: $(MANUAL_SRCS)
	./xmlproc.sh -html manual.xml
