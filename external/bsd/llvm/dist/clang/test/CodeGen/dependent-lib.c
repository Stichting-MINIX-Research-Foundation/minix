// RUN: %clang_cc1 %s --dependent-lib=msvcrt -triple i686-pc-win32 -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 %s --dependent-lib=msvcrt -triple x86_64-pc-win32 -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 %s --dependent-lib=msvcrt -triple i686-pc-linux -emit-llvm -o - | FileCheck -check-prefix LINUX %s

// CHECK: !llvm.module.flags = !{{{.*}}}
// CHECK: !{{[0-9]+}} = !{i32 6, !"Linker Options", ![[link_opts:[0-9]+]]}
// CHECK: ![[link_opts]] = !{![[msvcrt:[0-9]+]]}
// CHECK: ![[msvcrt]] = !{!"/DEFAULTLIB:msvcrt.lib"}

// LINUX: !llvm.module.flags = !{{{.*}}}
// LINUX: !{{[0-9]+}} = !{i32 6, !"Linker Options", ![[link_opts:[0-9]+]]}
// LINUX: ![[link_opts]] = !{![[msvcrt:[0-9]+]]}
// LINUX: ![[msvcrt]] = !{!"-lmsvcrt"}

int f();
