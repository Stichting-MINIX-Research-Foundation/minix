// Check that we error when -faltivec is specified on non-ppc platforms.

// RUN: %clang -target powerpc-unk-unk -faltivec -fsyntax-only %s
// RUN: %clang -target powerpc64-linux-gnu -faltivec -fsyntax-only %s
// RUN: %clang -target powerpc64-linux-gnu -maltivec -fsyntax-only %s

// RUN: not %clang -target i386-pc-win32 -faltivec -fsyntax-only %s 2>&1 | FileCheck %s
// RUN: not %clang -target x86_64-unknown-freebsd -faltivec -fsyntax-only %s 2>&1 | FileCheck %s
// RUN: not %clang -target armv6-apple-darwin -faltivec -fsyntax-only %s 2>&1 | FileCheck %s
// RUN: not %clang -target armv7-apple-darwin -faltivec -fsyntax-only %s 2>&1 | FileCheck %s
// RUN: not %clang -target mips-linux-gnu -faltivec -fsyntax-only %s 2>&1 | FileCheck %s
// RUN: not %clang -target mips64-linux-gnu -faltivec -fsyntax-only %s 2>&1 | FileCheck %s
// RUN: not %clang -target sparc-unknown-solaris -faltivec -fsyntax-only %s 2>&1 | FileCheck %s

// CHECK: invalid argument '-faltivec' only allowed with 'ppc/ppc64/ppc64le'

// Check that -fno-altivec and -mno-altivec correctly disable the altivec
// target feature on powerpc.

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -fno-altivec -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-1 %s
// CHECK-1: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-altivec -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-2 %s
// CHECK-2: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -faltivec -mno-altivec -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-3 %s
// CHECK-3: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -maltivec -fno-altivec -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-4 %s
// CHECK-4: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-altivec -faltivec -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-5 %s
// CHECK-5-NOT: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -fno-altivec -maltivec -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-6 %s
// CHECK-6-NOT: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -fno-altivec -mcpu=7400 -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-7 %s
// CHECK-7: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -fno-altivec -mcpu=g4 -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-8 %s
// CHECK-8: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -fno-altivec -mcpu=7450 -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-9 %s
// CHECK-9: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -fno-altivec -mcpu=g4+ -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-10 %s
// CHECK-10: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -fno-altivec -mcpu=970 -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-11 %s
// CHECK-11: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -fno-altivec -mcpu=g5 -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-12 %s
// CHECK-12: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -fno-altivec -mcpu=pwr6 -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-13 %s
// CHECK-13: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -fno-altivec -mcpu=pwr7 -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-14 %s
// CHECK-14: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -fno-altivec -mcpu=pwr8 -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-15 %s
// CHECK-15: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -fno-altivec -mcpu=ppc64 -### -o %t.o 2>&1 | FileCheck --check-prefix=CHECK-16 %s
// CHECK-16: "-target-feature" "-altivec"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-qpx -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-NOQPX %s
// CHECK-NOQPX: "-target-feature" "-qpx"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-qpx -mqpx -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-QPX %s
// CHECK-QPX-NOT: "-target-feature" "-qpx"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-mfcrf -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-NOMFCRF %s
// CHECK-NOMFCRF: "-target-feature" "-mfocrf"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-mfcrf -mmfcrf -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-MFCRF %s
// CHECK-MFCRF: "-target-feature" "+mfocrf"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-popcntd -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-NOPOPCNTD %s
// CHECK-NOPOPCNTD: "-target-feature" "-popcntd"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-popcntd -mpopcntd -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-POPCNTD %s
// CHECK-POPCNTD: "-target-feature" "+popcntd"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-fprnd -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-NOFPRND %s
// CHECK-NOFPRND: "-target-feature" "-fprnd"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-fprnd -mfprnd -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-FPRND %s
// CHECK-FPRND: "-target-feature" "+fprnd"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-cmpb -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-NOCMPB %s
// CHECK-NOCMPB: "-target-feature" "-cmpb"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-cmpb -mcmpb -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-CMPB %s
// CHECK-CMPB: "-target-feature" "+cmpb"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-vsx -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-NOVSX %s
// CHECK-NOVSX: "-target-feature" "-vsx"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-vsx -mvsx -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-VSX %s
// CHECK-VSX: "-target-feature" "+vsx"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-power8-vector -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-NOP8VECTOR %s
// CHECK-NOP8VECTOR: "-target-feature" "-power8-vector"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-power8-vector -mpower8-vector -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-P8VECTOR %s
// CHECK-P8VECTOR: "-target-feature" "+power8-vector"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-crbits -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-NOCRBITS %s
// CHECK-NOCRBITS: "-target-feature" "-crbits"

// RUN: %clang -target powerpc64-unknown-linux-gnu %s -mno-crbits -mcrbits -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK-CRBITS %s
// CHECK-CRBITS: "-target-feature" "+crbits"

// Assembler features
// RUN: %clang -target powerpc64-unknown-linux-gnu %s -### -o %t.o -no-integrated-as 2>&1 | FileCheck -check-prefix=CHECK_BE_AS_ARGS %s
// CHECK_BE_AS_ARGS: "-mppc64"
// CHECK_BE_AS_ARGS: "-many"

// RUN: %clang -target powerpc64le-unknown-linux-gnu %s -### -o %t.o -no-integrated-as 2>&1 | FileCheck -check-prefix=CHECK_LE_AS_ARGS %s
// CHECK_LE_AS_ARGS: "-mppc64"
// CHECK_LE_AS_ARGS: "-many"
// CHECK_LE_AS_ARGS: "-mlittle-endian"

// linker features
// RUN: %clang -target powerpc64-unknown-linux-gnu %s -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK_BE_LD_ARGS %s
// CHECK_BE_LD_ARGS: "elf64ppc"

// RUN: %clang -target powerpc64le-unknown-linux-gnu %s -### -o %t.o 2>&1 | FileCheck -check-prefix=CHECK_LE_LD_ARGS %s
// CHECK_LE_LD_ARGS: "elf64lppc"


