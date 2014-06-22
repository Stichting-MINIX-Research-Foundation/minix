include ../Makefile.inc

QUIET=@
ECHO=echo
CP=cp

PASSLIBNAME:= weak-alias-module-override.so

LLVM_VERSION = $($LLVMPREFIX/bin/llvm-config --version | sed "s/[^0-9]//g")
CFLAGS  += -Wno-long-long -Wall -W -Wno-unused-parameter -Wwrite-strings -DHAVE_EXCEPTIONS=0
CFLAGS  += $(shell $(LLVMPREFIX)/bin/llvm-config --cxxflags) -g -DLLVM_VERSION=$(LLVM_VERSION)
LDFLAGS += $(shell $(LLVMPREFIX)/bin/llvm-config --ldflags)
OBJS= WeakAliasModuleOverride.o

$(PASSLIBNAME): $(OBJS)
	$(QUIET) $(ECHO) "  [LINK] $@"
	$(QUIET) $(CXX) $(CFLAGS) -shared -o $@ $(CPPS) $(OBJS) $(LDFLAGS) $(LIBS)

%.o: %.cpp $(HEADERS)
	$(QUIET) $(ECHO)  "  [C++] $<"
	$(QUIET) $(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

install: $(PASSLIBNAME)
	$(QUIET) -mkdir -p ../../bin
	$(QUIET) $(CP)  $(PASSLIBNAME) ../../bin/$(PASSLIBNAME)

clean:
	-rm -f *.o *.so
