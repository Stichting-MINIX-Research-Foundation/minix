#
# Makefile for the nooks.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := swifi.o

# All of the (potential) objects that export symbols.
# This list comes from 'grep -l EXPORT_SYMBOL *.[hc]'.

export-objs = swifi-ksyms.o

obj-$(CONFIG_SWIFI) +=  db_sym.o db_disasm.o db_access.o \
			random.o fault_model.o swifi-ksyms.o

# db_interface.o 

include $(TOPDIR)/Rules.make

fastdep:

