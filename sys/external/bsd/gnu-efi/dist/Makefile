#
#  Copyright (C) 1999-2007 Hewlett-Packard Co.
#	Contributed by David Mosberger <davidm@hpl.hp.com>
#	Contributed by Stephane Eranian <eranian@hpl.hp.com>
#
#    All rights reserved.
#
#    Redistribution and use in source and binary forms, with or without
#    modification, are permitted provided that the following conditions
#    are met:
#
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
#      copyright notice, this list of conditions and the following
#      disclaimer in the documentation and/or other materials
#      provided with the distribution.
#    * Neither the name of Hewlett-Packard Co. nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#
#    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
#    CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
#    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
#    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
#    BE LIABLE FOR ANYDIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
#    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
#    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
#    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
#    SUCH DAMAGE.
#

VERSION = 3.0.8

MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
SRCDIR = $(dir $(MKFILE_PATH))

VPATH = $(SRCDIR)

include $(SRCDIR)/Make.defaults

SUBDIRS = lib gnuefi inc apps
gnuefi: lib

all:	check_gcc $(SUBDIRS)

mkvars:
	@echo AR=$(AR)
	@echo ARCH=$(ARCH)
	@echo ARCH3264=$(ARCH3264)
	@echo AS=$(AS)
	@echo ASFLAGS=$(ASFLAGS)
	@echo CC=$(CC)
	@echo CFLAGS=$(CFLAGS)
	@echo CPPFLAGS=$(CPPFLAGS)
	@echo GCCMINOR=$(GCCMINOR)
	@echo GCCNEWENOUGH=$(GCCNEWENOUGH)
	@echo GCCVERSION=$(GCCVERSION)
	@echo HOSTARCH=$(HOSTARCH)
	@echo INCDIR=$(INCDIR)
	@echo INSTALL=$(INSTALL)
	@echo INSTALLROOT=$(INSTALLROOT)
	@echo LD=$(LD)
	@echo LDFLAGS=$(LDFLAGS)
	@echo LIBDIR=$(LIBDIR)
	@echo OBJCOPY=$(OBJCOPY)
	@echo OS=$(OS)
	@echo prefix=$(prefix)
	@echo PREFIX=$(PREFIX)
	@echo RANLIB=$(RANLIB)
	@echo SRCDIR=$(SRCDIR)
	@echo TOPDIR=$(TOPDIR)

$(SUBDIRS):
	mkdir -p $(OBJDIR)/$@
	$(MAKE) -C $(OBJDIR)/$@ -f $(SRCDIR)/$@/Makefile SRCDIR=$(SRCDIR)/$@ ARCH=$(ARCH)

clean:
	rm -f *~
	@for d in $(SUBDIRS); do \
		if [ -d $(OBJDIR)/$$d ]; then \
			$(MAKE) -C $(OBJDIR)/$$d -f $(SRCDIR)/$$d/Makefile SRCDIR=$(SRCDIR)/$$d clean; \
		fi; \
	done

install:
	@for d in $(SUBDIRS); do \
		mkdir -p $(OBJDIR)/$$d; \
		$(MAKE) -C $(OBJDIR)/$$d -f $(SRCDIR)/$$d/Makefile SRCDIR=$(SRCDIR)/$$d install; done

.PHONY:	$(SUBDIRS) clean depend

#
# on both platforms you must use gcc 3.0 or higher 
#
check_gcc:
ifeq ($(GCC_VERSION),2)
	@echo "you need to use a version of gcc >= 3.0, you are using `$(CC) --version`"
	@exit 1
endif

include $(SRCDIR)/Make.rules

test-archive:
	@rm -rf /tmp/gnu-efi-$(VERSION) /tmp/gnu-efi-$(VERSION)-tmp
	@mkdir -p /tmp/gnu-efi-$(VERSION)-tmp
	@git archive --format=tar $(shell git branch | awk '/^*/ { print $$2 }') | ( cd /tmp/gnu-efi-$(VERSION)-tmp/ ; tar x )
	@git diff | ( cd /tmp/gnu-efi-$(VERSION)-tmp/ ; patch -s -p1 -b -z .gitdiff )
	@mv /tmp/gnu-efi-$(VERSION)-tmp/ /tmp/gnu-efi-$(VERSION)/
	@dir=$$PWD; cd /tmp; tar -c --bzip2 -f $$dir/gnu-efi-$(VERSION).tar.bz2 gnu-efi-$(VERSION)
	@rm -rf /tmp/gnu-efi-$(VERSION)
	@echo "The archive is in gnu-efi-$(VERSION).tar.bz2"

tag:
	git tag $(VERSION) refs/heads/master

archive: tag
	@rm -rf /tmp/gnu-efi-$(VERSION) /tmp/gnu-efi-$(VERSION)-tmp
	@mkdir -p /tmp/gnu-efi-$(VERSION)-tmp
	@git archive --format=tar $(VERSION) | ( cd /tmp/gnu-efi-$(VERSION)-tmp/ ; tar x )
	@mv /tmp/gnu-efi-$(VERSION)-tmp/ /tmp/gnu-efi-$(VERSION)/
	@dir=$$PWD; cd /tmp; tar -c --bzip2 -f $$dir/gnu-efi-$(VERSION).tar.bz2 gnu-efi-$(VERSION)
	@rm -rf /tmp/gnu-efi-$(VERSION)
	@echo "The archive is in gnu-efi-$(VERSION).tar.bz2"

