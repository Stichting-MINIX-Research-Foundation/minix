// RUN: %clang_cc1 -verify -fopenmp=libiomp5 -ast-print %s | FileCheck %s
// RUN: %clang_cc1 -fopenmp=libiomp5 -x c++ -std=c++11 -emit-pch -o %t %s
// RUN: %clang_cc1 -fopenmp=libiomp5 -std=c++11 -include-pch %t -fsyntax-only -verify %s -ast-print | FileCheck %s
// expected-no-diagnostics

#ifndef HEADER
#define HEADER

void foo() {}

template <class T, int N>
T tmain(T argc) {
  T b = argc, c, d, e, f, h;
  static T a;
// CHECK: static T a;
  static T g;
#pragma omp threadprivate(g)
#pragma omp parallel for schedule(dynamic) default(none) copyin(g)
  // CHECK: #pragma omp parallel for schedule(dynamic) default(none) copyin(g)
  for (int i = 0; i < 2; ++i)
    a = 2;
// CHECK-NEXT: for (int i = 0; i < 2; ++i)
// CHECK-NEXT: a = 2;
#pragma omp parallel for private(argc, b), firstprivate(c, d), lastprivate(d, f) collapse(N) schedule(static, N) ordered if (argc) num_threads(N) default(shared) shared(e) reduction(+ : h)
  for (int i = 0; i < 10; ++i)
    for (int j = 0; j < 10; ++j)
      for (int j = 0; j < 10; ++j)
        for (int j = 0; j < 10; ++j)
          for (int j = 0; j < 10; ++j)
            foo();
  // CHECK-NEXT: #pragma omp parallel for private(argc,b) firstprivate(c,d) lastprivate(d,f) collapse(N) schedule(static, N) ordered if(argc) num_threads(N) default(shared) shared(e) reduction(+: h)
  // CHECK-NEXT: for (int i = 0; i < 10; ++i)
  // CHECK-NEXT: for (int j = 0; j < 10; ++j)
  // CHECK-NEXT: for (int j = 0; j < 10; ++j)
  // CHECK-NEXT: for (int j = 0; j < 10; ++j)
  // CHECK-NEXT: for (int j = 0; j < 10; ++j)
  // CHECK-NEXT: foo();
  return T();
}

int main(int argc, char **argv) {
  int b = argc, c, d, e, f, h;
  static int a;
// CHECK: static int a;
  static float g;
#pragma omp threadprivate(g)
#pragma omp parallel for schedule(guided, argc) default(none) copyin(g)
  // CHECK: #pragma omp parallel for schedule(guided, argc) default(none) copyin(g)
  for (int i = 0; i < 2; ++i)
    a = 2;
// CHECK-NEXT: for (int i = 0; i < 2; ++i)
// CHECK-NEXT: a = 2;
#pragma omp parallel for private(argc, b), firstprivate(argv, c), lastprivate(d, f) collapse(2) schedule(auto) ordered if (argc) num_threads(a) default(shared) shared(e) reduction(+ : h)
  for (int i = 0; i < 10; ++i)
    for (int j = 0; j < 10; ++j)
      foo();
  // CHECK-NEXT: #pragma omp parallel for private(argc,b) firstprivate(argv,c) lastprivate(d,f) collapse(2) schedule(auto) ordered if(argc) num_threads(a) default(shared) shared(e) reduction(+: h)
  // CHECK-NEXT: for (int i = 0; i < 10; ++i)
  // CHECK-NEXT: for (int j = 0; j < 10; ++j)
  // CHECK-NEXT: foo();
  return (tmain<int, 5>(argc) + tmain<char, 1>(argv[0][0]));
}

#endif
