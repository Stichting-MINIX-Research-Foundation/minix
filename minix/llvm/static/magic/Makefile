#Makefile for creating the magic static library

MODULE = magic
OBJS = magic.o magic_analysis.o magic_asr.o magic_eval_lib.o magic_eval.o magic_mem.o magic_range.o magic_splay_tree.o magic_sentry.o magic_selement.o
HEADERS = $(wildcard $(LLVMINC)/*.h) $(wildcard $(LLVMINC)/st/*.h)

USE_SECTIONIFY=YES
SECTIONIFY_NO_OVERRIDE=yes
SECTIONIFY_DATA_SECTION_MAP=^_____magic_instr_.*/magic_instr_data,.*/magic_data
SECTIONIFY_FUNCTION_SECTION_MAP=.*/magic_functions

OBJS += minix/magic_util.o minix/magic_ds.o
HEADERS += $(wildcard minix/*.h)

OBJS+=magic_st.bcc

include Makefile.inc

CFLAGS += -I$(_MINIX_ROOT)/minix/servers

# XXX: there is essential code in assert() statements, so force asserts on..
CFLAGS += -UNDEBUG

magic_st.bcc:
	make -f Makefile.magic_st $@

magic_st.o:
	make -f Makefile.magic_st $@

