// RUN: rm -rf %t
// RUN: mkdir %t

// RUN: not env FORCE_CLANG_DIAGNOSTICS_CRASH= TMPDIR=%t TEMP=%t TMP=%t \
// RUN: %clang -fsyntax-only %s -I %S/Inputs/module                     \
// RUN: -fmodules -fmodules-cache-path=/tmp/ -DFOO=BAR 2>&1 | FileCheck %s

// RUN: FileCheck --check-prefix=CHECKSRC %s -input-file %t/crash-report-*.m
// RUN: FileCheck --check-prefix=CHECKSH %s -input-file %t/crash-report-*.sh
// REQUIRES: crash-recovery

// because of the glob (*.m, *.sh)
// REQUIRES: shell

// FIXME: This XFAIL is cargo-culted from crash-report.c. Do we need it?
// XFAIL: mingw32

@import simple;
const int x = MODULE_MACRO;

// CHECK: Preprocessed source(s) and associated run script(s) are located at:
// CHECK-NEXT: note: diagnostic msg: {{.*}}.m
// CHECK-NEXT: note: diagnostic msg: {{.*}}.cache

// CHECKSRC: @import simple;
// CHECKSRC: const int x = 10;

// CHECKSH: "-cc1"
// CHECKSH: "-D" "FOO=BAR"
// CHECKSH-NOT: "-fmodules-cache-path=/tmp/"
// CHECKSH: "crash-report-modules-{{[^ ]*}}.m"
// CHECKSH: "-ivfsoverlay" "crash-report-modules-{{[^ ]*}}.cache/vfs/vfs.yaml"
