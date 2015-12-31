// RUN: %clang_cc1 -triple i386-unknown-unknown -emit-llvm -O1 -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i386-unknown-unknown -emit-llvm -O1 -fcxx-exceptions -fexceptions -o - %s | FileCheck --check-prefix=CHECK-EH %s

// Test code generation for the named return value optimization.
class X {
public:
  X();
  X(const X&);
  ~X();
};

template<typename T> struct Y {
  Y();
  static Y f() {
    Y y;
    return y;
  }
};

// CHECK-LABEL: define void @_Z5test0v
// CHECK-EH-LABEL: define void @_Z5test0v
X test0() {
  X x;
  // CHECK:          call {{.*}} @_ZN1XC1Ev
  // CHECK-NEXT:     ret void

  // CHECK-EH:       call {{.*}} @_ZN1XC1Ev
  // CHECK-EH-NEXT:  ret void
  return x;
}

// CHECK-LABEL: define void @_Z5test1b(
// CHECK-EH-LABEL: define void @_Z5test1b(
X test1(bool B) {
  // CHECK:      tail call {{.*}} @_ZN1XC1Ev
  // CHECK-NEXT: ret void
  X x;
  if (B)
    return (x);
  return x;
  // CHECK-EH:      tail call {{.*}} @_ZN1XC1Ev
  // CHECK-EH-NEXT: ret void
}

// CHECK-LABEL: define void @_Z5test2b
// CHECK-EH-LABEL: define void @_Z5test2b
X test2(bool B) {
  // No NRVO.

  X x;
  X y;
  if (B)
    return y;
  return x;

  // CHECK: call {{.*}} @_ZN1XC1Ev
  // CHECK-NEXT: call {{.*}} @_ZN1XC1Ev
  // CHECK: call {{.*}} @_ZN1XC1ERKS_
  // CHECK: call {{.*}} @_ZN1XC1ERKS_
  // CHECK: call {{.*}} @_ZN1XD1Ev
  // CHECK: call {{.*}} @_ZN1XD1Ev
  // CHECK: ret void

  // The block ordering in the -fexceptions IR is unfortunate.

  // CHECK-EH:      call {{.*}} @_ZN1XC1Ev
  // CHECK-EH-NEXT: invoke {{.*}} @_ZN1XC1Ev
  // -> %invoke.cont, %lpad

  // %invoke.cont:
  // CHECK-EH:      br i1
  // -> %if.then, %if.end

  // %if.then: returning 'x'
  // CHECK-EH:      invoke {{.*}} @_ZN1XC1ERKS_
  // -> %cleanup, %lpad1

  // %lpad: landing pad for ctor of 'y', dtor of 'y'
  // CHECK-EH:      [[CAUGHTVAL:%.*]] = landingpad { i8*, i32 } personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*)
  // CHECK-EH-NEXT:   cleanup
  // CHECK-EH-NEXT: extractvalue { i8*, i32 } [[CAUGHTVAL]], 0
  // CHECK-EH-NEXT: extractvalue { i8*, i32 } [[CAUGHTVAL]], 1
  // CHECK-EH-NEXT: br label
  // -> %eh.cleanup

  // %lpad1: landing pad for return copy ctors, EH cleanup for 'y'
  // CHECK-EH: invoke {{.*}} @_ZN1XD1Ev
  // -> %eh.cleanup, %terminate.lpad

  // %if.end: returning 'y'
  // CHECK-EH: invoke {{.*}} @_ZN1XC1ERKS_
  // -> %cleanup, %lpad1

  // %cleanup: normal cleanup for 'y'
  // CHECK-EH: invoke {{.*}} @_ZN1XD1Ev
  // -> %invoke.cont11, %lpad

  // %invoke.cont11: normal cleanup for 'x'
  // CHECK-EH:      call {{.*}} @_ZN1XD1Ev
  // CHECK-EH-NEXT: ret void

  // %eh.cleanup:  EH cleanup for 'x'
  // CHECK-EH: invoke {{.*}} @_ZN1XD1Ev
  // -> %invoke.cont17, %terminate.lpad

  // %invoke.cont17: rethrow block for %eh.cleanup.
  // This really should be elsewhere in the function.
  // CHECK-EH:      resume { i8*, i32 }

  // %terminate.lpad: terminate landing pad.
  // CHECK-EH:      [[T0:%.*]] = landingpad { i8*, i32 } personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*)
  // CHECK-EH-NEXT:   catch i8* null
  // CHECK-EH-NEXT: [[T1:%.*]] = extractvalue { i8*, i32 } [[T0]], 0
  // CHECK-EH-NEXT: call void @__clang_call_terminate(i8* [[T1]]) [[NR_NUW:#[0-9]+]]
  // CHECK-EH-NEXT: unreachable

}

// CHECK-LABEL: define void @_Z5test3b
X test3(bool B) {
  // CHECK: tail call {{.*}} @_ZN1XC1Ev
  // CHECK-NOT: call {{.*}} @_ZN1XC1ERKS_
  // CHECK: call {{.*}} @_ZN1XC1Ev
  // CHECK: call {{.*}} @_ZN1XC1ERKS_
  if (B) {
    X y;
    return y;
  }
  // FIXME: we should NRVO this variable too.
  X x;
  return x;
}

extern "C" void exit(int) throw();

// CHECK-LABEL: define void @_Z5test4b
X test4(bool B) {
  {
    // CHECK: tail call {{.*}} @_ZN1XC1Ev
    X x;
    // CHECK: br i1
    if (B)
      return x;
  }
  // CHECK: tail call {{.*}} @_ZN1XD1Ev
  // CHECK: tail call void @exit(i32 1)
  exit(1);
}

#ifdef __EXCEPTIONS
// CHECK-EH-LABEL: define void @_Z5test5
void may_throw();
X test5() {
  try {
    may_throw();
  } catch (X x) {
    // CHECK-EH: invoke {{.*}} @_ZN1XC1ERKS_
    // CHECK-EH: call void @__cxa_end_catch()
    // CHECK-EH: ret void
    return x;
  }
}
#endif

// rdar://problem/10430868
// CHECK-LABEL: define void @_Z5test6v
X test6() {
  X a __attribute__((aligned(8)));
  return a;
  // CHECK:      [[A:%.*]] = alloca [[X:%.*]], align 8
  // CHECK-NEXT: call {{.*}} @_ZN1XC1Ev([[X]]* [[A]])
  // CHECK-NEXT: call {{.*}} @_ZN1XC1ERKS_([[X]]* {{%.*}}, [[X]]* dereferenceable({{[0-9]+}}) [[A]])
  // CHECK-NEXT: call {{.*}} @_ZN1XD1Ev([[X]]* [[A]])
  // CHECK-NEXT: ret void
}

// CHECK-LABEL: define void @_Z5test7b
X test7(bool b) {
  // CHECK: tail call {{.*}} @_ZN1XC1Ev
  // CHECK-NEXT: ret
  if (b) {
    X x;
    return x;
  }
  return X();
}

// CHECK-LABEL: define void @_Z5test8b
X test8(bool b) {
  // CHECK: tail call {{.*}} @_ZN1XC1Ev
  // CHECK-NEXT: ret
  if (b) {
    X x;
    return x;
  } else {
    X y;
    return y;
  }
}

Y<int> test9() {
  Y<int>::f();
}

// CHECK-LABEL: define linkonce_odr void @_ZN1YIiE1fEv
// CHECK: tail call {{.*}} @_ZN1YIiEC1Ev

// CHECK-EH: attributes [[NR_NUW]] = { noreturn nounwind }
