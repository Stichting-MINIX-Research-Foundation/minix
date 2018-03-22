PROGS+= test_arm_segfault
PROGS+= test_arm_unaligned

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}

test_arm_segfault.o : test_arm_segfault.S
test_arm_unaligned.o : test_arm_unaligned.S
