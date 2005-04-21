# Makefile for lib/i86/em.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -Was-ncc
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../../libe.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(em_adi.o) \
	$(LIBRARY)(em_and.o) \
	$(LIBRARY)(em_blm.o) \
	$(LIBRARY)(em_cii.o) \
	$(LIBRARY)(em_cmi4.o) \
	$(LIBRARY)(em_cms.o) \
	$(LIBRARY)(em_cmu4.o) \
	$(LIBRARY)(em_com.o) \
	$(LIBRARY)(em_csa2.o) \
	$(LIBRARY)(em_csa4.o) \
	$(LIBRARY)(em_csb2.o) \
	$(LIBRARY)(em_csb4.o) \
	$(LIBRARY)(em_cuu.o) \
	$(LIBRARY)(em_dup.o) \
	$(LIBRARY)(em_dvi4.o) \
	$(LIBRARY)(em_dvu4.o) \
	$(LIBRARY)(em_error.o) \
	$(LIBRARY)(em_exg.o) \
	$(LIBRARY)(em_fat.o) \
	$(LIBRARY)(em_fp8087.o) \
	$(LIBRARY)(em_gto.o) \
	$(LIBRARY)(em_hol0.o) \
	$(LIBRARY)(em_iaar.o) \
	$(LIBRARY)(em_ilar.o) \
	$(LIBRARY)(em_inn.o) \
	$(LIBRARY)(em_ior.o) \
	$(LIBRARY)(em_isar.o) \
	$(LIBRARY)(em_lar2.o) \
	$(LIBRARY)(em_lfr6.o) \
	$(LIBRARY)(em_lfr8.o) \
	$(LIBRARY)(em_loi.o) \
	$(LIBRARY)(em_mli4.o) \
	$(LIBRARY)(em_mon.o) \
	$(LIBRARY)(em_nop.o) \
	$(LIBRARY)(em_rck.o) \
	$(LIBRARY)(em_ret6.o) \
	$(LIBRARY)(em_ret8.o) \
	$(LIBRARY)(em_retarea.o) \
	$(LIBRARY)(em_return.o) \
	$(LIBRARY)(em_rmi4.o) \
	$(LIBRARY)(em_rmu4.o) \
	$(LIBRARY)(em_sar2.o) \
	$(LIBRARY)(em_sbi.o) \
	$(LIBRARY)(em_set.o) \
	$(LIBRARY)(em_stb.o) \
	$(LIBRARY)(em_sti.o) \
	$(LIBRARY)(em_stop.o) \
	$(LIBRARY)(em_trp.o) \
	$(LIBRARY)(em_unknown.o) \
	$(LIBRARY)(em_vars.o) \
	$(LIBRARY)(em_xor.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(em_adi.o):	em_adi.s
	$(CC1) em_adi.s

$(LIBRARY)(em_and.o):	em_and.s
	$(CC1) em_and.s

$(LIBRARY)(em_blm.o):	em_blm.s
	$(CC1) em_blm.s

$(LIBRARY)(em_cii.o):	em_cii.s
	$(CC1) em_cii.s

$(LIBRARY)(em_cmi4.o):	em_cmi4.s
	$(CC1) em_cmi4.s

$(LIBRARY)(em_cms.o):	em_cms.s
	$(CC1) em_cms.s

$(LIBRARY)(em_cmu4.o):	em_cmu4.s
	$(CC1) em_cmu4.s

$(LIBRARY)(em_com.o):	em_com.s
	$(CC1) em_com.s

$(LIBRARY)(em_csa2.o):	em_csa2.s
	$(CC1) em_csa2.s

$(LIBRARY)(em_csa4.o):	em_csa4.s
	$(CC1) em_csa4.s

$(LIBRARY)(em_csb2.o):	em_csb2.s
	$(CC1) em_csb2.s

$(LIBRARY)(em_csb4.o):	em_csb4.s
	$(CC1) em_csb4.s

$(LIBRARY)(em_cuu.o):	em_cuu.s
	$(CC1) em_cuu.s

$(LIBRARY)(em_dup.o):	em_dup.s
	$(CC1) em_dup.s

$(LIBRARY)(em_dvi4.o):	em_dvi4.s
	$(CC1) em_dvi4.s

$(LIBRARY)(em_dvu4.o):	em_dvu4.s
	$(CC1) em_dvu4.s

$(LIBRARY)(em_error.o):	em_error.s
	$(CC1) em_error.s

$(LIBRARY)(em_exg.o):	em_exg.s
	$(CC1) em_exg.s

$(LIBRARY)(em_fat.o):	em_fat.s
	$(CC1) em_fat.s

$(LIBRARY)(em_fp8087.o):	em_fp8087.s
	$(CC1) em_fp8087.s

$(LIBRARY)(em_gto.o):	em_gto.s
	$(CC1) em_gto.s

$(LIBRARY)(em_hol0.o):	em_hol0.s
	$(CC1) em_hol0.s

$(LIBRARY)(em_iaar.o):	em_iaar.s
	$(CC1) em_iaar.s

$(LIBRARY)(em_ilar.o):	em_ilar.s
	$(CC1) em_ilar.s

$(LIBRARY)(em_inn.o):	em_inn.s
	$(CC1) em_inn.s

$(LIBRARY)(em_ior.o):	em_ior.s
	$(CC1) em_ior.s

$(LIBRARY)(em_isar.o):	em_isar.s
	$(CC1) em_isar.s

$(LIBRARY)(em_lar2.o):	em_lar2.s
	$(CC1) em_lar2.s

$(LIBRARY)(em_lfr6.o):	em_lfr6.s
	$(CC1) em_lfr6.s

$(LIBRARY)(em_lfr8.o):	em_lfr8.s
	$(CC1) em_lfr8.s

$(LIBRARY)(em_loi.o):	em_loi.s
	$(CC1) em_loi.s

$(LIBRARY)(em_mli4.o):	em_mli4.s
	$(CC1) em_mli4.s

$(LIBRARY)(em_mon.o):	em_mon.s
	$(CC1) em_mon.s

$(LIBRARY)(em_nop.o):	em_nop.s
	$(CC1) em_nop.s

$(LIBRARY)(em_rck.o):	em_rck.s
	$(CC1) em_rck.s

$(LIBRARY)(em_ret6.o):	em_ret6.s
	$(CC1) em_ret6.s

$(LIBRARY)(em_ret8.o):	em_ret8.s
	$(CC1) em_ret8.s

$(LIBRARY)(em_retarea.o):	em_retarea.s
	$(CC1) em_retarea.s

$(LIBRARY)(em_return.o):	em_return.s
	$(CC1) em_return.s

$(LIBRARY)(em_rmi4.o):	em_rmi4.s
	$(CC1) em_rmi4.s

$(LIBRARY)(em_rmu4.o):	em_rmu4.s
	$(CC1) em_rmu4.s

$(LIBRARY)(em_sar2.o):	em_sar2.s
	$(CC1) em_sar2.s

$(LIBRARY)(em_sbi.o):	em_sbi.s
	$(CC1) em_sbi.s

$(LIBRARY)(em_set.o):	em_set.s
	$(CC1) em_set.s

$(LIBRARY)(em_stb.o):	em_stb.s
	$(CC1) em_stb.s

$(LIBRARY)(em_sti.o):	em_sti.s
	$(CC1) em_sti.s

$(LIBRARY)(em_stop.o):	em_stop.s
	$(CC1) em_stop.s

$(LIBRARY)(em_trp.o):	em_trp.s
	$(CC1) em_trp.s

$(LIBRARY)(em_unknown.o):	em_unknown.s
	$(CC1) em_unknown.s

$(LIBRARY)(em_vars.o):	em_vars.s
	$(CC1) em_vars.s

$(LIBRARY)(em_xor.o):	em_xor.s
	$(CC1) em_xor.s
