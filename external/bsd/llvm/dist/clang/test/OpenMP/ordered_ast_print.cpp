// RUN: %clang_cc1 -verify -fopenmp=libiomp5 -ast-print %s | FileCheck %s
// RUN: %clang_cc1 -fopenmp=libiomp5 -x c++ -std=c++11 -emit-pch -o %t %s
// RUN: %clang_cc1 -fopenmp=libiomp5 -std=c++11 -include-pch %t -fsyntax-only -verify %s -ast-print | FileCheck %s
// expected-no-diagnostics

#ifndef HEADER
#define HEADER

void foo() {}

template <class T>
T tmain (T argc) {
  T b = argc, c, d, e, f, g;
  static T a;
  #pragma omp for ordered
  for (int i =0 ; i < argc; ++i)
  #pragma omp ordered
  {
    a=2;
  }
  return (0);
}

// CHECK: static int a;
// CHECK-NEXT: #pragma omp for ordered
// CHECK-NEXT: for (int i = 0; i < argc; ++i)
// CHECK-NEXT: #pragma omp ordered
// CHECK-NEXT: {
// CHECK-NEXT: a = 2;
// CHECK-NEXT: }

// CHECK: static T a;
// CHECK-NEXT: #pragma omp for ordered
// CHECK-NEXT: for (int i = 0; i < argc; ++i)
// CHECK-NEXT: #pragma omp ordered
// CHECK-NEXT: {
// CHECK-NEXT: a = 2;
// CHECK-NEXT: }

int main (int argc, char **argv) {
  int b = argc, c, d, e, f, g;
  static int a;
// CHECK: static int a;
  #pragma omp for ordered
  for (int i =0 ; i < argc; ++i)
  #pragma omp ordered
  {
    a=2;
  }
// CHECK-NEXT: #pragma omp for ordered
// CHECK-NEXT: for (int i = 0; i < argc; ++i)
// CHECK-NEXT: #pragma omp ordered
// CHECK-NEXT: {
// CHECK-NEXT: a = 2;
// CHECK-NEXT: }
  return tmain(argc);
}

#endif
