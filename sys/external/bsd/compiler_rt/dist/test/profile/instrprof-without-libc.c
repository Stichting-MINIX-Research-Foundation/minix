// RUN: %clang_profgen -DCHECK_SYMBOLS -O3 -o %t.symbols %s
// RUN: llvm-nm %t.symbols | FileCheck %s --check-prefix=CHECK-SYMBOLS
// RUN: %clang_profgen -O3 -o %t %s
// RUN: %run %t %t.profraw
// RUN: llvm-profdata merge -o %t.profdata %t.profraw
// RUN: %clang_profuse=%t.profdata -o - -S -emit-llvm %s | FileCheck %s

#include <stdint.h>
#include <stdlib.h>

#ifndef CHECK_SYMBOLS
#include <stdio.h>
#endif

int __llvm_profile_runtime = 0;
uint64_t __llvm_profile_get_size_for_buffer(void);
int __llvm_profile_write_buffer(char *);
int write_buffer(uint64_t, const char *);
int main(int argc, const char *argv[]) {
  // CHECK-LABEL: define i32 @main(
  // CHECK: br i1 %{{.*}}, label %{{.*}}, label %{{.*}}, !prof !1
  if (argc < 2)
    return 1;

  const uint64_t MaxSize = 10000;
  static char Buffer[MaxSize];

  uint64_t Size = __llvm_profile_get_size_for_buffer();
  if (Size > MaxSize)
    return 1;
  int Write = __llvm_profile_write_buffer(Buffer);
  if (__llvm_profile_write_buffer(Buffer))
    return Write;

#ifdef CHECK_SYMBOLS
  // Don't write it out.  Since we're checking the symbols, we don't have libc
  // available.
  return 0;
#else
  // Actually write it out so we can FileCheck the output.
  FILE *File = fopen(argv[1], "w");
  if (!File)
    return 1;
  if (fwrite(Buffer, 1, Size, File) != Size)
    return 1;
  return fclose(File);
#endif
}
// CHECK: !1 = metadata !{metadata !"branch_weights", i32 1, i32 2}

// CHECK-SYMBOLS-NOT: ___cxx_global_var_init
// CHECK-SYMBOLS-NOT: ___llvm_profile_register_write_file_atexit
// CHECK-SYMBOLS-NOT: ___llvm_profile_set_filename
// CHECK-SYMBOLS-NOT: ___llvm_profile_write_file
// CHECK-SYMBOLS-NOT: _fdopen
// CHECK-SYMBOLS-NOT: _fopen
// CHECK-SYMBOLS-NOT: _fwrite
// CHECK-SYMBOLS-NOT: _getenv
// CHECK-SYMBOLS-NOT: _malloc
// CHECK-SYMBOLS-NOT: _open
