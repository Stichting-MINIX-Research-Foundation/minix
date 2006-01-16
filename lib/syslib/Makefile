#Generated from ./syslib/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../generate.sh ./syslib ../obj-ack ../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../obj-ack/libsys.a

../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_attr_r16.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_attr_r32.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_attr_r8.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_attr_w16.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_attr_w32.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_attr_w8.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_dev_name.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_find_dev.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_first_dev.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_ids.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_init.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_init1.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_next_dev.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_rescan_bus.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_reserve.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(pci_slot_name.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_abort.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_eniop.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_endsig.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_exec.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_exit.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_fork.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_getinfo.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_getsig.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_in.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_int86.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_irqctl.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_kill.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_memset.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_newmap.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_nice.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_out.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_physcopy.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_sdevio.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_segctl.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_setalarm.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_sigreturn.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_sigsend.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_svrctl.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_times.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_trace.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_umap.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_vinb.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_vinl.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_vinw.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_vircopy.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_vm_map.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_vm_setbuf.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_voutb.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_voutl.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(sys_voutw.o)
../obj-ack/libsys.a: ../obj-ack/libsys.a(taskcall.o)

../obj-ack/libsys.a:
	ar cr ../obj-ack/libsys.a ../obj-ack/./syslib/*.o
	rm ../obj-ack/./syslib/*.o

../obj-ack/libsys.a(pci_attr_r16.o): pci_attr_r16.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_attr_r16.o pci_attr_r16.c
../obj-ack/libsys.a(pci_attr_r32.o): pci_attr_r32.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_attr_r32.o pci_attr_r32.c
../obj-ack/libsys.a(pci_attr_r8.o): pci_attr_r8.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_attr_r8.o pci_attr_r8.c
../obj-ack/libsys.a(pci_attr_w16.o): pci_attr_w16.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_attr_w16.o pci_attr_w16.c
../obj-ack/libsys.a(pci_attr_w32.o): pci_attr_w32.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_attr_w32.o pci_attr_w32.c
../obj-ack/libsys.a(pci_attr_w8.o): pci_attr_w8.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_attr_w8.o pci_attr_w8.c
../obj-ack/libsys.a(pci_dev_name.o): pci_dev_name.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_dev_name.o pci_dev_name.c
../obj-ack/libsys.a(pci_find_dev.o): pci_find_dev.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_find_dev.o pci_find_dev.c
../obj-ack/libsys.a(pci_first_dev.o): pci_first_dev.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_first_dev.o pci_first_dev.c
../obj-ack/libsys.a(pci_ids.o): pci_ids.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_ids.o pci_ids.c
../obj-ack/libsys.a(pci_init.o): pci_init.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_init.o pci_init.c
../obj-ack/libsys.a(pci_init1.o): pci_init1.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_init1.o pci_init1.c
../obj-ack/libsys.a(pci_next_dev.o): pci_next_dev.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_next_dev.o pci_next_dev.c
../obj-ack/libsys.a(pci_rescan_bus.o): pci_rescan_bus.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_rescan_bus.o pci_rescan_bus.c
../obj-ack/libsys.a(pci_reserve.o): pci_reserve.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_reserve.o pci_reserve.c
../obj-ack/libsys.a(pci_slot_name.o): pci_slot_name.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/pci_slot_name.o pci_slot_name.c
../obj-ack/libsys.a(sys_abort.o): sys_abort.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_abort.o sys_abort.c
../obj-ack/libsys.a(sys_eniop.o): sys_eniop.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_eniop.o sys_eniop.c
../obj-ack/libsys.a(sys_endsig.o): sys_endsig.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_endsig.o sys_endsig.c
../obj-ack/libsys.a(sys_exec.o): sys_exec.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_exec.o sys_exec.c
../obj-ack/libsys.a(sys_exit.o): sys_exit.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_exit.o sys_exit.c
../obj-ack/libsys.a(sys_fork.o): sys_fork.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_fork.o sys_fork.c
../obj-ack/libsys.a(sys_getinfo.o): sys_getinfo.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_getinfo.o sys_getinfo.c
../obj-ack/libsys.a(sys_getsig.o): sys_getsig.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_getsig.o sys_getsig.c
../obj-ack/libsys.a(sys_in.o): sys_in.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_in.o sys_in.c
../obj-ack/libsys.a(sys_int86.o): sys_int86.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_int86.o sys_int86.c
../obj-ack/libsys.a(sys_irqctl.o): sys_irqctl.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_irqctl.o sys_irqctl.c
../obj-ack/libsys.a(sys_kill.o): sys_kill.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_kill.o sys_kill.c
../obj-ack/libsys.a(sys_memset.o): sys_memset.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_memset.o sys_memset.c
../obj-ack/libsys.a(sys_newmap.o): sys_newmap.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_newmap.o sys_newmap.c
../obj-ack/libsys.a(sys_nice.o): sys_nice.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_nice.o sys_nice.c
../obj-ack/libsys.a(sys_out.o): sys_out.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_out.o sys_out.c
../obj-ack/libsys.a(sys_physcopy.o): sys_physcopy.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_physcopy.o sys_physcopy.c
../obj-ack/libsys.a(sys_sdevio.o): sys_sdevio.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_sdevio.o sys_sdevio.c
../obj-ack/libsys.a(sys_segctl.o): sys_segctl.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_segctl.o sys_segctl.c
../obj-ack/libsys.a(sys_setalarm.o): sys_setalarm.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_setalarm.o sys_setalarm.c
../obj-ack/libsys.a(sys_sigreturn.o): sys_sigreturn.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_sigreturn.o sys_sigreturn.c
../obj-ack/libsys.a(sys_sigsend.o): sys_sigsend.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_sigsend.o sys_sigsend.c
../obj-ack/libsys.a(sys_svrctl.o): sys_svrctl.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_svrctl.o sys_svrctl.c
../obj-ack/libsys.a(sys_times.o): sys_times.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_times.o sys_times.c
../obj-ack/libsys.a(sys_trace.o): sys_trace.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_trace.o sys_trace.c
../obj-ack/libsys.a(sys_umap.o): sys_umap.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_umap.o sys_umap.c
../obj-ack/libsys.a(sys_vinb.o): sys_vinb.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_vinb.o sys_vinb.c
../obj-ack/libsys.a(sys_vinl.o): sys_vinl.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_vinl.o sys_vinl.c
../obj-ack/libsys.a(sys_vinw.o): sys_vinw.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_vinw.o sys_vinw.c
../obj-ack/libsys.a(sys_vircopy.o): sys_vircopy.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_vircopy.o sys_vircopy.c
../obj-ack/libsys.a(sys_vm_map.o): sys_vm_map.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_vm_map.o sys_vm_map.c
../obj-ack/libsys.a(sys_vm_setbuf.o): sys_vm_setbuf.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_vm_setbuf.o sys_vm_setbuf.c
../obj-ack/libsys.a(sys_voutb.o): sys_voutb.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_voutb.o sys_voutb.c
../obj-ack/libsys.a(sys_voutl.o): sys_voutl.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_voutl.o sys_voutl.c
../obj-ack/libsys.a(sys_voutw.o): sys_voutw.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/sys_voutw.o sys_voutw.c
../obj-ack/libsys.a(taskcall.o): taskcall.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./syslib/taskcall.o taskcall.c

all-gnu: ../obj-gnu/libsys.a

../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_attr_r16.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_attr_r32.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_attr_r8.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_attr_w16.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_attr_w32.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_attr_w8.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_dev_name.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_find_dev.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_first_dev.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_ids.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_init.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_init1.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_next_dev.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_rescan_bus.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_reserve.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/pci_slot_name.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_abort.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_eniop.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_endsig.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_exec.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_exit.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_fork.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_getinfo.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_getsig.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_in.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_int86.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_irqctl.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_kill.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_memset.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_newmap.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_nice.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_out.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_physcopy.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_sdevio.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_segctl.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_setalarm.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_sigreturn.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_sigsend.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_svrctl.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_times.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_trace.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_umap.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_vinb.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_vinl.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_vinw.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_vircopy.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_vm_map.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_vm_setbuf.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_voutb.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_voutl.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/sys_voutw.o
../obj-gnu/libsys.a: ../obj-gnu/./syslib/taskcall.o

../obj-gnu/libsys.a:
	gar cr ../obj-gnu/libsys.a $?

../obj-gnu/./syslib/pci_attr_r16.o: pci_attr_r16.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_attr_r16.o pci_attr_r16.c

../obj-gnu/./syslib/pci_attr_r32.o: pci_attr_r32.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_attr_r32.o pci_attr_r32.c

../obj-gnu/./syslib/pci_attr_r8.o: pci_attr_r8.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_attr_r8.o pci_attr_r8.c

../obj-gnu/./syslib/pci_attr_w16.o: pci_attr_w16.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_attr_w16.o pci_attr_w16.c

../obj-gnu/./syslib/pci_attr_w32.o: pci_attr_w32.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_attr_w32.o pci_attr_w32.c

../obj-gnu/./syslib/pci_attr_w8.o: pci_attr_w8.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_attr_w8.o pci_attr_w8.c

../obj-gnu/./syslib/pci_dev_name.o: pci_dev_name.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_dev_name.o pci_dev_name.c

../obj-gnu/./syslib/pci_find_dev.o: pci_find_dev.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_find_dev.o pci_find_dev.c

../obj-gnu/./syslib/pci_first_dev.o: pci_first_dev.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_first_dev.o pci_first_dev.c

../obj-gnu/./syslib/pci_ids.o: pci_ids.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_ids.o pci_ids.c

../obj-gnu/./syslib/pci_init.o: pci_init.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_init.o pci_init.c

../obj-gnu/./syslib/pci_init1.o: pci_init1.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_init1.o pci_init1.c

../obj-gnu/./syslib/pci_next_dev.o: pci_next_dev.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_next_dev.o pci_next_dev.c

../obj-gnu/./syslib/pci_rescan_bus.o: pci_rescan_bus.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_rescan_bus.o pci_rescan_bus.c

../obj-gnu/./syslib/pci_reserve.o: pci_reserve.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_reserve.o pci_reserve.c

../obj-gnu/./syslib/pci_slot_name.o: pci_slot_name.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/pci_slot_name.o pci_slot_name.c

../obj-gnu/./syslib/sys_abort.o: sys_abort.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_abort.o sys_abort.c

../obj-gnu/./syslib/sys_eniop.o: sys_eniop.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_eniop.o sys_eniop.c

../obj-gnu/./syslib/sys_endsig.o: sys_endsig.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_endsig.o sys_endsig.c

../obj-gnu/./syslib/sys_exec.o: sys_exec.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_exec.o sys_exec.c

../obj-gnu/./syslib/sys_exit.o: sys_exit.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_exit.o sys_exit.c

../obj-gnu/./syslib/sys_fork.o: sys_fork.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_fork.o sys_fork.c

../obj-gnu/./syslib/sys_getinfo.o: sys_getinfo.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_getinfo.o sys_getinfo.c

../obj-gnu/./syslib/sys_getsig.o: sys_getsig.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_getsig.o sys_getsig.c

../obj-gnu/./syslib/sys_in.o: sys_in.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_in.o sys_in.c

../obj-gnu/./syslib/sys_int86.o: sys_int86.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_int86.o sys_int86.c

../obj-gnu/./syslib/sys_irqctl.o: sys_irqctl.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_irqctl.o sys_irqctl.c

../obj-gnu/./syslib/sys_kill.o: sys_kill.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_kill.o sys_kill.c

../obj-gnu/./syslib/sys_memset.o: sys_memset.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_memset.o sys_memset.c

../obj-gnu/./syslib/sys_newmap.o: sys_newmap.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_newmap.o sys_newmap.c

../obj-gnu/./syslib/sys_nice.o: sys_nice.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_nice.o sys_nice.c

../obj-gnu/./syslib/sys_out.o: sys_out.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_out.o sys_out.c

../obj-gnu/./syslib/sys_physcopy.o: sys_physcopy.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_physcopy.o sys_physcopy.c

../obj-gnu/./syslib/sys_sdevio.o: sys_sdevio.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_sdevio.o sys_sdevio.c

../obj-gnu/./syslib/sys_segctl.o: sys_segctl.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_segctl.o sys_segctl.c

../obj-gnu/./syslib/sys_setalarm.o: sys_setalarm.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_setalarm.o sys_setalarm.c

../obj-gnu/./syslib/sys_sigreturn.o: sys_sigreturn.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_sigreturn.o sys_sigreturn.c

../obj-gnu/./syslib/sys_sigsend.o: sys_sigsend.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_sigsend.o sys_sigsend.c

../obj-gnu/./syslib/sys_svrctl.o: sys_svrctl.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_svrctl.o sys_svrctl.c

../obj-gnu/./syslib/sys_times.o: sys_times.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_times.o sys_times.c

../obj-gnu/./syslib/sys_trace.o: sys_trace.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_trace.o sys_trace.c

../obj-gnu/./syslib/sys_umap.o: sys_umap.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_umap.o sys_umap.c

../obj-gnu/./syslib/sys_vinb.o: sys_vinb.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_vinb.o sys_vinb.c

../obj-gnu/./syslib/sys_vinl.o: sys_vinl.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_vinl.o sys_vinl.c

../obj-gnu/./syslib/sys_vinw.o: sys_vinw.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_vinw.o sys_vinw.c

../obj-gnu/./syslib/sys_vircopy.o: sys_vircopy.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_vircopy.o sys_vircopy.c

../obj-gnu/./syslib/sys_vm_map.o: sys_vm_map.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_vm_map.o sys_vm_map.c

../obj-gnu/./syslib/sys_vm_setbuf.o: sys_vm_setbuf.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_vm_setbuf.o sys_vm_setbuf.c

../obj-gnu/./syslib/sys_voutb.o: sys_voutb.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_voutb.o sys_voutb.c

../obj-gnu/./syslib/sys_voutl.o: sys_voutl.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_voutl.o sys_voutl.c

../obj-gnu/./syslib/sys_voutw.o: sys_voutw.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/sys_voutw.o sys_voutw.c

../obj-gnu/./syslib/taskcall.o: taskcall.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./syslib/taskcall.o taskcall.c




clean::
	rm -f ../obj-ack/./syslib/*
	rm -f ../obj-gnu/./syslib/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
