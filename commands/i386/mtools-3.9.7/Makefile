# Generated automatically from Makefile.in by configure.
#
#       Makefile for Mtools
#
# check the Configure file for some examples of device-specific setups
# Berkeley flavors of Unix should include -DBSD in the CFLAGS.  Pick
# a lock method... either -DLOCKF, -DFLOCK, or -DFCNTL and put that
# string in the CFLAGS line below.

# User specified flags
USERCFLAGS = 
USERLDFLAGS = -stack 11m
USERLDLIBS =

MAKEINFO = makeinfo
TEXI2DVI = texi2dvi
TEXI2HTML = texi2html


# do not edit below this line
# =============================================================================

SHELL = /bin/sh

top_srcdir=.
srcdir=.

prefix      = /usr
exec_prefix = ${prefix}
bindir      = ${exec_prefix}/bin
infodir     = ${prefix}/info
mandir      = ${prefix}/man
infodir     = ${prefix}/info
sysconfdir  = /etc

CC         = cc -D_MINIX
CXX        = @CXX@
MYCFLAGS   = 
MYCXXFLAGS = 
CPPFLAGS   = 
HOST_ID    = -DCPU_i386 -DVENDOR_pc -DOS_Minix 
DEFS       = -DHAVE_CONFIG_H -DNO_CONFIG $(HOST_ID)

LDFLAGS     = 
LIBS        = 
SHLIB       = 
MACHDEPLIBS = 	
LN_S        = ln -s

INSTALL         = /usr/bin/install -cs
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA    = ${INSTALL} -m 644
INSTALL_INFO	= 

.SUFFIXES:
.SUFFIXES: .o .c
.SUFFIXES: .o .c

MAN1 = floppyd.1 mattrib.1 mbadblocks.1 mcat.1 mcd.1 mcopy.1 mdel.1 mdeltree.1 mdir.1 \
mdu.1 mformat.1 mkmanifest.1 mlabel.1 mmd.1 mmount.1 mmove.1 mpartition.1 \
mrd.1 mread.1 mren.1 mshowfat.1 mtoolstest.1 mtools.1 mtype.1 mzip.1
MAN1EXT	= 1
MAN1DIR	= $(mandir)/man${MAN1EXT}
MAN5	= mtools.5
MAN5EXT	= 5
MAN5DIR	= $(mandir)/man${MAN5EXT}

# all files in this directory included in the distribution 
DIST = \
COPYING Changelog INSTALL Makefile Makefile.in README Release.notes \
buffer.c buffer.h codepage.c codepage.h codepages.c config.c \
config.guess config.h.in config.log config.sub configure configure.in \
copyfile.c devices.c devices.h dirCache.c dirCache.h directory.c direntry.c \
expand.c fat.c \
fat_free.c file.c file.h file_name.c file_read.c files filter.c floppyd.1 \
floppyd.c floppyd_io.c floppyd_io.h force_io.c fs.h fsP.h \
getopt.h hash.c htable.h init.c llong.c mainloop.c match.c mattrib.1 \
mattrib.c mbadblocks.1 mbadblocks.c mcat.1 mcat.c mcd.1 mcd.c mcopy.1 \
mcopy.c mdel.1 mdel.c mdeltree.1 mdir.1 mdir.c mdu.c mdu.1 mformat.1 \
mformat.c minfo.c \
misc.c tty.c scsi.c missFuncs.c mk_direntry.c mkmanifest.1 mkmanifest.c \
mlabel.1 mlabel.c mmd.1 mmd.c mmount.1 mmount.c mmove.1 mmove.c \
mpartition.1 mpartition.c mrd.1 \
mread.1 mren.1 msdos.h mshowfat.1 mtoolstest.1 mtools.1 mtools.5 mtools.c \
mtools.conf mtools.h mtype.1 nameclash.h patchlevel.c \
plain_io.c plain_io.h precmd.c privileges.c scripts signal.c stream.c stream.h \
streamcache.c streamcache.h subdir.c sysincludes.h unixdir.c todo toupper.c \
vfat.c vfat.h xdf_io.c xdf_io.h

#OBJS1 = buffer.o codepage.o codepages.o config.o copyfile.o devices.o \
#dirCache.o directory.o direntry.o expand.o fat.o fat_free.o file.o file_name.o \
#file_read.o filter.o floppyd_io.o force_io.o hash.o init.o llong.o match.o \
#mainloop.o mattrib.o mbadblocks.o mcat.o mcd.o mcopy.o mdel.o mdir.o \
#mdoctorfat.o mdu.o \
#mformat.o minfo.o misc.o missFuncs.o mk_direntry.o mlabel.o mmd.o mmount.o \
#mmove.o mpartition.o mshowfat.o mzip.o mtools.o patchlevel.o plain_io.o \
#precmd.o privileges.o scsi.o signal.o stream.o streamcache.o subdir.o \
#unixdir.o toupper.o tty.o vfat.o xdf_io.o

OBJS1 = buffer.o config.o copyfile.o devices.o \
dirCache.o directory.o direntry.o expand.o fat.o fat_free.o file.o file_name.o \
file_read.o filter.o floppyd_io.o force_io.o hash.o init.o llong.o match.o \
mainloop.o mattrib.o mbadblocks.o mcat.o mcd.o mcopy.o mdel.o mdir.o \
mdoctorfat.o mdu.o \
mformat.o minfo.o misc.o missFuncs.o mk_direntry.o mlabel.o mmd.o mmount.o \
mmove.o mpartition.o mshowfat.o mtools.o patchlevel.o plain_io.o \
precmd.o privileges.o scsi.o signal.o stream.o streamcache.o subdir.o \
unixdir.o toupper.o tty.o vfat.o xdf_io.o

OBJS2 = missFuncs.o mkmanifest.o misc.o patchlevel.o

SRCS3 = floppyd.c

OBJS4 = floppyd_installtest.o misc.o expand.o privileges.o

SRCS = buffer.c codepage.c codepages.c config.c copyfile.c devices.c \
dirCache.c directory.c direntry.c expand.c fat.c fat_free.c file.c file_name.c \
file_read.c filter.c floppyd_io.c force_io.c hash.c init.c match.c mainloop.c \
mattrib.c mbadblocks.c mcat.c mcd.c mcopy.c mdel.c mdir.c mdu.c mdoctorfat.c \
mformat.c minfo.c misc.c \
missFuncs.c mk_direntry.c mlabel.c mmd.c mmount.c mmove.c mpartition.c \
mshowfat.c mzip.c mtools.c plain_io.c precmd.c privileges.c scsi.o \
signal.c stream.c streamcache.c subdir.c unixdir.c toupper.c tty.o vfat.c \
xdf_io.c mkmanifest.c


SCRIPTS = mcheck mxtar uz tgz mcomp

LINKS=mattrib mcat mcd mcopy mdel mdeltree mdir mdu mformat minfo mlabel \
mmd mmount mmove mpartition mrd mread mren mtype mtoolstest mshowfat \
mbadblocks mzip

X_CFLAGS = 
X_LIBS = 
X_EXTRA_LIBS = 
X_PRE_LIBS = 
CFLAGS = $(CPPFLAGS) $(DEFS) $(MYCFLAGS) -I.  -I. $(USERCFLAGS) 
CXXFLAGS  = $(CPPFLAGS) $(DEFS) $(MYCXXFLAGS) -I.  -I. $(USERCFLAGS) 
LINK      = $(CC) $(LDFLAGS) $(USERLDFLAGS) 
ALLLIBS   = $(USERLDLIBS) $(MACHDEPLIBS) $(SHLIB) $(LIBS)
X_LDFLAGS = $(X_EXTRA_LIBS) $(X_LIBS) $(X_PRE_LIBS) -lXau -lX11 $(LIBS) 
X_CCFLAGS = $(X_CFLAGS) $(CFLAGS)

all:    mtools

%.o: %.c
	$(CC) $(CFLAGS) -c $<

#%.o: %.cpp
#	$(CXX) $(CXXFLAGS) -c $<

mtools: $(OBJS1)
	$(LINK) $(OBJS1) -o $@ $(ALLLIBS)

mkmanifest: $(OBJS2)
	$(LINK) $(OBJS2) -o $@ $(ALLLIBS)

floppyd.o: floppyd.c
	$(CC) $(X_CCFLAGS) -c $<

floppyd: floppyd.o
	$(LINK) $< -o $@ $(X_LDFLAGS)
floppyd_installtest: $(OBJS4)
	$(LINK) $(OBJS4) -o $@ $(ALLLIBS)


$(LINKS): mtools
	rm -f $@ && $(LN_S) mtools $@

mostlyclean:
	-rm -f *~ *.orig *.o a.out core 2>/dev/null

clean:	mostlyclean
	-rm -f mtools $(LINKS) floppyd floppyd_installtest mkmanifest *.info* *.dvi *.html 2>/dev/null


texclean:
	-rm mtools.aux mtools.toc mtools.log
	-rm mtools.cps mtools.pgs mtools.vrs
	-rm mtools.cp mtools.fn mtools.ky
	-rm mtools.pg mtools.tp mtools.vr

info: mtools.info
%.info: %.texi
	$(MAKEINFO) -I$(srcdir) $< --no-split --output=$@

dvi: mtools.dvi
%.dvi: %.texi
	$(TEXI2DVI) $<

ps: mtools.ps
%.ps: %.dvi
	dvips -f < $< > $@

pdf: mtools.pdf
%.pdf: %.ps
	ps2pdf $< $@


html: mtools.html mtools_toc.html
%.html %_toc.html: %.texi
	$(TEXI2HTML) $<

# Don't cd, to avoid breaking install-sh references.
install-info: info
	$(top_srcdir)/mkinstalldirs $(infodir)
	if test -f mtools.info; then \
	  for i in mtools.info*; do \
	    $(INSTALL_DATA) $$i $(infodir)/$$i; \
	  done; \
	else \
	  for i in $(srcdir)/mtools.info*; do \
	    $(INSTALL_DATA) $$i $(infodir)/`echo $$i | sed 's|^$(srcdir)/||'`; \
	  done; \
	fi; \
	if [ -n "$(INSTALL_INFO)" ] ; then \
		if [ -f $(infodir)/dir.info ] ; then \
			$(INSTALL_INFO) $(infodir)/mtools.info $(infodir)/dir.info; \
		fi; \
		if [ -f $(infodir)/dir ] ; then \
			$(INSTALL_INFO) $(infodir)/mtools.info $(infodir)/dir; \
		fi; \
	fi

uninstall-info:
	cd $(infodir) && rm -f mtools.info*

install:	$(bindir)/mtools

# The manual pages are of such horrible quality that one is better off without
# them.  (Frankly this whole package is horrible.)  Using -? hopefully gives
# enough clues for use.  -- kjb
dontinstall:
		$(MAN1DIR)/mattrib.1 $(MAN1DIR)/mbadblocks.1 \
		$(MAN1DIR)/mcd.1 $(MAN1DIR)/mcopy.1 $(MAN1DIR)/mdel.1 \
		$(MAN1DIR)/mdeltree.1 $(MAN1DIR)/mdir.1 $(MAN1DIR)/mdu.1 \
		$(MAN1DIR)/mformat.1 $(MAN1DIR)/mlabel.1 \
		$(MAN1DIR)/mmd.1 $(MAN1DIR)/mmove.1 $(MAN1DIR)/mrd.1 \
		$(MAN1DIR)/mread.1 $(MAN1DIR)/mren.1 \
		$(MAN1DIR)/mshowfat.1 $(MAN1DIR)/mtools.1 \
		$(MAN1DIR)/mtype.1 $(MAN1DIR)/mzip.1

$(bindir)/mtools:	mtools
	install -c $? $@

$(MAN1DIR)/mattrib.1:	mattrib.1
	install -lc $? $@

$(MAN1DIR)/mbadblocks.1:	mbadblocks.1
	install -lc $? $@

$(MAN1DIR)/mcd.1:	mcd.1
	install -lc $? $@

$(MAN1DIR)/mcopy.1:	mcopy.1
	install -lc $? $@

$(MAN1DIR)/mdel.1:	mdel.1
	install -lc $? $@

$(MAN1DIR)/mdeltree.1:	mdeltree.1
	install -lc $? $@

$(MAN1DIR)/mdir.1:	mdir.1
	install -lc $? $@

$(MAN1DIR)/mdu.1:	mdu.1
	install -lc $? $@

$(MAN1DIR)/mformat.1:	mformat.1
	install -lc $? $@

$(MAN1DIR)/mlabel.1:	mlabel.1
	install -lc $? $@

$(MAN1DIR)/mmd.1:	mmd.1
	install -lc $? $@

$(MAN1DIR)/mmove.1:	mmove.1
	install -lc $? $@

$(MAN1DIR)/mrd.1:	mrd.1
	install -lc $? $@

$(MAN1DIR)/mread.1:	mread.1
	install -lc $? $@

$(MAN1DIR)/mren.1:	mren.1
	install -lc $? $@

$(MAN1DIR)/mshowfat.1:	mshowfat.1
	install -lc $? $@

$(MAN1DIR)/mtools.1:	mtools.1
	install -lc $? $@

$(MAN1DIR)/mtype.1:	mtype.1
	install -lc $? $@

$(MAN1DIR)/mzip.1:	mzip.1
	install -lc $? $@

#install:	$(bindir)/mtools  install-man install-links \
#		$(bindir)/mkmanifest install-scripts install-info
#
#uninstall:	uninstall-bin uninstall-man uninstall-links \
#		uninstall-scripts

distclean: clean texclean
	rm -f config.cache config.h config.status config.log Makefile
maintainer-clean: distclean


#$(bindir)/floppyd: floppyd
#	$(top_srcdir)/mkinstalldirs $(bindir)
#	$(INSTALL_PROGRAM) floppyd $(bindir)/floppyd
#
#$(bindir)/floppyd_installtest: floppyd_installtest
#	$(top_srcdir)/mkinstalldirs $(bindir)
#	$(INSTALL_PROGRAM) floppyd_installtest $(bindir)/floppyd_installtest
#
#$(bindir)/mtools: mtools
#	$(top_srcdir)/mkinstalldirs $(bindir)
#	$(INSTALL_PROGRAM) mtools $(bindir)/mtools
#
#$(bindir)/mkmanifest: mkmanifest
#	$(top_srcdir)/mkinstalldirs $(bindir)
#	$(INSTALL_PROGRAM) mkmanifest $(bindir)/mkmanifest

#$(ETCDIR)/mtools: mtools.etc
#	cp mtools.etc $(ETCDIR)/mtools

install-links: $(bindir)/mtools
	@for j in $(LINKS); do \
		rm -f $(bindir)/$$j ; \
		$(LN_S) mtools $(bindir)/$$j ; \
		echo $(bindir)/$$j ; \
	done

## "z" is the older version of "gz"; the name is just *too* short
install-scripts: $(bindir)/mtools
	@$(top_srcdir)/mkinstalldirs $(bindir)
	@for j in $(SCRIPTS) ; do \
		$(INSTALL_PROGRAM) $(srcdir)/scripts/$$j $(bindir)/$$j ; \
		echo $(bindir)/$$j ; \
	done
	rm -f $(bindir)/lz
	$(LN_S) uz $(bindir)/lz

install-man:
	@$(top_srcdir)/mkinstalldirs $(MAN1DIR)
	@for j in $(MAN1); do \
		$(INSTALL_DATA) $(srcdir)/$$j $(MAN1DIR)/$$j ; \
		echo $(MAN1DIR)/$$j ; \
	done
	@$(top_srcdir)/mkinstalldirs $(MAN5DIR)
	@for j in $(MAN5); do \
		$(INSTALL_DATA) $(srcdir)/$$j $(MAN5DIR)/$$j ; \
		echo $(MAN5DIR)/$$j ; \
	done

uninstall-bin:
	@for j in mtools mkmanifest; do \
		rm -f $(bindir)/$$j ; \
		echo $(bindir)/$$j ; \
	done

uninstall-scripts:
	@for j in $(SCRIPTS); do \
		rm -f $(bindir)/$$j ; \
		echo $(bindir)/$$j ; \
	done

uninstall-man:
	@for j in $(MAN1); do \
		rm -f $(MAN1DIR)/$$j ; \
		echo $(MAN1DIR)/$$j ; \
	done
	@for j in $(MAN5); do \
		rm -f $(MAN5DIR)/$$j ; \
		echo $(MAN5DIR)/$$j ; \
	done

uninstall-links:
	@for j in $(LINKS); \
		do rm -f $(bindir)/$$j ; \
		echo $(bindir)/$$j ; \
	done

depend: $(SRCS)
	makedepend -- $(CFLAGS) -- $^

check:
	echo No self tests included
# check target needed even if empty, in order to make life easyer for
# automatic tools to install GNU soft


# DO NOT DELETE THIS LINE -- make depend depends on it.
