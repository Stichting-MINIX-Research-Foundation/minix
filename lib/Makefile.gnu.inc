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


ifndef GNUBASE
$(error GNUBASE not set)
endif

.PHONY: all 
all:: $(addsuffix .a, \
	$(addprefix $(GNUBASE)/,$(LIBRARIES))) \
	$(addprefix $(GNUBASE)/,$(TOPLEVEL_OBJECTS)) \
	subdirs

.PHONY: clean
clean:: subdirs
	rm -rf $(GNUBASE)/$(OBJDIR)

#Start objects
define TOPLEVEL_template
$$(addprefix $$(GNUBASE)/,$(1)): $$(addprefix $$(GNUBASE)/$$(OBJDIR)/,$(1))
	cp $$? $$@
endef

$(foreach obj,$(TOPLEVEL_OBJECTS),$(eval $(call TOPLEVEL_template,$(obj))))

#Library stuff
define LIBRARY_template
$$(GNUBASE)/$(1).a: $$(addprefix $$(GNUBASE)/$$(OBJDIR)/,$$($(1)_OBJECTS))
	$$(AR) cr $$@ $$?
	ranlib $$@
endef

$(foreach lib,$(LIBRARIES),$(eval $(call LIBRARY_template,$(lib))))

#subdirectories
.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS):
	if [ ! -d $(GNUBASE)/$(OBJDIR)/$@ ]; \
	then \
		mkdir $(GNUBASE)/$(OBJDIR)/$@; \
	fi
	GNUBASE=$(GNUBASE) OBJDIR=$(OBJDIR)/$@ $(MAKE) -C $@ $(MAKECMDGOALS)

#Build commands
#define COMPILE_template
$(GNUBASE)/%: AR=gar
$(GNUBASE)/%: AS=gas
$(GNUBASE)/%: CC=gcc

$(GNUBASE)/$(OBJDIR)/%.o: %.gs
	$(COMPILE.s) -o $@ $<

$(GNUBASE)/$(OBJDIR)/%.o: %.c
	$(COMPILE.c) -o $@ $<
#endef

#$(eval $(call COMPILE_template,$$(GNUBASE),gs,gar,gas,gcc))

%.gs: %.s
	@echo converting $< to gnu assembler format...
	@-gcc $(CFLAGS) -E -x assembler-with-cpp -I. $< | asmconv -mi386 ack gnu > $@

#DEPENDENCIES
define DEPENDENCIES_template
$$(GNUBASE)/$$(OBJDIR)/$(1): $(2)
endef

define ADDDEPENDENCIES
$(foreach target,$(1),$(eval $(call DEPENDENCIES_template,$(target),$(2))))
endef
