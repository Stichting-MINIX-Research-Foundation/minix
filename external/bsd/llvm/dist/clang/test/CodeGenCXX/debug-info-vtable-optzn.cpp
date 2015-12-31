// RUN: %clang_cc1 -emit-llvm -g -triple x86_64-apple-darwin %s -o - | FileCheck %s
// RUN: %clang_cc1 -emit-llvm -g -triple amd64-unknown-freebsd %s -o - | FileCheck %s
//
// This tests that the "emit debug info for a C++ class only in the
// module that has its vtable" optimization is disabled by default on
// Darwin and FreeBSD.
//
// CHECK: [ DW_TAG_member ] [lost]
class A
{
  virtual bool f() = 0;
  int lost;
};

class B : public A
{
  B *g();
};

B *B::g() {
  return this;
}
