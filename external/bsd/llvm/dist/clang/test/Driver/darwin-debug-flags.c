// RUN: env RC_DEBUG_OPTIONS=1 %clang -target i386-apple-darwin9 -I "path with \spaces" -g -Os %s  -emit-llvm -S -o - | FileCheck %s
// <rdar://problem/7256886>
// RUN: touch %t.s
// RUN: env RC_DEBUG_OPTIONS=1 %clang -### -target i386-apple-darwin9 -c -g %t.s 2>&1 | FileCheck -check-prefix=S %s
// <rdar://problem/12955296>
// RUN: %clang -### -target i386-apple-darwin9 -c -g %t.s 2>&1 | FileCheck -check-prefix=P %s

// CHECK: !0 = !{
// CHECK: -I path\5C with\5C \5C\5Cspaces
// CHECK: -g -Os
// CHECK: -mmacosx-version-min=10.5.0
// CHECK: [ DW_TAG_compile_unit ]

int x;

// S: "-dwarf-debug-flags"

// P: "-dwarf-debug-producer"

// This depends on shell quoting.
// REQUIRES: shell
