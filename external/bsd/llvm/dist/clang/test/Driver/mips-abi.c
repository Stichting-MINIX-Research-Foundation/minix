// Check passing Mips ABI options to the backend.
//
// RUN: %clang -target mips-linux-gnu -### -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-DEF %s
// MIPS-DEF: "-target-cpu" "mips32r2"
// MIPS-DEF: "-target-abi" "o32"
//
// RUN: %clang -target mips64-linux-gnu -### -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS64-DEF %s
// MIPS64-DEF: "-target-cpu" "mips64r2"
// MIPS64-DEF: "-target-abi" "n64"
//
// RUN: %clang -target mips-linux-gnu -### -c %s \
// RUN:        -mabi=32 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ABI-32 %s
// MIPS-ABI-32: "-target-cpu" "mips32r2"
// MIPS-ABI-32: "-target-abi" "o32"
//
// RUN: %clang -target mips-linux-gnu -### -c %s \
// RUN:        -mabi=o32 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ABI-O32 %s
// MIPS-ABI-O32: "-target-cpu" "mips32r2"
// MIPS-ABI-O32: "-target-abi" "o32"
//
// RUN: %clang -target mips-linux-gnu -### -c %s \
// RUN:        -mabi=n32 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ABI-N32 %s
// MIPS-ABI-N32: "-target-cpu" "mips64r2"
// MIPS-ABI-N32: "-target-abi" "n32"
//
// RUN: %clang -target mips64-linux-gnu -### -c %s \
// RUN:        -mabi=64 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ABI-64 %s
// MIPS-ABI-64: "-target-cpu" "mips64r2"
// MIPS-ABI-64: "-target-abi" "n64"
//
// RUN: %clang -target mips64-linux-gnu -### -c %s \
// RUN:        -mabi=n64 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ABI-N64 %s
// MIPS-ABI-N64: "-target-cpu" "mips64r2"
// MIPS-ABI-N64: "-target-abi" "n64"
//
// RUN: not %clang -target mips64-linux-gnu -c %s \
// RUN:        -mabi=o64 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ABI-O64 %s
// MIPS-ABI-O64: error: unknown target ABI 'o64'
//
// RUN: %clang -target mips-linux-gnu -### -c %s \
// RUN:        -mabi=eabi 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ABI-EABI %s
// MIPS-ABI-EABI: "-target-cpu" "mips32r2"
// MIPS-ABI-EABI: "-target-abi" "eabi"
//
// RUN: not %clang -target mips-linux-gnu -c %s \
// RUN:        -mabi=unknown 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ABI-UNKNOWN %s
// MIPS-ABI-UNKNOWN: error: unknown target ABI 'unknown'
//
// RUN: %clang -target mips-linux-gnu -### -c %s \
// RUN:        -march=mips1 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ARCH-1 %s
// MIPS-ARCH-1: "-target-cpu" "mips1"
// MIPS-ARCH-1: "-target-abi" "o32"
//
// RUN: %clang -target mips-linux-gnu -### -c %s \
// RUN:        -march=mips2 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ARCH-2 %s
// MIPS-ARCH-2: "-target-cpu" "mips2"
// MIPS-ARCH-2: "-target-abi" "o32"
//
// RUN: %clang -target mips-linux-gnu -### -c %s \
// RUN:        -march=mips3 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ARCH-3 %s
// MIPS-ARCH-3: "-target-cpu" "mips3"
// MIPS-ARCH-3: "-target-abi" "o32"
//
// RUN: %clang -target mips-linux-gnu -### -c %s \
// RUN:        -march=mips4 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ARCH-4 %s
// MIPS-ARCH-4: "-target-cpu" "mips4"
// MIPS-ARCH-4: "-target-abi" "o32"
//
// RUN: %clang -target mips-linux-gnu -### -c %s \
// RUN:        -march=mips5 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ARCH-5 %s
// MIPS-ARCH-5: "-target-cpu" "mips5"
// MIPS-ARCH-5: "-target-abi" "o32"
//
// RUN: %clang -target mips-linux-gnu -### -c %s \
// RUN:        -march=mips32 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ARCH-32 %s
// MIPS-ARCH-32: "-target-cpu" "mips32"
// MIPS-ARCH-32: "-target-abi" "o32"
//
// RUN: %clang -target mips-linux-gnu -### -c %s \
// RUN:        -march=mips32r2 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ARCH-32R2 %s
// MIPS-ARCH-32R2: "-target-cpu" "mips32r2"
// MIPS-ARCH-32R2: "-target-abi" "o32"
//
// RUN: %clang -target mips-linux-gnu -### -c %s \
// RUN:        -march=mips64 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ARCH-3264 %s
// MIPS-ARCH-3264: "-target-cpu" "mips64"
// MIPS-ARCH-3264: "-target-abi" "o32"
//
// RUN: %clang -target mips64-linux-gnu -### -c %s \
// RUN:        -march=mips64 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ARCH-64 %s
// MIPS-ARCH-64: "-target-cpu" "mips64"
// MIPS-ARCH-64: "-target-abi" "n64"
//
// RUN: %clang -target mips64-linux-gnu -### -c %s \
// RUN:        -march=mips64r2 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ARCH-64R2 %s
// MIPS-ARCH-64R2: "-target-cpu" "mips64r2"
// MIPS-ARCH-64R2: "-target-abi" "n64"
//
// RUN: %clang -target mips64-linux-gnu -### -c %s \
// RUN:        -march=octeon 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ARCH-OCTEON %s
// MIPS-ARCH-OCTEON: "-target-cpu" "octeon"
// MIPS-ARCH-OCTEON: "-target-abi" "n64"
//
// RUN: not %clang -target mips64-linux-gnu -c %s \
// RUN:        -march=mips32 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ARCH-6432 %s
// MIPS-ARCH-6432: error: unknown target CPU 'mips32'
//
// RUN: not %clang -target mips-linux-gnu -c %s \
// RUN:        -march=unknown 2>&1 \
// RUN:   | FileCheck -check-prefix=MIPS-ARCH-UNKNOWN %s
// MIPS-ARCH-UNKNOWN: error: unknown target CPU 'unknown'
