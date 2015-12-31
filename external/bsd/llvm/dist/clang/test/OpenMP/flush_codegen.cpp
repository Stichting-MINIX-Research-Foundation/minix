// RUN: %clang_cc1 -verify -fopenmp=libiomp5 -x c++ -emit-llvm %s -fexceptions -fcxx-exceptions -o - | FileCheck %s
// RUN: %clang_cc1 -fopenmp=libiomp5 -x c++ -std=c++11 -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -emit-pch -o %t %s
// RUN: %clang_cc1 -fopenmp=libiomp5 -x c++ -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -g -std=c++11 -include-pch %t -verify %s -emit-llvm -o - | FileCheck %s
// expected-no-diagnostics

#ifndef HEADER
#define HEADER

template <class T>
T tmain(T argc) {
  static T a;
#pragma omp flush
#pragma omp flush(a)
  return a + argc;
}

// CHECK-LABEL: @main
int main() {
  static int a;
#pragma omp flush
#pragma omp flush(a)
  // CHECK: call void (%{{.+}}*, ...)* @__kmpc_flush(%{{.+}}* {{(@|%).+}}, i32 0)
  // CHECK: call void (%{{.+}}*, ...)* @__kmpc_flush(%{{.+}}* {{(@|%).+}}, i32 0)
  return tmain(a);
  // CHECK: call {{.*}} [[TMAIN:@.+]](
  // CHECK: ret
}

// CHECK: [[TMAIN]]
// CHECK: call void (%{{.+}}*, ...)* @__kmpc_flush(%{{.+}}* {{(@|%).+}}, i32 0)
// CHECK: call void (%{{.+}}*, ...)* @__kmpc_flush(%{{.+}}* {{(@|%).+}}, i32 0)
// CHECK: ret

#endif
