// RUN: %clang -### -c -integrated-as %s 2>&1 | FileCheck %s
// CHECK: cc1as
// CHECK-NOT: -relax-all

// RUN: %clang -### -c -integrated-as -Wa,-L %s 2>&1 | FileCheck --check-prefix=OPT_L %s
// OPT_L: msave-temp-labels

// RUN: %clang -### -target x86_64-linux-gnu -c -integrated-as %s -fsanitize=address 2>&1 %s | FileCheck --check-prefix=SANITIZE %s
// SANITIZE: argument unused during compilation: '-fsanitize=address'

// Test that -I params in -Wa, and -Xassembler args are passed to integrated assembler
// RUN: %clang -### -c -integrated-as %s -Wa,-I,foo_dir 2>&1 | FileCheck --check-prefix=WA_INCLUDE1 %s
// WA_INCLUDE1: cc1as
// WA_INCLUDE1: "-I" "foo_dir"

// RUN: %clang -### -c -integrated-as %s -Wa,-Ifoo_dir 2>&1 | FileCheck --check-prefix=WA_INCLUDE2 %s
// WA_INCLUDE2: cc1as
// WA_INCLUDE2: "-Ifoo_dir"

// RUN: %clang -### -c -integrated-as %s -Wa,-I -Wa,foo_dir 2>&1 | FileCheck --check-prefix=WA_INCLUDE3 %s
// WA_INCLUDE3: cc1as
// WA_INCLUDE3: "-I" "foo_dir"

// RUN: %clang -### -c -integrated-as %s -Xassembler -I -Xassembler foo_dir 2>&1 | FileCheck --check-prefix=XA_INCLUDE1 %s
// XA_INCLUDE1: cc1as
// XA_INCLUDE1: "-I" "foo_dir"

// RUN: %clang -### -c -integrated-as %s -Xassembler -Ifoo_dir 2>&1 | FileCheck --check-prefix=XA_INCLUDE2 %s
// XA_INCLUDE2: cc1as
// XA_INCLUDE2: "-Ifoo_dir"

// RUN: %clang -### -c -integrated-as %s -gdwarf-2 2>&1 | FileCheck --check-prefix=DWARF2 %s
// DWARF2: "-g" "-gdwarf-2"

// RUN: %clang -### -c -integrated-as %s -gdwarf-3 2>&1 | FileCheck --check-prefix=DWARF3 %s
// DWARF3: "-g" "-gdwarf-3"

// RUN: %clang -### -c -integrated-as %s -gdwarf-4 2>&1 | FileCheck --check-prefix=DWARF4 %s
// DWARF4: "-g" "-gdwarf-4"

// RUN: %clang -### -c -integrated-as %s -Xassembler -gdwarf-2 2>&1 | FileCheck --check-prefix=DWARF2XASSEMBLER %s
// DWARF2XASSEMBLER: "-gdwarf-2"

// RUN: %clang -### -c -integrated-as %s -Wa,-gdwarf-2 2>&1 | FileCheck --check-prefix=DWARF2WA %s
// DWARF2WA: "-gdwarf-2"
