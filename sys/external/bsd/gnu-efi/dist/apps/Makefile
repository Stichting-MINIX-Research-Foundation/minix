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
LINUX_HEADERS	= /usr/src/sys/build
CPPFLAGS	+= -D__KERNEL__ -I$(LINUX_HEADERS)/include
CRTOBJS		= ../gnuefi/crt0-efi-$(ARCH).o

LDSCRIPT	= $(TOPDIR)/gnuefi/elf_$(ARCH)_efi.lds
ifneq (,$(findstring FreeBSD,$(OS)))
LDSCRIPT	= $(TOPDIR)/gnuefi/elf_$(ARCH)_fbsd_efi.lds
endif

LDFLAGS		+= -shared -Bsymbolic -L../lib -L../gnuefi $(CRTOBJS)

LOADLIBES	+= -lefi -lgnuefi
LOADLIBES	+= $(LIBGCC)
LOADLIBES	+= -T $(LDSCRIPT)

TARGET_APPS = t.efi t2.efi t3.efi t4.efi t5.efi t6.efi \
	      printenv.efi t7.efi t8.efi tcc.efi modelist.efi \
	      route80h.efi drv0_use.efi AllocPages.efi exit.efi \
	      FreePages.efi setjmp.efi debughook.efi debughook.efi.debug \
	      bltgrid.efi lfbgrid.efi setdbg.efi unsetdbg.efi
TARGET_BSDRIVERS = drv0.efi
TARGET_RTDRIVERS =

ifneq ($(HAVE_EFI_OBJCOPY),)

FORMAT		:= --target efi-app-$(ARCH)
$(TARGET_BSDRIVERS): FORMAT=--target efi-bsdrv-$(ARCH)
$(TARGET_RTDRIVERS): FORMAT=--target efi-rtdrv-$(ARCH)

else

SUBSYSTEM	:= 0xa
$(TARGET_BSDRIVERS): SUBSYSTEM = 0xb
$(TARGET_RTDRIVERS): SUBSYSTEM = 0xc

FORMAT		:= -O binary
LDFLAGS		+= --defsym=EFI_SUBSYSTEM=$(SUBSYSTEM)

endif

TARGETS = $(TARGET_APPS) $(TARGET_BSDRIVERS) $(TARGET_RTDRIVERS)

all:	$(TARGETS)

clean:
	rm -f $(TARGETS) *~ *.o *.so

.PHONY: install

include $(SRCDIR)/../Make.rules
