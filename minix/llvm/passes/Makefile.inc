# This file is not part of the regular Makefile.inc
# It is used from the LLVM build system, when compiling the passes.
#
# This files is used with gmake, so do NOT add .include "..."
#
LLVMPREFIX?=../../../../../obj_llvm.i386/Release+Asserts

CFLAGS += -D__MINIX -D_MINIX_SYSTEM -DDEBUG_TYPE=\""$PASSNAME"\"
INCLUDES += -I../../include

QUIET=@
ECHO=echo
CP=cp

PASSLIBNAME = $(PASSNAME).so

LLVM_VERSION=$(shell ($(LLVMPREFIX)/bin/llvm-config --version | sed "s/[^0-9]//g"))
CFLAGS  += -Wno-long-long -Wall -W -Wno-unused-parameter -Wwrite-strings -DHAVE_EXCEPTIONS=0
CFLAGS  += $(shell $(LLVMPREFIX)/bin/llvm-config --cxxflags) -g -DLLVM_VERSION=$(LLVM_VERSION)
LDFLAGS += $(shell $(LLVMPREFIX)/bin/llvm-config --ldflags)

INCLUDES += -I../include

all: $(PASSLIBNAME)

$(PASSLIBNAME): $(OBJS)
	$(QUIET) $(ECHO) "  [LINK] $@"
	$(QUIET) $(CXX) $(CFLAGS) -shared -o $@ $(CPPS) $(OBJS) $(LDFLAGS) $(LIBS)

%.o: %.cpp $(HEADERS)
	$(QUIET) $(ECHO)  "  [C++] $<"
	$(QUIET) $(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

install: $(PASSLIBNAME)
	$(QUIET) -mkdir -p ../../bin
	$(QUIET) $(CP) $(PASSLIBNAME) ../../bin/$(PASSLIBNAME)

clean:
	-rm -f $(OBJS) $(PASSLIBNAME)
