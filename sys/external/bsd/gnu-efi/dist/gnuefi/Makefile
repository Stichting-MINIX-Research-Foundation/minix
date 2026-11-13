#
#  Copyright (C) 1999-2001 Hewlett-Packard Co.
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

SRCDIR = .

VPATH = $(SRCDIR)

include $(SRCDIR)/../Make.defaults

TOPDIR = $(SRCDIR)/..

CDIR=$(TOPDIR)/..
FILES	= reloc_$(ARCH)  

OBJS	= $(FILES:%=%.o)

# on aarch64, avoid jump tables before all relocations have been processed
reloc_aarch64.o: CFLAGS += -fno-jump-tables

TARGETS	= crt0-efi-$(ARCH).o libgnuefi.a

all:	$(TARGETS)

libgnuefi.a: $(patsubst %,libgnuefi.a(%),$(OBJS))

clean:
	rm -f $(TARGETS) *~ *.o $(OBJS)

install:
	mkdir -p $(INSTALLROOT)$(LIBDIR)
	$(INSTALL) -m 644 $(TARGETS) $(INSTALLROOT)$(LIBDIR)
ifneq (,$(findstring FreeBSD,$(OS)))
 ifeq ($(ARCH),x86_64)
	$(INSTALL) -m 644 $(SRCDIR)/elf_$(ARCH)_fbsd_efi.lds $(INSTALLROOT)$(LIBDIR)
 else
	$(INSTALL) -m 644 $(SRCDIR)/elf_$(ARCH)_efi.lds $(INSTALLROOT)$(LIBDIR)
 endif
else
	$(INSTALL) -m 644 $(SRCDIR)/elf_$(ARCH)_efi.lds $(INSTALLROOT)$(LIBDIR)
endif

include $(SRCDIR)/../Make.rules
