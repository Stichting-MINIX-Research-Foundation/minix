# Makefile for lib/i386/em.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -Was-ack
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../../libe.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(em_adf4.o) \
	$(LIBRARY)(em_adf8.o) \
	$(LIBRARY)(em_adi.o) \
	$(LIBRARY)(em_and.o) \
	$(LIBRARY)(em_blm.o) \
	$(LIBRARY)(em_cff4.o) \
	$(LIBRARY)(em_cff8.o) \
	$(LIBRARY)(em_cfi.o) \
	$(LIBRARY)(em_cfu.o) \
	$(LIBRARY)(em_cif4.o) \
	$(LIBRARY)(em_cif8.o) \
	$(LIBRARY)(em_cii.o) \
	$(LIBRARY)(em_cmf4.o) \
	$(LIBRARY)(em_cmf8.o) \
	$(LIBRARY)(em_cms.o) \
	$(LIBRARY)(em_com.o) \
	$(LIBRARY)(em_csa4.o) \
	$(LIBRARY)(em_csb4.o) \
	$(LIBRARY)(em_cuf4.o) \
	$(LIBRARY)(em_cuf8.o) \
	$(LIBRARY)(em_cuu.o) \
	$(LIBRARY)(em_dup.o) \
	$(LIBRARY)(em_dvf4.o) \
	$(LIBRARY)(em_dvf8.o) \
	$(LIBRARY)(em_dvi.o) \
	$(LIBRARY)(em_dvu.o) \
	$(LIBRARY)(em_error.o) \
	$(LIBRARY)(em_exg.o) \
	$(LIBRARY)(em_fat.o) \
	$(LIBRARY)(em_fef4.o) \
	$(LIBRARY)(em_fef8.o) \
	$(LIBRARY)(em_fif4.o) \
	$(LIBRARY)(em_fif8.o) \
	$(LIBRARY)(em_fp8087.o) \
	$(LIBRARY)(em_gto.o) \
	$(LIBRARY)(em_hol0.o) \
	$(LIBRARY)(em_iaar.o) \
	$(LIBRARY)(em_ilar.o) \
	$(LIBRARY)(em_inn.o) \
	$(LIBRARY)(em_ior.o) \
	$(LIBRARY)(em_isar.o) \
	$(LIBRARY)(em_lar4.o) \
	$(LIBRARY)(em_loi.o) \
	$(LIBRARY)(em_mlf4.o) \
	$(LIBRARY)(em_mlf8.o) \
	$(LIBRARY)(em_mli.o) \
	$(LIBRARY)(em_mon.o) \
	$(LIBRARY)(em_ngf4.o) \
	$(LIBRARY)(em_ngf8.o) \
	$(LIBRARY)(em_ngi.o) \
	$(LIBRARY)(em_nop.o) \
	$(LIBRARY)(em_print.o) \
	$(LIBRARY)(em_rck.o) \
	$(LIBRARY)(em_rmi.o) \
	$(LIBRARY)(em_rmu.o) \
	$(LIBRARY)(em_rol.o) \
	$(LIBRARY)(em_ror.o) \
	$(LIBRARY)(em_sar4.o) \
	$(LIBRARY)(em_sbf4.o) \
	$(LIBRARY)(em_sbf8.o) \
	$(LIBRARY)(em_sbi.o) \
	$(LIBRARY)(em_set.o) \
	$(LIBRARY)(em_sli.o) \
	$(LIBRARY)(em_sri.o) \
	$(LIBRARY)(em_sti.o) \
	$(LIBRARY)(em_stop.o) \
	$(LIBRARY)(em_trp.o) \
	$(LIBRARY)(em_unknown.o) \
	$(LIBRARY)(em_xor.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(em_adf4.o):	em_adf4.s
	$(CC1) em_adf4.s

$(LIBRARY)(em_adf8.o):	em_adf8.s
	$(CC1) em_adf8.s

$(LIBRARY)(em_adi.o):	em_adi.s
	$(CC1) em_adi.s

$(LIBRARY)(em_and.o):	em_and.s
	$(CC1) em_and.s

$(LIBRARY)(em_blm.o):	em_blm.s
	$(CC1) em_blm.s

$(LIBRARY)(em_cff4.o):	em_cff4.s
	$(CC1) em_cff4.s

$(LIBRARY)(em_cff8.o):	em_cff8.s
	$(CC1) em_cff8.s

$(LIBRARY)(em_cfi.o):	em_cfi.s
	$(CC1) em_cfi.s

$(LIBRARY)(em_cfu.o):	em_cfu.s
	$(CC1) em_cfu.s

$(LIBRARY)(em_cif4.o):	em_cif4.s
	$(CC1) em_cif4.s

$(LIBRARY)(em_cif8.o):	em_cif8.s
	$(CC1) em_cif8.s

$(LIBRARY)(em_cii.o):	em_cii.s
	$(CC1) em_cii.s

$(LIBRARY)(em_cmf4.o):	em_cmf4.s
	$(CC1) em_cmf4.s

$(LIBRARY)(em_cmf8.o):	em_cmf8.s
	$(CC1) em_cmf8.s

$(LIBRARY)(em_cms.o):	em_cms.s
	$(CC1) em_cms.s

$(LIBRARY)(em_com.o):	em_com.s
	$(CC1) em_com.s

$(LIBRARY)(em_csa4.o):	em_csa4.s
	$(CC1) em_csa4.s

$(LIBRARY)(em_csb4.o):	em_csb4.s
	$(CC1) em_csb4.s

$(LIBRARY)(em_cuf4.o):	em_cuf4.s
	$(CC1) em_cuf4.s

$(LIBRARY)(em_cuf8.o):	em_cuf8.s
	$(CC1) em_cuf8.s

$(LIBRARY)(em_cuu.o):	em_cuu.s
	$(CC1) em_cuu.s

$(LIBRARY)(em_dup.o):	em_dup.s
	$(CC1) em_dup.s

$(LIBRARY)(em_dvf4.o):	em_dvf4.s
	$(CC1) em_dvf4.s

$(LIBRARY)(em_dvf8.o):	em_dvf8.s
	$(CC1) em_dvf8.s

$(LIBRARY)(em_dvi.o):	em_dvi.s
	$(CC1) em_dvi.s

$(LIBRARY)(em_dvu.o):	em_dvu.s
	$(CC1) em_dvu.s

$(LIBRARY)(em_error.o):	em_error.s
	$(CC1) em_error.s

$(LIBRARY)(em_exg.o):	em_exg.s
	$(CC1) em_exg.s

$(LIBRARY)(em_fat.o):	em_fat.s
	$(CC1) em_fat.s

$(LIBRARY)(em_fef4.o):	em_fef4.s
	$(CC1) em_fef4.s

$(LIBRARY)(em_fef8.o):	em_fef8.s
	$(CC1) em_fef8.s

$(LIBRARY)(em_fif4.o):	em_fif4.s
	$(CC1) em_fif4.s

$(LIBRARY)(em_fif8.o):	em_fif8.s
	$(CC1) em_fif8.s

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

$(LIBRARY)(em_lar4.o):	em_lar4.s
	$(CC1) em_lar4.s

$(LIBRARY)(em_loi.o):	em_loi.s
	$(CC1) em_loi.s

$(LIBRARY)(em_mlf4.o):	em_mlf4.s
	$(CC1) em_mlf4.s

$(LIBRARY)(em_mlf8.o):	em_mlf8.s
	$(CC1) em_mlf8.s

$(LIBRARY)(em_mli.o):	em_mli.s
	$(CC1) em_mli.s

$(LIBRARY)(em_mon.o):	em_mon.s
	$(CC1) em_mon.s

$(LIBRARY)(em_ngf4.o):	em_ngf4.s
	$(CC1) em_ngf4.s

$(LIBRARY)(em_ngf8.o):	em_ngf8.s
	$(CC1) em_ngf8.s

$(LIBRARY)(em_ngi.o):	em_ngi.s
	$(CC1) em_ngi.s

$(LIBRARY)(em_nop.o):	em_nop.s
	$(CC1) em_nop.s

$(LIBRARY)(em_print.o):	em_print.s
	$(CC1) em_print.s

$(LIBRARY)(em_rck.o):	em_rck.s
	$(CC1) em_rck.s

$(LIBRARY)(em_rmi.o):	em_rmi.s
	$(CC1) em_rmi.s

$(LIBRARY)(em_rmu.o):	em_rmu.s
	$(CC1) em_rmu.s

$(LIBRARY)(em_rol.o):	em_rol.s
	$(CC1) em_rol.s

$(LIBRARY)(em_ror.o):	em_ror.s
	$(CC1) em_ror.s

$(LIBRARY)(em_sar4.o):	em_sar4.s
	$(CC1) em_sar4.s

$(LIBRARY)(em_sbf4.o):	em_sbf4.s
	$(CC1) em_sbf4.s

$(LIBRARY)(em_sbf8.o):	em_sbf8.s
	$(CC1) em_sbf8.s

$(LIBRARY)(em_sbi.o):	em_sbi.s
	$(CC1) em_sbi.s

$(LIBRARY)(em_set.o):	em_set.s
	$(CC1) em_set.s

$(LIBRARY)(em_sli.o):	em_sli.s
	$(CC1) em_sli.s

$(LIBRARY)(em_sri.o):	em_sri.s
	$(CC1) em_sri.s

$(LIBRARY)(em_sti.o):	em_sti.s
	$(CC1) em_sti.s

$(LIBRARY)(em_stop.o):	em_stop.s
	$(CC1) em_stop.s

$(LIBRARY)(em_trp.o):	em_trp.s
	$(CC1) em_trp.s

$(LIBRARY)(em_unknown.o):	em_unknown.s
	$(CC1) em_unknown.s

$(LIBRARY)(em_xor.o):	em_xor.s
	$(CC1) em_xor.s
