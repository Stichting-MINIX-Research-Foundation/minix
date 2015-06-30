
include ../Makefile.settings

MODULE_NAME=$(MODULE).bcc

CFLAGS += $(CXXFLAGS) -Wall -g -Wno-long-long
CFLAGS += -D__MINIX -D_MINIX_SYSTEM

LLVM_VERSION=$(shell $(LLVMPREFIX)/bin/llvm-config --version | cut -d"." -f1,2 | sed "s/[^0-9]//g")

HAVE_NOCONSTRUCTOR_PRIORITY = $(shell expr $(LLVM_VERSION) \>= 30)
ifeq ($(HAVE_NOCONSTRUCTOR_PRIORITY),0)
CFLAGS += -DHAVE_NOCONSTRUCTOR_PRIORITY=1 
endif

ifeq ($(USE_SECTIONIFY),YES)
LOPT_FLAGS+=-load=$(LLVMSECTIONIFY_SO) -sectionify -sectionify-no-override -sectionify-data-section-map=$(SECTIONIFY_DATA_SECTION_MAP) -sectionify-function-section-map=$(SECTIONIFY_FUNCTION_SECTION_MAP)
ifeq (SECTIONIFY_NO_OVERRIDE,YES)
LOPT_FLAGS+=-sectionify-no-override
endif
endif

V?=0
ifneq ($V,0)
QUIET=
ECHO:=@\#
QMAKE=VERBOSE=1 make
else
QUIET= @
ECHO= echo
QMAKE=make -s
endif

$(MODULE_NAME): $(OBJS) $(HEADERS) $(COMMON_BCCS)
	$(QUIET) $(ECHO) "  [GOLD_LINK] $@.o"
	$(QUIET) $(LLVMGOLD_CC) $(CFLAGS) $(LLVMGOLD_CFLAGS) $(LLVMGOLD_LDFLAGS) -shared -o $@.o $(OBJS) $(COMMON_BCCS)
	$(QUIET) $(ECHO) "  [OPT] $@"
	$(QUIET) $(LLVMPREFIX)/bin/opt $(LOPT_FLAGS) -o $@ $@.o.bc

all: $(MODULE_NAME)

%.o: %.c $(HEADERS)
	$(QUIET) $(ECHO) "  [GOLD_CC] $<"
	$(QUIET) $(LLVMGOLD_CC) $(CFLAGS) $(LLVMGOLD_CFLAGS) $(INCLUDES) $(INCLUDES_$<) -c -o $@ $<

install: $(INSTALL_DIR)/$(MODULE_NAME)

$(INSTALL_DIR)/$(MODULE_NAME): $(MODULE_NAME)
	install -c -D -m 744 $? $@

clean: $(EXTRA_CLEAN)
	$(QUIET) $(ECHO) "  [RM] *.BCC *.bcc *.bc *.BCL *.BCL.sh *.bcl $(OBJS) *.s $(COMMON_BCCS)"
	$(QUIET) rm -f *.BCC *.bcc *.bc *.BCL *.BCL.sh *.bcl *.o $(OBJS) *.s $(COMMON_BCCS)

distclean: clean
	$(QUIET) $(ECHO) "  [RM] $(INSTALL_DIR)/$(MODULE_NAME)"
	$(QUIET) rm -f $(INSTALL_DIR)/$(MODULE_NAME)
