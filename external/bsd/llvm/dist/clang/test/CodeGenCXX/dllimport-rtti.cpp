// RUN: %clang_cc1 -triple i686-windows-msvc -emit-llvm -std=c++1y -O1 -disable-llvm-optzns -o - %s | FileCheck %s --check-prefix=MSVC
// RUN: %clang_cc1 -triple i686-windows-gnu  -emit-llvm -std=c++1y -O1 -disable-llvm-optzns -o - %s | FileCheck %s --check-prefix=GNU

struct __declspec(dllimport) S {
  virtual void f() {}
} s;
// MSVC-DAG: @"\01??_7S@@6B@" = available_externally dllimport
// MSVC-DAG: @"\01??_R0?AUS@@@8" = linkonce_odr
// MSVC-DAG: @"\01??_R1A@?0A@EA@S@@8" = linkonce_odr
// MSVC-DAG: @"\01??_R2S@@8" = linkonce_odr
// MSVC-DAG: @"\01??_R3S@@8" = linkonce_odr

// GNU-DAG: @_ZTV1S = available_externally dllimport
// GNU-DAG: @_ZTI1S = external dllimport

struct U : S {
} u;
