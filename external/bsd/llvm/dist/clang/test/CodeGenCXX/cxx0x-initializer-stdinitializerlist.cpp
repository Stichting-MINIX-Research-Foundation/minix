// RUN: %clang_cc1 -std=c++11 -triple x86_64-none-linux-gnu -emit-llvm -o - %s | FileCheck %s

namespace std {
  typedef decltype(sizeof(int)) size_t;

  // libc++'s implementation
  template <class _E>
  class initializer_list
  {
    const _E* __begin_;
    size_t    __size_;

    initializer_list(const _E* __b, size_t __s)
      : __begin_(__b),
        __size_(__s)
    {}

  public:
    typedef _E        value_type;
    typedef const _E& reference;
    typedef const _E& const_reference;
    typedef size_t    size_type;

    typedef const _E* iterator;
    typedef const _E* const_iterator;

    initializer_list() : __begin_(nullptr), __size_(0) {}

    size_t    size()  const {return __size_;}
    const _E* begin() const {return __begin_;}
    const _E* end()   const {return __begin_ + __size_;}
  };
}

struct destroyme1 {
  ~destroyme1();
};
struct destroyme2 {
  ~destroyme2();
};
struct witharg1 {
  witharg1(const destroyme1&);
  ~witharg1();
};
struct wantslist1 {
  wantslist1(std::initializer_list<destroyme1>);
  ~wantslist1();
};

// CHECK: @_ZGR15globalInitList1_ = private constant [3 x i32] [i32 1, i32 2, i32 3]
// CHECK: @globalInitList1 = global %{{[^ ]+}} { i32* getelementptr inbounds ([3 x i32]* @_ZGR15globalInitList1_, i32 0, i32 0), i{{32|64}} 3 }
std::initializer_list<int> globalInitList1 = {1, 2, 3};

namespace thread_local_global_array {
  // FIXME: We should be able to constant-evaluate this even though the
  // initializer is not a constant expression (pointers to thread_local
  // objects aren't really a problem).
  //
  // CHECK: @_ZN25thread_local_global_array1xE = thread_local global
  // CHECK: @_ZGRN25thread_local_global_array1xE_ = private thread_local constant [4 x i32] [i32 1, i32 2, i32 3, i32 4]
  std::initializer_list<int> thread_local x = { 1, 2, 3, 4 };
}

// CHECK: @globalInitList2 = global %{{[^ ]+}} zeroinitializer
// CHECK: @_ZGR15globalInitList2_ = private global [2 x %[[WITHARG:[^ ]*]]] zeroinitializer

// CHECK: @_ZN15partly_constant1kE = global i32 0, align 4
// CHECK: @_ZN15partly_constant2ilE = global {{.*}} null, align 8
// CHECK: @[[PARTLY_CONSTANT_OUTER:_ZGRN15partly_constant2ilE.*]] = private global {{.*}} zeroinitializer, align 8
// CHECK: @[[PARTLY_CONSTANT_INNER:_ZGRN15partly_constant2ilE.*]] = private global [3 x {{.*}}] zeroinitializer, align 8
// CHECK: @[[PARTLY_CONSTANT_FIRST:_ZGRN15partly_constant2ilE.*]] = private constant [3 x i32] [i32 1, i32 2, i32 3], align 4
// CHECK: @[[PARTLY_CONSTANT_SECOND:_ZGRN15partly_constant2ilE.*]] = private global [2 x i32] zeroinitializer, align 4
// CHECK: @[[PARTLY_CONSTANT_THIRD:_ZGRN15partly_constant2ilE.*]] = private constant [4 x i32] [i32 5, i32 6, i32 7, i32 8], align 4

// CHECK: appending global


// thread_local initializer:
// CHECK-LABEL: define internal void
// CHECK: store i32* getelementptr inbounds ([4 x i32]* @_ZGRN25thread_local_global_array1xE_, i64 0, i64 0),
// CHECK:       i32** getelementptr inbounds ({{.*}}* @_ZN25thread_local_global_array1xE, i32 0, i32 0), align 8
// CHECK: store i64 4, i64* getelementptr inbounds ({{.*}}* @_ZN25thread_local_global_array1xE, i32 0, i32 1), align 8


// CHECK-LABEL: define internal void
// CHECK: call void @_ZN8witharg1C1ERK10destroyme1(%[[WITHARG]]* getelementptr inbounds ([2 x %[[WITHARG]]]* @_ZGR15globalInitList2_, i{{32|64}} 0, i{{32|64}} 0
// CHECK: call void @_ZN8witharg1C1ERK10destroyme1(%[[WITHARG]]* getelementptr inbounds ([2 x %[[WITHARG]]]* @_ZGR15globalInitList2_, i{{32|64}} 0, i{{32|64}} 1
// CHECK: __cxa_atexit
// CHECK: store %[[WITHARG]]* getelementptr inbounds ([2 x %[[WITHARG]]]* @_ZGR15globalInitList2_, i64 0, i64 0),
// CHECK:       %[[WITHARG]]** getelementptr inbounds (%{{.*}}* @globalInitList2, i32 0, i32 0), align 8
// CHECK: store i64 2, i64* getelementptr inbounds (%{{.*}}* @globalInitList2, i32 0, i32 1), align 8
// CHECK: call void @_ZN10destroyme1D1Ev
// CHECK: call void @_ZN10destroyme1D1Ev
std::initializer_list<witharg1> globalInitList2 = {
  witharg1(destroyme1()), witharg1(destroyme1())
};

void fn1(int i) {
  // CHECK-LABEL: define void @_Z3fn1i
  // temporary array
  // CHECK: [[array:%[^ ]+]] = alloca [3 x i32]
  // CHECK: getelementptr inbounds [3 x i32]* [[array]], i{{32|64}} 0
  // CHECK-NEXT: store i32 1, i32*
  // CHECK-NEXT: getelementptr
  // CHECK-NEXT: store
  // CHECK-NEXT: getelementptr
  // CHECK-NEXT: load
  // CHECK-NEXT: store
  // init the list
  // CHECK-NEXT: getelementptr
  // CHECK-NEXT: getelementptr inbounds [3 x i32]*
  // CHECK-NEXT: store i32*
  // CHECK-NEXT: getelementptr
  // CHECK-NEXT: store i{{32|64}} 3
  std::initializer_list<int> intlist{1, 2, i};
}

void fn2() {
  // CHECK-LABEL: define void @_Z3fn2v
  void target(std::initializer_list<destroyme1>);
  // objects should be destroyed before dm2, after call returns
  // CHECK: call void @_Z6targetSt16initializer_listI10destroyme1E
  target({ destroyme1(), destroyme1() });
  // CHECK: call void @_ZN10destroyme1D1Ev
  destroyme2 dm2;
  // CHECK: call void @_ZN10destroyme2D1Ev
}

void fn3() {
  // CHECK-LABEL: define void @_Z3fn3v
  // objects should be destroyed after dm2
  auto list = { destroyme1(), destroyme1() };
  destroyme2 dm2;
  // CHECK: call void @_ZN10destroyme2D1Ev
  // CHECK: call void @_ZN10destroyme1D1Ev
}

void fn4() {
  // CHECK-LABEL: define void @_Z3fn4v
  void target(std::initializer_list<witharg1>);
  // objects should be destroyed before dm2, after call returns
  // CHECK: call void @_ZN8witharg1C1ERK10destroyme1
  // CHECK: call void @_Z6targetSt16initializer_listI8witharg1E
  target({ witharg1(destroyme1()), witharg1(destroyme1()) });
  // CHECK: call void @_ZN8witharg1D1Ev
  // CHECK: call void @_ZN10destroyme1D1Ev
  destroyme2 dm2;
  // CHECK: call void @_ZN10destroyme2D1Ev
}

void fn5() {
  // CHECK-LABEL: define void @_Z3fn5v
  // temps should be destroyed before dm2
  // objects should be destroyed after dm2
  // CHECK: call void @_ZN8witharg1C1ERK10destroyme1
  auto list = { witharg1(destroyme1()), witharg1(destroyme1()) };
  // CHECK: call void @_ZN10destroyme1D1Ev
  destroyme2 dm2;
  // CHECK: call void @_ZN10destroyme2D1Ev
  // CHECK: call void @_ZN8witharg1D1Ev
}

void fn6() {
  // CHECK-LABEL: define void @_Z3fn6v
  void target(const wantslist1&);
  // objects should be destroyed before dm2, after call returns
  // CHECK: call void @_ZN10wantslist1C1ESt16initializer_listI10destroyme1E
  // CHECK: call void @_Z6targetRK10wantslist1
  target({ destroyme1(), destroyme1() });
  // CHECK: call void @_ZN10wantslist1D1Ev
  // CHECK: call void @_ZN10destroyme1D1Ev
  destroyme2 dm2;
  // CHECK: call void @_ZN10destroyme2D1Ev
}

void fn7() {
  // CHECK-LABEL: define void @_Z3fn7v
  // temps should be destroyed before dm2
  // object should be destroyed after dm2
  // CHECK: call void @_ZN10wantslist1C1ESt16initializer_listI10destroyme1E
  wantslist1 wl = { destroyme1(), destroyme1() };
  // CHECK: call void @_ZN10destroyme1D1Ev
  destroyme2 dm2;
  // CHECK: call void @_ZN10destroyme2D1Ev
  // CHECK: call void @_ZN10wantslist1D1Ev
}

void fn8() {
  // CHECK-LABEL: define void @_Z3fn8v
  void target(std::initializer_list<std::initializer_list<destroyme1>>);
  // objects should be destroyed before dm2, after call returns
  // CHECK: call void @_Z6targetSt16initializer_listIS_I10destroyme1EE
  std::initializer_list<destroyme1> inner;
  target({ inner, { destroyme1() } });
  // CHECK: call void @_ZN10destroyme1D1Ev
  // Only one destroy loop, since only one inner init list is directly inited.
  // CHECK-NOT: call void @_ZN10destroyme1D1Ev
  destroyme2 dm2;
  // CHECK: call void @_ZN10destroyme2D1Ev
}

void fn9() {
  // CHECK-LABEL: define void @_Z3fn9v
  // objects should be destroyed after dm2
  std::initializer_list<destroyme1> inner;
  std::initializer_list<std::initializer_list<destroyme1>> list =
      { inner, { destroyme1() } };
  destroyme2 dm2;
  // CHECK: call void @_ZN10destroyme2D1Ev
  // CHECK: call void @_ZN10destroyme1D1Ev
  // Only one destroy loop, since only one inner init list is directly inited.
  // CHECK-NOT: call void @_ZN10destroyme1D1Ev
  // CHECK: ret void
}

struct haslist1 {
  std::initializer_list<int> il;
  haslist1();
};

// CHECK-LABEL: define void @_ZN8haslist1C2Ev
haslist1::haslist1()
// CHECK: alloca [3 x i32]
// CHECK: store i32 1
// CHECK: store i32 2
// CHECK: store i32 3
// CHECK: store i{{32|64}} 3
  : il{1, 2, 3}
{
  destroyme2 dm2;
}

struct haslist2 {
  std::initializer_list<destroyme1> il;
  haslist2();
};

// CHECK-LABEL: define void @_ZN8haslist2C2Ev
haslist2::haslist2()
  : il{destroyme1(), destroyme1()}
{
  destroyme2 dm2;
  // CHECK: call void @_ZN10destroyme2D1Ev
  // CHECK: call void @_ZN10destroyme1D1Ev
}

void fn10() {
  // CHECK-LABEL: define void @_Z4fn10v
  // CHECK: alloca [3 x i32]
  // CHECK: call noalias i8* @_Znw{{[jm]}}
  // CHECK: store i32 1
  // CHECK: store i32 2
  // CHECK: store i32 3
  // CHECK: store i32*
  // CHECK: store i{{32|64}} 3
  (void) new std::initializer_list<int> {1, 2, 3};
}

void fn11() {
  // CHECK-LABEL: define void @_Z4fn11v
  (void) new std::initializer_list<destroyme1> {destroyme1(), destroyme1()};
  // CHECK: call void @_ZN10destroyme1D1Ev
  destroyme2 dm2;
  // CHECK: call void @_ZN10destroyme2D1Ev
}

namespace PR12178 {
  struct string {
    string(int);
    ~string();
  };

  struct pair {
    string a;
    int b;
  };

  struct map {
    map(std::initializer_list<pair>);
  };

  map m{ {1, 2}, {3, 4} };
}

namespace rdar13325066 {
  struct X { ~X(); };

  // CHECK-LABEL: define void @_ZN12rdar133250664loopERNS_1XES1_
  void loop(X &x1, X &x2) {
    // CHECK: br label
    // CHECK: br i1
    // CHECK: br label
    // CHECK call void @_ZN12rdar133250661XD1Ev
    // CHECK: br label
    // CHECK: br label
    // CHECK: call void @_ZN12rdar133250661XD1Ev
    // CHECK: br i1
    // CHECK: br label
    // CHECK: ret void
    for (X x : { x1, x2 }) { }
  }
}

namespace dtors {
  struct S {
    S();
    ~S();
  };
  void z();

  // CHECK-LABEL: define void @_ZN5dtors1fEv(
  void f() {
    // CHECK: call void @_ZN5dtors1SC1Ev(
    // CHECK: call void @_ZN5dtors1SC1Ev(
    std::initializer_list<S>{ S(), S() };

    // Destruction loop for underlying array.
    // CHECK: br label
    // CHECK: call void @_ZN5dtors1SD1Ev(
    // CHECK: br i1

    // CHECK: call void @_ZN5dtors1zEv(
    z();

    // CHECK-NOT: call void @_ZN5dtors1SD1Ev(
  }

  // CHECK-LABEL: define void @_ZN5dtors1gEv(
  void g() {
    // CHECK: call void @_ZN5dtors1SC1Ev(
    // CHECK: call void @_ZN5dtors1SC1Ev(
    auto x = std::initializer_list<S>{ S(), S() };

    // Destruction loop for underlying array.
    // CHECK: br label
    // CHECK: call void @_ZN5dtors1SD1Ev(
    // CHECK: br i1

    // CHECK: call void @_ZN5dtors1zEv(
    z();

    // CHECK-NOT: call void @_ZN5dtors1SD1Ev(
  }

  // CHECK-LABEL: define void @_ZN5dtors1hEv(
  void h() {
    // CHECK: call void @_ZN5dtors1SC1Ev(
    // CHECK: call void @_ZN5dtors1SC1Ev(
    std::initializer_list<S> x = { S(), S() };

    // CHECK-NOT: call void @_ZN5dtors1SD1Ev(

    // CHECK: call void @_ZN5dtors1zEv(
    z();

    // Destruction loop for underlying array.
    // CHECK: br label
    // CHECK: call void @_ZN5dtors1SD1Ev(
    // CHECK: br i1
  }
}

namespace partly_constant {
  int k;
  std::initializer_list<std::initializer_list<int>> &&il = { { 1, 2, 3 }, { 4, k }, { 5, 6, 7, 8 } };
  // First init list.
  // CHECK-NOT: @[[PARTLY_CONSTANT_FIRST]],
  // CHECK: store i32* getelementptr inbounds ({{.*}}* @[[PARTLY_CONSTANT_FIRST]], i64 0, i64 0),
  // CHECK:       i32** getelementptr inbounds ({{.*}}* @[[PARTLY_CONSTANT_INNER]], i64 0, i64 0, i32 0)
  // CHECK: store i64 3, i64* getelementptr inbounds ({{.*}}* @[[PARTLY_CONSTANT_INNER]], i64 0, i64 0, i32 1)
  // CHECK-NOT: @[[PARTLY_CONSTANT_FIRST]],
  //
  // Second init list array (non-constant).
  // CHECK: store i32 4, i32* getelementptr inbounds ({{.*}}* @[[PARTLY_CONSTANT_SECOND]], i64 0, i64 0)
  // CHECK: load i32* @_ZN15partly_constant1kE
  // CHECK: store i32 {{.*}}, i32* getelementptr inbounds ({{.*}}* @[[PARTLY_CONSTANT_SECOND]], i64 0, i64 1)
  //
  // Second init list.
  // CHECK: store i32* getelementptr inbounds ({{.*}}* @[[PARTLY_CONSTANT_SECOND]], i64 0, i64 0),
  // CHECK:       i32** getelementptr inbounds ({{.*}}* @[[PARTLY_CONSTANT_INNER]], i64 0, i64 1, i32 0)
  // CHECK: store i64 2, i64* getelementptr inbounds ({{.*}}* @[[PARTLY_CONSTANT_INNER]], i64 0, i64 1, i32 1)
  //
  // Third init list.
  // CHECK-NOT: @[[PARTLY_CONSTANT_THIRD]],
  // CHECK: store i32* getelementptr inbounds ({{.*}}* @[[PARTLY_CONSTANT_THIRD]], i64 0, i64 0),
  // CHECK:       i32** getelementptr inbounds ({{.*}}* @[[PARTLY_CONSTANT_INNER]], i64 0, i64 2, i32 0)
  // CHECK: store i64 4, i64* getelementptr inbounds ({{.*}}* @_ZGRN15partly_constant2ilE4_, i64 0, i64 2, i32 1)
  // CHECK-NOT: @[[PARTLY_CONSTANT_THIRD]],
  //
  // Outer init list.
  // CHECK: store {{.*}}* getelementptr inbounds ({{.*}}* @[[PARTLY_CONSTANT_INNER]], i64 0, i64 0),
  // CHECK:       {{.*}}** getelementptr inbounds ({{.*}}* @[[PARTLY_CONSTANT_OUTER]], i32 0, i32 0)
  // CHECK: store i64 3, i64* getelementptr inbounds ({{.*}}* @[[PARTLY_CONSTANT_OUTER]], i32 0, i32 1)
  //
  // 'il' reference.
  // CHECK: store {{.*}}* @[[PARTLY_CONSTANT_OUTER]], {{.*}}** @_ZN15partly_constant2ilE, align 8
}

namespace nested {
  struct A { A(); ~A(); };
  struct B { const A &a; ~B(); };
  struct C { std::initializer_list<B> b; ~C(); };
  void f();
  // CHECK-LABEL: define void @_ZN6nested1gEv(
  void g() {
    // CHECK: call void @_ZN6nested1AC1Ev(
    // CHECK-NOT: call
    // CHECK: call void @_ZN6nested1AC1Ev(
    // CHECK-NOT: call
    const C &c { { { A() }, { A() } } };

    // CHECK: call void @_ZN6nested1fEv(
    // CHECK-NOT: call
    f();

    // CHECK: call void @_ZN6nested1CD1Ev(
    // CHECK-NOT: call

    // Destroy B[2] array.
    // FIXME: This isn't technically correct: reverse construction order would
    // destroy the second B then the second A then the first B then the first A.
    // CHECK: call void @_ZN6nested1BD1Ev(
    // CHECK-NOT: call
    // CHECK: br

    // CHECK-NOT: call
    // CHECK: call void @_ZN6nested1AD1Ev(
    // CHECK-NOT: call
    // CHECK: call void @_ZN6nested1AD1Ev(
    // CHECK-NOT: call
    // CHECK: }
  }
}

namespace DR1070 {
  struct A {
    A(std::initializer_list<int>);
  };
  struct B {
    int i;
    A a;
  };
  B b = {1};
  struct C {
    std::initializer_list<int> a;
    B b;
    std::initializer_list<double> c;
  };
  C c = {};
}

namespace ArrayOfInitList {
  struct S {
    S(std::initializer_list<int>);
  };
  S x[1] = {};
}

namespace PR20445 {
  struct vector { vector(std::initializer_list<int>); };
  struct MyClass { explicit MyClass(const vector &v); };
  template<int x> void f() { new MyClass({42, 43}); }
  template void f<0>();
  // CHECK-LABEL: define {{.*}} @_ZN7PR204451fILi0EEEvv(
  // CHECK: call void @_ZN7PR204456vectorC1ESt16initializer_listIiE(
  // CHECK: call void @_ZN7PR204457MyClassC1ERKNS_6vectorE(
}
