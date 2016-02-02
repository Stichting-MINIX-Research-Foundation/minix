// Test sanitizers ld flags.

// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target i386-unknown-linux -fsanitize=address \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-ASAN-LINUX %s
//
// CHECK-ASAN-LINUX: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-ASAN-LINUX-NOT: "-lc"
// CHECK-ASAN-LINUX: libclang_rt.asan-i386.a"
// CHECK-ASAN-LINUX-NOT: "-export-dynamic"
// CHECK-ASAN-LINUX: "--dynamic-list={{.*}}libclang_rt.asan-i386.a.syms"
// CHECK-ASAN-LINUX-NOT: "-export-dynamic"
// CHECK-ASAN-LINUX: "-lpthread"
// CHECK-ASAN-LINUX: "-lrt"
// CHECK-ASAN-LINUX: "-ldl"

// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target i386-unknown-linux -fsanitize=address -shared-libasan \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-SHARED-ASAN-LINUX %s
//
// CHECK-SHARED-ASAN-LINUX: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-SHARED-ASAN-LINUX-NOT: "-lc"
// CHECK-SHARED-ASAN-LINUX-NOT: libclang_rt.asan-i386.a"
// CHECK-SHARED-ASAN-LINUX: libclang_rt.asan-i386.so"
// CHECK-SHARED-ASAN-LINUX: "-whole-archive" "{{.*}}libclang_rt.asan-preinit-i386.a" "-no-whole-archive"
// CHECK-SHARED-ASAN-LINUX-NOT: "-lpthread"
// CHECK-SHARED-ASAN-LINUX-NOT: "-lrt"
// CHECK-SHARED-ASAN-LINUX-NOT: "-ldl"
// CHECK-SHARED-ASAN-LINUX-NOT: "-export-dynamic"
// CHECK-SHARED-ASAN-LINUX-NOT: "--dynamic-list"

// RUN: %clang -no-canonical-prefixes %s -### -o %t.so -shared 2>&1 \
// RUN:     -target i386-unknown-linux -fsanitize=address -shared-libasan \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-DSO-SHARED-ASAN-LINUX %s
//
// CHECK-DSO-SHARED-ASAN-LINUX: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-DSO-SHARED-ASAN-LINUX-NOT: "-lc"
// CHECK-DSO-SHARED-ASAN-LINUX-NOT: libclang_rt.asan-i386.a"
// CHECK-DSO-SHARED-ASAN-LINUX-NOT: "libclang_rt.asan-preinit-i386.a"
// CHECK-DSO-SHARED-ASAN-LINUX: libclang_rt.asan-i386.so"
// CHECK-DSO-SHARED-ASAN-LINUX-NOT: "-lpthread"
// CHECK-DSO-SHARED-ASAN-LINUX-NOT: "-lrt"
// CHECK-DSO-SHARED-ASAN-LINUX-NOT: "-ldl"
// CHECK-DSO-SHARED-ASAN-LINUX-NOT: "-export-dynamic"
// CHECK-DSO-SHARED-ASAN-LINUX-NOT: "--dynamic-list"

// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target i386-unknown-freebsd -fsanitize=address \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_freebsd_tree \
// RUN:   | FileCheck --check-prefix=CHECK-ASAN-FREEBSD %s
//
// CHECK-ASAN-FREEBSD: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-ASAN-FREEBSD-NOT: "-lc"
// CHECK-ASAN-FREEBSD-NOT: libclang_rt.asan_cxx
// CHECK-ASAN-FREEBSD: freebsd{{/|\\+}}libclang_rt.asan-i386.a"
// CHECK-ASAN-FREEBSD-NOT: libclang_rt.asan_cxx
// CHECK-ASAN-FREEBSD-NOT: "--dynamic-list"
// CHECK-ASAN-FREEBSD: "-export-dynamic"
// CHECK-ASAN-FREEBSD: "-lpthread"
// CHECK-ASAN-FREEBSD: "-lrt"

// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target i386-unknown-freebsd -fsanitize=address \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_freebsd_tree \
// RUN:   | FileCheck --check-prefix=CHECK-ASAN-FREEBSD-LDL %s
//
// CHECK-ASAN-FREEBSD-LDL: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-ASAN-FREEBSD-LDL-NOT: "-ldl"

// RUN: %clangxx -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target i386-unknown-linux -fsanitize=address \
// RUN:     -resource-dir=%S/Inputs/empty_resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-ASAN-LINUX-CXX %s
//
// CHECK-ASAN-LINUX-CXX: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-ASAN-LINUX-CXX-NOT: "-lc"
// CHECK-ASAN-LINUX-CXX: "-whole-archive" "{{.*}}libclang_rt.asan-i386.a" "-no-whole-archive"
// CHECK-ASAN-LINUX-CXX: "-whole-archive" "{{.*}}libclang_rt.asan_cxx-i386.a" "-no-whole-archive"
// CHECK-ASAN-LINUX-CXX-NOT: "--dynamic-list"
// CHECK-ASAN-LINUX-CXX: "-export-dynamic"
// CHECK-ASAN-LINUX-CXX: stdc++
// CHECK-ASAN-LINUX-CXX: "-lpthread"
// CHECK-ASAN-LINUX-CXX: "-lrt"
// CHECK-ASAN-LINUX-CXX: "-ldl"

// RUN: %clang -no-canonical-prefixes %s -### -o /dev/null -fsanitize=address \
// RUN:     -target i386-unknown-linux --sysroot=%S/Inputs/basic_linux_tree \
// RUN:     -lstdc++ -static 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-ASAN-LINUX-CXX-STATIC %s
//
// CHECK-ASAN-LINUX-CXX-STATIC: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-ASAN-LINUX-CXX-STATIC-NOT: stdc++
// CHECK-ASAN-LINUX-CXX-STATIC: "-whole-archive" "{{.*}}libclang_rt.asan-i386.a" "-no-whole-archive"
// CHECK-ASAN-LINUX-CXX-STATIC: stdc++

// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target arm-linux-gnueabi -fsanitize=address \
// RUN:     --sysroot=%S/Inputs/basic_android_tree/sysroot \
// RUN:   | FileCheck --check-prefix=CHECK-ASAN-ARM %s
//
// CHECK-ASAN-ARM: "{{(.*[^.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-ASAN-ARM-NOT: "-lc"
// CHECK-ASAN-ARM: libclang_rt.asan-arm.a"
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target armv7l-linux-gnueabi -fsanitize=address \
// RUN:     --sysroot=%S/Inputs/basic_android_tree/sysroot \
// RUN:   | FileCheck --check-prefix=CHECK-ASAN-ARMv7 %s
//
// CHECK-ASAN-ARMv7: "{{(.*[^.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-ASAN-ARMv7-NOT: "-lc"
// CHECK-ASAN-ARMv7: libclang_rt.asan-arm.a"

// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target arm-linux-androideabi -fsanitize=address \
// RUN:     --sysroot=%S/Inputs/basic_android_tree/sysroot \
// RUN:   | FileCheck --check-prefix=CHECK-ASAN-ANDROID %s
//
// CHECK-ASAN-ANDROID: "{{(.*[^.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-ASAN-ANDROID-NOT: "-lc"
// CHECK-ASAN-ANDROID: "-pie"
// CHECK-ASAN-ANDROID-NOT: "-lpthread"
// CHECK-ASAN-ANDROID: libclang_rt.asan-arm-android.so"
// CHECK-ASAN-ANDROID-NOT: "-lpthread"
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target arm-linux-androideabi -fsanitize=address \
// RUN:     --sysroot=%S/Inputs/basic_android_tree/sysroot \
// RUN:     -shared-libasan \
// RUN:   | FileCheck --check-prefix=CHECK-ASAN-ANDROID-SHARED-LIBASAN %s
//
// CHECK-ASAN-ANDROID-SHARED-LIBASAN-NOT: argument unused during compilation: '-shared-libasan'
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target arm-linux-androideabi -fsanitize=address \
// RUN:     --sysroot=%S/Inputs/basic_android_tree/sysroot \
// RUN:     -shared \
// RUN:   | FileCheck --check-prefix=CHECK-ASAN-ANDROID-SHARED %s
//
// CHECK-ASAN-ANDROID-SHARED: "{{(.*[^.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-ASAN-ANDROID-SHARED-NOT: "-lc"
// CHECK-ASAN-ANDROID-SHARED: libclang_rt.asan-arm-android.so"
// CHECK-ASAN-ANDROID-SHARED-NOT: "-lpthread"

// RUN: %clangxx -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target x86_64-unknown-linux -lstdc++ -fsanitize=thread \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-TSAN-LINUX-CXX %s
//
// CHECK-TSAN-LINUX-CXX: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-TSAN-LINUX-CXX-NOT: stdc++
// CHECK-TSAN-LINUX-CXX: "-whole-archive" "{{.*}}libclang_rt.tsan-x86_64.a" "-no-whole-archive"
// CHECK-TSAN-LINUX-CXX-NOT: "-export-dynamic"
// CHECK-TSAN-LINUX-CXX: "--dynamic-list={{.*}}libclang_rt.tsan-x86_64.a.syms"
// CHECK-TSAN-LINUX-CXX-NOT: "-export-dynamic"
// CHECK-TSAN-LINUX-CXX: stdc++
// CHECK-TSAN-LINUX-CXX: "-lpthread"
// CHECK-TSAN-LINUX-CXX: "-lrt"
// CHECK-TSAN-LINUX-CXX: "-ldl"

// RUN: %clangxx -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target x86_64-unknown-linux -lstdc++ -fsanitize=memory \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-MSAN-LINUX-CXX %s
//
// CHECK-MSAN-LINUX-CXX: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-MSAN-LINUX-CXX-NOT: stdc++
// CHECK-MSAN-LINUX-CXX: "-whole-archive" "{{.*}}libclang_rt.msan-x86_64.a" "-no-whole-archive"
// CHECK-MSAN-LINUX-CXX-NOT: "-export-dynamic"
// CHECK-MSAN-LINUX-CXX: "--dynamic-list={{.*}}libclang_rt.msan-x86_64.a.syms"
// CHECK-MSAN-LINUX-CXX-NOT: "-export-dynamic"
// CHECK-MSAN-LINUX-CXX: stdc++
// CHECK-MSAN-LINUX-CXX: "-lpthread"
// CHECK-MSAN-LINUX-CXX: "-lrt"
// CHECK-MSAN-LINUX-CXX: "-ldl"

// RUN: %clang -fsanitize=undefined %s -### -o %t.o 2>&1 \
// RUN:     -target i386-unknown-linux \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-UBSAN-LINUX %s
// CHECK-UBSAN-LINUX: "{{.*}}ld{{(.exe)?}}"
// CHECK-UBSAN-LINUX-NOT: libclang_rt.asan
// CHECK-UBSAN-LINUX-NOT: libclang_rt.ubsan_cxx
// CHECK-UBSAN-LINUX: "-whole-archive" "{{.*}}libclang_rt.san-i386.a" "-no-whole-archive"
// CHECK-UBSAN-LINUX-NOT: libclang_rt.asan
// CHECK-UBSAN-LINUX-NOT: libclang_rt.ubsan_cxx
// CHECK-UBSAN-LINUX: "-whole-archive" "{{.*}}libclang_rt.ubsan-i386.a" "-no-whole-archive"
// CHECK-UBSAN-LINUX-NOT: libclang_rt.asan
// CHECK-UBSAN-LINUX-NOT: libclang_rt.ubsan_cxx
// CHECK-UBSAN-LINUX-NOT: "-lstdc++"
// CHECK-UBSAN-LINUX: "-lpthread"

// RUN: %clang -fsanitize=undefined -fsanitize-link-c++-runtime %s -### -o %t.o 2>&1 \
// RUN:     -target i386-unknown-linux \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-UBSAN-LINUX-LINK-CXX %s
// CHECK-UBSAN-LINUX-LINK-CXX-NOT: "-lstdc++"
// CHECK-UBSAN-LINUX-LINK-CXX: "-whole-archive" "{{.*}}libclang_rt.ubsan_cxx-i386.a" "-no-whole-archive"
// CHECK-UBSAN-LINUX-LINK-CXX-NOT: "-lstdc++"

// RUN: %clangxx -fsanitize=undefined %s -### -o %t.o 2>&1 \
// RUN:     -target i386-unknown-linux \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-UBSAN-LINUX-CXX %s
// CHECK-UBSAN-LINUX-CXX: "{{.*}}ld{{(.exe)?}}"
// CHECK-UBSAN-LINUX-CXX-NOT: libclang_rt.asan
// CHECK-UBSAN-LINUX-CXX: "-whole-archive" "{{.*}}libclang_rt.san-i386.a" "-no-whole-archive"
// CHECK-UBSAN-LINUX-CXX-NOT: libclang_rt.asan
// CHECK-UBSAN-LINUX-CXX: "-whole-archive" "{{.*}}libclang_rt.ubsan-i386.a" "-no-whole-archive"
// CHECK-UBSAN-LINUX-CXX-NOT: libclang_rt.asan
// CHECK-UBSAN-LINUX-CXX: "--dynamic-list={{.*}}libclang_rt.ubsan-i386.a.syms"
// CHECK-UBSAN-LINUX-CXX-NOT: libclang_rt.asan
// CHECK-UBSAN-LINUX-CXX: "-whole-archive" "{{.*}}libclang_rt.ubsan_cxx-i386.a" "-no-whole-archive"
// CHECK-UBSAN-LINUX-CXX-NOT: libclang_rt.asan
// CHECK-UBSAN-LINUX-CXX: "--dynamic-list={{.*}}libclang_rt.ubsan_cxx-i386.a.syms"
// CHECK-UBSAN-LINUX-CXX-NOT: libclang_rt.asan
// CHECK-UBSAN-LINUX-CXX: "-lstdc++"
// CHECK-UBSAN-LINUX-CXX-NOT: libclang_rt.asan
// CHECK-UBSAN-LINUX-CXX: "-lpthread"

// RUN: %clang -fsanitize=address,undefined %s -### -o %t.o 2>&1 \
// RUN:     -target i386-unknown-linux \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-ASAN-UBSAN-LINUX %s
// CHECK-ASAN-UBSAN-LINUX: "{{.*}}ld{{(.exe)?}}"
// CHECK-ASAN-UBSAN-LINUX-NOT: libclang_rt.san
// CHECK-ASAN-UBSAN-LINUX: "-whole-archive" "{{.*}}libclang_rt.asan-i386.a" "-no-whole-archive"
// CHECK-ASAN-UBSAN-LINUX-NOT: libclang_rt.san
// CHECK-ASAN-UBSAN-LINUX: "-whole-archive" "{{.*}}libclang_rt.ubsan-i386.a" "-no-whole-archive"
// CHECK-ASAN-UBSAN-LINUX-NOT: libclang_rt.ubsan_cxx
// CHECK-ASAN-UBSAN-LINUX-NOT: "-lstdc++"
// CHECK-ASAN-UBSAN-LINUX: "-lpthread"

// RUN: %clangxx -fsanitize=address,undefined %s -### -o %t.o 2>&1 \
// RUN:     -target i386-unknown-linux \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-ASAN-UBSAN-LINUX-CXX %s
// CHECK-ASAN-UBSAN-LINUX-CXX: "{{.*}}ld{{(.exe)?}}"
// CHECK-ASAN-UBSAN-LINUX-CXX-NOT: libclang_rt.san
// CHECK-ASAN-UBSAN-LINUX-CXX: "-whole-archive" "{{.*}}libclang_rt.asan-i386.a" "-no-whole-archive"
// CHECK-ASAN-UBSAN-LINUX-CXX-NOT: libclang_rt.san
// CHECK-ASAN-UBSAN-LINUX-CXX: "-whole-archive" "{{.*}}libclang_rt.ubsan-i386.a" "-no-whole-archive"
// CHECK-ASAN-UBSAN-LINUX-CXX: "-whole-archive" "{{.*}}libclang_rt.ubsan_cxx-i386.a" "-no-whole-archive"
// CHECK-ASAN-UBSAN-LINUX-CXX: "-lstdc++"
// CHECK-ASAN-UBSAN-LINUX-CXX: "-lpthread"

// RUN: %clang -fsanitize=undefined %s -### -o %t.o 2>&1 \
// RUN:     -target i386-unknown-linux \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:     -shared \
// RUN:   | FileCheck --check-prefix=CHECK-UBSAN-LINUX-SHARED %s
// CHECK-UBSAN-LINUX-SHARED: "{{.*}}ld{{(.exe)?}}"
// CHECK-UBSAN-LINUX-SHARED-NOT: --export-dynamic
// CHECK-UBSAN-LINUX-SHARED-NOT: --dynamic-list
// CHECK-UBSAN-LINUX-SHARED-NOT: libclang_rt.ubsan-i386.a"
// CHECK-UBSAN-LINUX-SHARED-NOT: --export-dynamic
// CHECK-UBSAN-LINUX-SHARED-NOT: --dynamic-list

// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target x86_64-unknown-linux -fsanitize=leak \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-LSAN-LINUX %s
//
// CHECK-LSAN-LINUX: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-LSAN-LINUX-NOT: "-lc"
// CHECK-LSAN-LINUX: libclang_rt.lsan-x86_64.a"
// CHECK-LSAN-LINUX: "-lpthread"
// CHECK-LSAN-LINUX: "-ldl"

// RUN: %clang -fsanitize=leak,undefined %s -### -o %t.o 2>&1 \
// RUN:     -target x86_64-unknown-linux \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-LSAN-UBSAN-LINUX %s
// CHECK-LSAN-UBSAN-LINUX: "{{.*}}ld{{(.exe)?}}"
// CHECK-LSAN-UBSAN-LINUX-NOT: libclang_rt.san
// CHECK-LSAN-UBSAN-LINUX: "-whole-archive" "{{.*}}libclang_rt.lsan-x86_64.a" "-no-whole-archive"
// CHECK-LSAN-UBSAN-LINUX-NOT: libclang_rt.san
// CHECK-LSAN-UBSAN-LINUX: "-whole-archive" "{{.*}}libclang_rt.ubsan-x86_64.a" "-no-whole-archive"
// CHECK-LSAN-UBSAN-LINUX-NOT: libclang_rt.ubsan_cxx
// CHECK-LSAN-UBSAN-LINUX-NOT: "-lstdc++"
// CHECK-LSAN-UBSAN-LINUX: "-lpthread"

// RUN: %clang -fsanitize=leak,address %s -### -o %t.o 2>&1 \
// RUN:     -target x86_64-unknown-linux \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-LSAN-ASAN-LINUX %s
// CHECK-LSAN-ASAN-LINUX: "{{.*}}ld{{(.exe)?}}"
// CHECK-LSAN-ASAN-LINUX-NOT: libclang_rt.lsan
// CHECK-LSAN-ASAN-LINUX: libclang_rt.asan-x86_64
// CHECK-LSAN-ASAN-LINUX-NOT: libclang_rt.lsan
