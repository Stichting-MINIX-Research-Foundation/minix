// RUN: %clang     -target i386-unknown-unknown -fsanitize=address %s -S -emit-llvm -o - | FileCheck %s
// RUN: %clang -O1 -target i386-unknown-unknown -fsanitize=address %s -S -emit-llvm -o - | FileCheck %s
// RUN: %clang -O2 -target i386-unknown-unknown -fsanitize=address %s -S -emit-llvm -o - | FileCheck %s
// RUN: %clang -O3 -target i386-unknown-unknown -fsanitize=address %s -S -emit-llvm -o - | FileCheck %s
// Verify that -fsanitize=address invokes asan instrumentation.

int foo(int *a) { return *a; }
// CHECK: __asan_init
