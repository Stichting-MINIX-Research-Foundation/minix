// RUN: %clang_cc1 -emit-llvm %s -o - -triple=i386-pc-win32 -fno-rtti -mconstructor-aliases -O1 -disable-llvm-optzns | FileCheck %s

namespace test1 {
template <typename T> class A {
  ~A() {}
};
template class A<char>;
// CHECK-DAG: define weak_odr x86_thiscallcc void @"\01??1?$A@D@test1@@AAE@XZ"
}

namespace test2 {
struct A {
  virtual ~A();
};
struct B : A {
  B();
  virtual ~B();
};

A::~A() {}
B::~B() {}
void foo() {
  B b;
}
// CHECK-DAG: @"\01??1B@test2@@UAE@XZ" = alias bitcast (void (%"struct.test2::A"*)* @"\01??1A@test2@@UAE@XZ" to void (%"struct.test2::B"*)*)
}
