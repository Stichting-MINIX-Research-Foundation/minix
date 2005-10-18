#####
##Top Makefile to build the Minix libraries for ack and gnu
#
##needed in makefile that includes this file:
#
##libraries to build without .a:
#LIBRARIES=liba libb libc
#
##object files per library:
#liba_OBJECTS=filea2.o filea2.o
#libb_OBJECTS=fileb.o
#libc_OBJECTS=filec.o
#
##if it has subdirectories
#SUBDIRS=subdir1 subdir2
#
#
##The main Makefile has to specify the ack and gnu base object directories:
#ACKBASE=/path/to/ack-obj
#GNUBASE=/path/to/gnu-obj
##or relative in this way: 
#ACKBASE=$(CURDIR)/ack-obj
#GNUBASE=$(CURDIR)/gnu-obj
#
##All these variables have to be set before including this Makefile
#
#
##To customize dependencies for targets you can do the following
#$(call ADDDEPENDENCIES,filea2.o fileb.o,header1.h header2.h)
#
##And to customize MAKE variables for targets use the same construct this way
#$(call ADDDEPENDENCIES,filea2.o fileb.o,CFLAGS+=-D_EXTRA_DEFINES -g)
#
##Only one variable can be set per call to ADDDEPENDENCIES
#

##Object files that must end up in the top level object directory must be in
##then variable named TOPLEVEL_OBJECTS

ifndef ACKBASE
$(error ACKBASE not set)
endif

ifndef GNUBASE
$(error GNUBASE not set)
endif

.PHONY: all 
all:: $(addsuffix .a, \
	$(addprefix $(ACKBASE)/,$(LIBRARIES)) \
	$(addprefix $(GNUBASE)/,$(LIBRARIES))) \
	$(addprefix $(ACKBASE)/,$(TOPLEVEL_OBJECTS)) \
	$(addprefix $(GNUBASE)/,$(TOPLEVEL_OBJECTS)) \
	subdirs

.PHONY: clean
clean:: subdirs
	rm -rf $(ACKBASE)/$(OBJDIR) $(GNUBASE)/$(OBJDIR)

.PHONY: install
install::

#Start objects
define TOPLEVEL_template
$$(addprefix $$(ACKBASE)/,$(1)): $$(addprefix $$(ACKBASE)/$$(OBJDIR)/,$(1))
	cp $$? $$@
$$(addprefix $$(GNUBASE)/,$(1)): $$(addprefix $$(GNUBASE)/$$(OBJDIR)/,$(1))
	cp $$? $$@
endef

$(foreach obj,$(TOPLEVEL_OBJECTS),$(eval $(call TOPLEVEL_template,$(obj))))

#Library stuff
define LIBRARY_template
$$(ACKBASE)/$(1).a: $$(addprefix $$(ACKBASE)/$$(OBJDIR)/,$$($(1)_OBJECTS))
	$$(AR) cr $$@ $$?

$$(GNUBASE)/$(1).a: $$(addprefix $$(GNUBASE)/$$(OBJDIR)/,$$($(1)_OBJECTS))
	$$(AR) cr $$@ $$?
	ranlib $$@
endef

$(foreach lib,$(LIBRARIES),$(eval $(call LIBRARY_template,$(lib))))

#subdirectories
.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS):
	mkdir -p $(ACKBASE)/$(OBJDIR)/$@
	mkdir -p $(GNUBASE)/$(OBJDIR)/$@
	ACKBASE=$(ACKBASE) GNUBASE=$(GNUBASE) OBJDIR=$(OBJDIR)/$@ $(MAKE) -C $@ $(MAKECMDGOALS)
#	$(call compiledir-ack,$@)
#	$(call compiledir-gnu,$@)

#ack vs gnu
define compiledir_ack
	mkdir -p $(ack_OBJDIR)/$(OBJDIR)/$(1)
	cd $(1) && OBJDIR=$(OBJDIR)/$(1) AR=ar AS='cc -c' CC=cc RANLIB= ASMEXT=s gmake $@ $(MAKECMDGOALS)
endef

define compiledir_gnu
	mkdir -p $(gnu_OBJDIR)/$(OBJDIR)/$(1)
	cd $(1) && OBJDIR=$(OBJDIR)/$(1) AR=gar AS=gas CC=gcc RANLIB=ranlib ASMEXT=gs gmake $@ $(MAKECMDGOALS)
endef

#Build commands
#$(GNUBASE)/$(OBJDIR)/%: AR=gar AS=gas CC=gcc RANLIB=ranlib ASMEXT=gs ASMPREFIX=$(GNUBASE)/$(OBJDIR)
#$(ACKBASE)/$(OBJDIR)/%: AR=ar AS='cc -c' CC=cc RANLIB= ASMEXT=s

#$(1)/$$(OBJDIR)/%.o: $(3)
define COMPILE_template
$(1)/%: AR=$(3)
$(1)/%: AS=$(4)
$(1)/%: CC=$(5)

$(1)/$$(OBJDIR)/%.o: %.$(2)
	$$(COMPILE.s) -o $$@ $$<

$(1)/$$(OBJDIR)/%.o: %.c
	$$(COMPILE.c) -o $$@ $$<
endef

#ASMPREFIX?
#$(1)/$$(OBJDIR)/%.o: $(2)%.$(3)
#	$$(COMPILE.s) -o $$@ $$<
#$(eval $(call COMPILE_template,$$(ACKBASE),,s,ar,cc -c,cc))
#$(eval $(call COMPILE_template,$$(GNUBASE),$$(GNUBASE)/$$(OBJDIR)/,gs,gar,gas,gcc))
#$(GNUBASE)/$(OBJDIR)/%.gs: %.s

#$(GNUBASE)/$(OBJDIR)/%.o: $(GNUBASE)/$(OBJDIR)/%.gs
#	$(COMPILE.s) -o $@ $<

#$(GNUBASE)/$(OBJDIR)/%.o: %.c
#	$(COMPILE.c) -o $@ $<

$(eval $(call COMPILE_template,$$(ACKBASE),s,ar,cc -c,cc))
$(eval $(call COMPILE_template,$$(GNUBASE),gs,gar,gas,gcc))

%.gs: %.s
	@echo converting $< to gnu assembler format...
	@-gcc $(CFLAGS) -E -x assembler-with-cpp -I. $< | asmconv -mi386 ack gnu > $@

#DEPENDENCIES
define DEPENDENCIES_template
$$(ACKBASE)/$$(OBJDIR)/$(1): $(2)
$$(GNUBASE)/$$(OBJDIR)/$(1): $(2)
endef

define ADDDEPENDENCIES
$(foreach target,$(1),$(eval $(call DEPENDENCIES_template,$(target),$(2))))
endef

#this could also be automated by adding an extra template that uses the names of object files as variable name
