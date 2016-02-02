// RUN: %clang_cc1 %s -g -triple %itanium_abi_triple -fno-use-cxa-atexit -S -emit-llvm -o - \
// RUN:     | FileCheck %s --check-prefix=CHECK-NOKEXT
// RUN: %clang_cc1 %s -g -triple %itanium_abi_triple -fno-use-cxa-atexit -fapple-kext -S -emit-llvm -o - \
// RUN:     | FileCheck %s --check-prefix=CHECK-KEXT

class A {
 public:
  A() {}
  virtual ~A() {}
};

A glob;
A array[2];

void foo() {
  static A stat;
}

// CHECK-NOKEXT: [ DW_TAG_subprogram ] [line 12] [local] [def] [__cxx_global_var_init]
// CHECK-NOKEXT: [ DW_TAG_subprogram ] [line 12] [local] [def] [__dtor_glob]
// CHECK-NOKEXT: [ DW_TAG_subprogram ] [line 13] [local] [def] [__cxx_global_var_init1]
// CHECK-NOKEXT: [ DW_TAG_subprogram ] [line 13] [local] [def] [__cxx_global_array_dtor]
// CHECK-NOKEXT: [ DW_TAG_subprogram ] [line 13] [local] [def] [__dtor_array]
// CHECK-NOKEXT: [ DW_TAG_subprogram ] [line 16] [local] [def] [__dtor__ZZ3foovE4stat]
// CHECK-NOKEXT: [ DW_TAG_subprogram ] [line {{.*}}] [local] [def]{{$}}

// CHECK-KEXT: [ DW_TAG_subprogram ] [line {{.*}}] [local] [def]{{$}}
