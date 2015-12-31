// RUN: %clang_cc1 -verify -fopenmp=libiomp5 -DBODY -triple x86_64-unknown-unknown -x c++ -emit-llvm %s -fexceptions -fcxx-exceptions -o - | FileCheck %s
// RUN: %clang_cc1 -fopenmp=libiomp5 -x c++ -std=c++11 -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -emit-pch -o %t %s
// RUN: %clang_cc1 -fopenmp=libiomp5 -DBODY -x c++ -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -g -std=c++11 -include-pch %t -verify %s -emit-llvm -o - | FileCheck --check-prefix=CHECK-DEBUG %s
// expected-no-diagnostics
#ifndef HEADER
#define HEADER
// CHECK-DAG: [[IDENT:%.+]] = type { i32, i32, i32, i32, i8* }
// CHECK-DAG: [[S1:%.+]] = type { [[INT:i[0-9]+]] }
// CHECK-DAG: [[S2:%.+]] = type { [[INT]], double }
// CHECK-DAG: [[S3:%.+]] = type { [[INT]], float }
// CHECK-DAG: [[S4:%.+]] = type { [[INT]], [[INT]] }
// CHECK-DAG: [[S5:%.+]] = type { [[INT]], [[INT]], [[INT]] }
// CHECK-DAG: [[SMAIN:%.+]] = type { [[INT]], double, double }
// CHECK-DEBUG-DAG: [[IDENT:%.+]] = type { i32, i32, i32, i32, i8* }
// CHECK-DEBUG-DAG: [[S1:%.+]] = type { [[INT:i[0-9]+]] }
// CHECK-DEBUG-DAG: [[S2:%.+]] = type { [[INT]], double }
// CHECK-DEBUG-DAG: [[S3:%.+]] = type { [[INT]], float }
// CHECK-DEBUG-DAG: [[S4:%.+]] = type { [[INT]], [[INT]] }
// CHECK-DEBUG-DAG: [[S5:%.+]] = type { [[INT]], [[INT]], [[INT]] }
// CHECK-DEBUG-DAG: [[SMAIN:%.+]] = type { [[INT]], double, double }

struct S1 {
  int a;
  S1()
      : a(0) {
  }
  S1(int a)
      : a(a) {
  }
  S1(const S1 &s) {
    a = 12 + s.a;
  }
  ~S1() {
    a = 0;
  }
};

struct S2 {
  int a;
  double b;
  S2()
      : a(0) {
  }
  S2(int a)
      : a(a) {
  }
  S2(const S2 &s) {
    a = 12 + s.a;
  }
  ~S2() {
    a = 0;
  }
};

struct S3 {
  int a;
  float b;
  S3()
      : a(0) {
  }
  S3(int a)
      : a(a) {
  }
  S3(const S3 &s) {
    a = 12 + s.a;
  }
  ~S3() {
    a = 0;
  }
};

struct S4 {
  int a, b;
  S4()
      : a(0) {
  }
  S4(int a)
      : a(a) {
  }
  S4(const S4 &s) {
    a = 12 + s.a;
  }
  ~S4() {
    a = 0;
  }
};

struct S5 {
  int a, b, c;
  S5()
      : a(0) {
  }
  S5(int a)
      : a(a) {
  }
  S5(const S5 &s) {
    a = 12 + s.a;
  }
  ~S5() {
    a = 0;
  }
};

// CHECK-DAG:  [[GS1:@.+]] = internal global [[S1]] zeroinitializer
// CHECK-DAG:  [[GS1]].cache. = common global i8** null
// CHECK-DAG:  [[DEFAULT_LOC:@.+]] = private unnamed_addr constant [[IDENT]] { i32 0, i32 2, i32 0, i32 0, i8* getelementptr inbounds ([{{[0-9]+}} x i8]* {{@.+}}, i32 0, i32 0) }
// CHECK-DAG:  [[GS2:@.+]] = internal global [[S2]] zeroinitializer
// CHECK-DAG:  [[ARR_X:@.+]] = global [2 x [3 x [[S1]]]] zeroinitializer
// CHECK-DAG:  [[ARR_X]].cache. = common global i8** null
// CHECK-DAG:  [[SM:@.+]] = internal global [[SMAIN]] zeroinitializer
// CHECK-DAG:  [[SM]].cache. = common global i8** null
// CHECK-DAG:  [[STATIC_S:@.+]] = external global [[S3]]
// CHECK-DAG:  [[STATIC_S]].cache. = common global i8** null
// CHECK-DAG:  [[GS3:@.+]] = external global [[S5]]
// CHECK-DAG:  [[GS3]].cache. = common global i8** null
// CHECK-DAG:  [[ST_INT_ST:@.+]] = linkonce_odr global i32 23
// CHECK-DAG:  [[ST_INT_ST]].cache. = common global i8** null
// CHECK-DAG:  [[ST_FLOAT_ST:@.+]] = linkonce_odr global float 2.300000e+01
// CHECK-DAG:  [[ST_FLOAT_ST]].cache. = common global i8** null
// CHECK-DAG:  [[ST_S4_ST:@.+]] = linkonce_odr global %struct.S4 zeroinitializer
// CHECK-DAG:  [[ST_S4_ST]].cache. = common global i8** null
// CHECK-NOT:  .cache. = common global i8** null
// There is no cache for gs2 - it is not threadprivate. Check that there is only
// 8 caches created (for Static::s, gs1, gs3, arr_x, main::sm, ST<int>::st,
// ST<float>::st, ST<S4>::st)
// CHECK-DEBUG-DAG: [[GS1:@.+]] = internal global [[S1]] zeroinitializer
// CHECK-DEBUG-DAG: [[GS2:@.+]] = internal global [[S2]] zeroinitializer
// CHECK-DEBUG-DAG: [[ARR_X:@.+]] = global [2 x [3 x [[S1]]]] zeroinitializer
// CHECK-DEBUG-DAG: [[SM:@.+]] = internal global [[SMAIN]] zeroinitializer
// CHECK-DEBUG-DAG: [[STATIC_S:@.+]] = external global [[S3]]
// CHECK-DEBUG-DAG: [[GS3:@.+]] = external global [[S5]]
// CHECK-DEBUG-DAG: [[ST_INT_ST:@.+]] = linkonce_odr global i32 23
// CHECK-DEBUG-DAG: [[ST_FLOAT_ST:@.+]] = linkonce_odr global float 2.300000e+01
// CHECK-DEBUG-DAG: [[ST_S4_ST:@.+]] = linkonce_odr global %struct.S4 zeroinitializer
// CHECK-DEBUG-DAG: [[LOC1:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;;162;9;;\00"
// CHECK-DEBUG-DAG: [[LOC2:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;;216;9;;\00"
// CHECK-DEBUG-DAG: [[LOC3:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;;303;19;;\00"
// CHECK-DEBUG-DAG: [[LOC4:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;main;328;9;;\00"
// CHECK-DEBUG-DAG: [[LOC5:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;main;341;9;;\00"
// CHECK-DEBUG-DAG: [[LOC6:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;main;358;10;;\00"
// CHECK-DEBUG-DAG: [[LOC7:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;main;375;10;;\00"
// CHECK-DEBUG-DAG: [[LOC8:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;main;401;10;;\00"
// CHECK-DEBUG-DAG: [[LOC9:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;main;422;10;;\00"
// CHECK-DEBUG-DAG: [[LOC10:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;main;437;10;;\00"
// CHECK-DEBUG-DAG: [[LOC11:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;main;454;27;;\00"
// CHECK-DEBUG-DAG: [[LOC12:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;main;471;10;;\00"
// CHECK-DEBUG-DAG: [[LOC13:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;foobar;550;9;;\00"
// CHECK-DEBUG-DAG: [[LOC14:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;foobar;567;10;;\00"
// CHECK-DEBUG-DAG: [[LOC15:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;foobar;593;10;;\00"
// CHECK-DEBUG-DAG: [[LOC16:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;foobar;614;10;;\00"
// CHECK-DEBUG-DAG: [[LOC17:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;foobar;629;10;;\00"
// CHECK-DEBUG-DAG: [[LOC18:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;foobar;646;27;;\00"
// CHECK-DEBUG-DAG: [[LOC19:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;foobar;663;10;;\00"
// CHECK-DEBUG-DAG: [[LOC20:@.*]] = private unnamed_addr constant [{{[0-9]+}} x i8] c";{{.*}}threadprivate_codegen.cpp;;275;9;;\00"

struct Static {
  static S3 s;
#pragma omp threadprivate(s)
};

static S1 gs1(5);
#pragma omp threadprivate(gs1)
// CHECK:      define {{.*}} [[S1_CTOR:@.*]]([[S1]]* {{.*}},
// CHECK:      define {{.*}} [[S1_DTOR:@.*]]([[S1]]* {{.*}})
// CHECK:      define internal {{.*}}i8* [[GS1_CTOR:@\.__kmpc_global_ctor_\..*]](i8*)
// CHECK:      store i8* %0, i8** [[ARG_ADDR:%.*]],
// CHECK:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK:      [[RES:%.*]] = bitcast i8* [[ARG]] to [[S1]]*
// CHECK-NEXT: call {{.*}} [[S1_CTOR]]([[S1]]* [[RES]], {{.*}} 5)
// CHECK:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK:      ret i8* [[ARG]]
// CHECK-NEXT: }
// CHECK:      define internal {{.*}}void [[GS1_DTOR:@\.__kmpc_global_dtor_\..*]](i8*)
// CHECK:      store i8* %0, i8** [[ARG_ADDR:%.*]],
// CHECK:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK:      [[RES:%.*]] = bitcast i8* [[ARG]] to [[S1]]*
// CHECK-NEXT: call {{.*}} [[S1_DTOR]]([[S1]]* [[RES]])
// CHECK-NEXT: ret void
// CHECK-NEXT: }
// CHECK:      define internal {{.*}}void [[GS1_INIT:@\.__omp_threadprivate_init_\..*]]()
// CHECK:      call {{.*}}void @__kmpc_threadprivate_register([[IDENT]]* [[DEFAULT_LOC]], i8* bitcast ([[S1]]* [[GS1]] to i8*), i8* (i8*)* [[GS1_CTOR]], i8* (i8*, i8*)* null, void (i8*)* [[GS1_DTOR]])
// CHECK-NEXT: ret void
// CHECK-NEXT: }
// CHECK-DEBUG:      [[KMPC_LOC_ADDR:%.*]] = alloca [[IDENT]]
// CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
// CHECK-DEBUG:      store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC1]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
// CHECK-DEBUG:      @__kmpc_global_thread_num
// CHECK-DEBUG:      call {{.*}}void @__kmpc_threadprivate_register([[IDENT]]* [[KMPC_LOC_ADDR]], i8* bitcast ([[S1]]* [[GS1]] to i8*), i8* (i8*)* [[GS1_CTOR:@\.__kmpc_global_ctor_\..*]], i8* (i8*, i8*)* null, void (i8*)* [[GS1_DTOR:@\.__kmpc_global_dtor_\..*]])
// CHECK-DEBUG:      define internal {{.*}}i8* [[GS1_CTOR]](i8*)
// CHECK-DEBUG:      store i8* %0, i8** [[ARG_ADDR:%.*]],
// CHECK-DEBUG:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK-DEBUG:      [[RES:%.*]] = bitcast i8* [[ARG]] to [[S1]]*
// CHECK-DEBUG-NEXT: call {{.*}} [[S1_CTOR:@.+]]([[S1]]* [[RES]], {{.*}} 5)
// CHECK-DEBUG:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK-DEBUG:      ret i8* [[ARG]]
// CHECK-DEBUG-NEXT: }
// CHECK-DEBUG:      define {{.*}} [[S1_CTOR]]([[S1]]* {{.*}},
// CHECK-DEBUG:      define internal {{.*}}void [[GS1_DTOR]](i8*)
// CHECK-DEBUG:      store i8* %0, i8** [[ARG_ADDR:%.*]],
// CHECK-DEBUG:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK-DEBUG:      [[RES:%.*]] = bitcast i8* [[ARG]] to [[S1]]*
// CHECK-DEBUG-NEXT: call {{.*}} [[S1_DTOR:@.+]]([[S1]]* [[RES]])
// CHECK-DEBUG-NEXT: ret void
// CHECK-DEBUG-NEXT: }
// CHECK-DEBUG:      define {{.*}} [[S1_DTOR]]([[S1]]* {{.*}})
static S2 gs2(27);
// CHECK:      define {{.*}} [[S2_CTOR:@.*]]([[S2]]* {{.*}},
// CHECK:      define {{.*}} [[S2_DTOR:@.*]]([[S2]]* {{.*}})
// No another call for S2 constructor because it is not threadprivate
// CHECK-NOT:  call {{.*}} [[S2_CTOR]]([[S2]]*
// CHECK-DEBUG:      define {{.*}} [[S2_CTOR:@.*]]([[S2]]* {{.*}},
// CHECK-DEBUG:      define {{.*}} [[S2_DTOR:@.*]]([[S2]]* {{.*}})
// No another call for S2 constructor because it is not threadprivate
// CHECK-DEBUG-NOT:  call {{.*}} [[S2_CTOR]]([[S2]]*
S1 arr_x[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
#pragma omp threadprivate(arr_x)
// CHECK:      define internal {{.*}}i8* [[ARR_X_CTOR:@\.__kmpc_global_ctor_\..*]](i8*)
// CHECK:      store i8* %0, i8** [[ARG_ADDR:%.*]],
// CHECK:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK:      [[RES:%.*]] = bitcast i8* [[ARG]] to [2 x [3 x [[S1]]]]*
// CHECK:      [[ARR1:%.*]] = getelementptr inbounds [2 x [3 x [[S1]]]]* [[RES]], i{{.*}} 0, i{{.*}} 0
// CHECK:      [[ARR:%.*]] = getelementptr inbounds [3 x [[S1]]]* [[ARR1]], i{{.*}} 0, i{{.*}} 0
// CHECK:      invoke {{.*}} [[S1_CTOR]]([[S1]]* [[ARR]], [[INT]] {{.*}}1)
// CHECK:      [[ARR_ELEMENT:%.*]] = getelementptr inbounds [[S1]]* [[ARR]], i{{.*}} 1
// CHECK:      invoke {{.*}} [[S1_CTOR]]([[S1]]* [[ARR_ELEMENT]], [[INT]] {{.*}}2)
// CHECK:      [[ARR_ELEMENT2:%.*]] = getelementptr inbounds [[S1]]* [[ARR_ELEMENT]], i{{.*}} 1
// CHECK:      invoke {{.*}} [[S1_CTOR]]([[S1]]* [[ARR_ELEMENT2]], [[INT]] {{.*}}3)
// CHECK:      [[ARR_ELEMENT3:%.*]] = getelementptr inbounds [3 x [[S1]]]* [[ARR1]], i{{.*}} 1
// CHECK:      [[ARR_:%.*]] = getelementptr inbounds [3 x [[S1]]]* [[ARR_ELEMENT3]], i{{.*}} 0, i{{.*}} 0
// CHECK:      invoke {{.*}} [[S1_CTOR]]([[S1]]* [[ARR_]], [[INT]] {{.*}}4)
// CHECK:      [[ARR_ELEMENT:%.*]] = getelementptr inbounds [[S1]]* [[ARR_]], i{{.*}} 1
// CHECK:      invoke {{.*}} [[S1_CTOR]]([[S1]]* [[ARR_ELEMENT]], [[INT]] {{.*}}5)
// CHECK:      [[ARR_ELEMENT2:%.*]] = getelementptr inbounds [[S1]]* [[ARR_ELEMENT]], i{{.*}} 1
// CHECK:      invoke {{.*}} [[S1_CTOR]]([[S1]]* [[ARR_ELEMENT2]], [[INT]] {{.*}}6)
// CHECK:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK:      ret i8* [[ARG]]
// CHECK:      }
// CHECK:      define internal {{.*}}void [[ARR_X_DTOR:@\.__kmpc_global_dtor_\..*]](i8*)
// CHECK:      store i8* %0, i8** [[ARG_ADDR:%.*]],
// CHECK:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK:      [[ARR_BEGIN:%.*]] = bitcast i8* [[ARG]] to [[S1]]*
// CHECK-NEXT: [[ARR_CUR:%.*]] = getelementptr inbounds [[S1]]* [[ARR_BEGIN]], i{{.*}} 6
// CHECK-NEXT: br label %[[ARR_LOOP:.*]]
// CHECK:      {{.*}}[[ARR_LOOP]]{{.*}}
// CHECK-NEXT: [[ARR_ELEMENTPAST:%.*]] = phi [[S1]]* [ [[ARR_CUR]], {{.*}} ], [ [[ARR_ELEMENT:%.*]], {{.*}} ]
// CHECK-NEXT: [[ARR_ELEMENT:%.*]] = getelementptr inbounds [[S1]]* [[ARR_ELEMENTPAST]], i{{.*}} -1
// CHECK-NEXT: invoke {{.*}} [[S1_DTOR]]([[S1]]* [[ARR_ELEMENT]])
// CHECK:      [[ARR_DONE:%.*]] = icmp eq [[S1]]* [[ARR_ELEMENT]], [[ARR_BEGIN]]
// CHECK-NEXT: br i1 [[ARR_DONE]], label %[[ARR_EXIT:.*]], label %[[ARR_LOOP]]
// CHECK:      {{.*}}[[ARR_EXIT]]{{.*}}
// CHECK-NEXT: ret void
// CHECK:      }
// CHECK:      define internal {{.*}}void [[ARR_X_INIT:@\.__omp_threadprivate_init_\..*]]()
// CHECK:      call {{.*}}void @__kmpc_threadprivate_register([[IDENT]]* [[DEFAULT_LOC]], i8* bitcast ([2 x [3 x [[S1]]]]* [[ARR_X]] to i8*), i8* (i8*)* [[ARR_X_CTOR]], i8* (i8*, i8*)* null, void (i8*)* [[ARR_X_DTOR]])
// CHECK-NEXT: ret void
// CHECK-NEXT: }
// CHECK-DEBUG:      [[KMPC_LOC_ADDR:%.*]] = alloca [[IDENT]]
// CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
// CHECK-DEBUG:      store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC2]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
// CHECK-DEBUG:      @__kmpc_global_thread_num
// CHECK-DEBUG:      call {{.*}}void @__kmpc_threadprivate_register([[IDENT]]* [[KMPC_LOC_ADDR]], i8* bitcast ([2 x [3 x [[S1]]]]* [[ARR_X]] to i8*), i8* (i8*)* [[ARR_X_CTOR:@\.__kmpc_global_ctor_\..*]], i8* (i8*, i8*)* null, void (i8*)* [[ARR_X_DTOR:@\.__kmpc_global_dtor_\..*]])
// CHECK-DEBUG:      define internal {{.*}}i8* [[ARR_X_CTOR]](i8*)
// CHECK-DEBUG:      }
// CHECK-DEBUG:      define internal {{.*}}void [[ARR_X_DTOR]](i8*)
// CHECK-DEBUG:      }
extern S5 gs3;
#pragma omp threadprivate(gs3)
// No call for S5 constructor because gs3 has just declaration, not a definition.
// CHECK-NOT:  call {{.*}}([[S5]]*
// CHECK-DEBUG-NOT:  call {{.*}}([[S5]]*

template <class T>
struct ST {
  static T st;
#pragma omp threadprivate(st)
};

template <class T>
T ST<T>::st(23);

// CHECK-LABEL:  @main()
// CHECK-DEBUG-LABEL: @main()
int main() {
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR:%.*]] = alloca [[IDENT]]
  int Res;
  struct Smain {
    int a;
    double b, c;
    Smain()
        : a(0) {
    }
    Smain(int a)
        : a(a) {
    }
    Smain(const Smain &s) {
      a = 12 + s.a;
    }
    ~Smain() {
      a = 0;
    }
  };

  static Smain sm(gs1.a);
// CHECK:      [[THREAD_NUM:%.+]] = call {{.*}}i32 @__kmpc_global_thread_num([[IDENT]]* [[DEFAULT_LOC]])
// CHECK:      call {{.*}}i{{.*}} @__cxa_guard_acquire
// CHECK:      call {{.*}}i32 @__kmpc_global_thread_num([[IDENT]]* [[DEFAULT_LOC]])
// CHECK:      call {{.*}}void @__kmpc_threadprivate_register([[IDENT]]* [[DEFAULT_LOC]], i8* bitcast ([[SMAIN]]* [[SM]] to i8*), i8* (i8*)* [[SM_CTOR:@\.__kmpc_global_ctor_\..+]], i8* (i8*, i8*)* null, void (i8*)* [[SM_DTOR:@\.__kmpc_global_dtor_\..+]])
// CHECK:      [[GS1_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([[S1]]* [[GS1]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[GS1]].cache.)
// CHECK-NEXT: [[GS1_ADDR:%.*]] = bitcast i8* [[GS1_TEMP_ADDR]] to [[S1]]*
// CHECK-NEXT: [[GS1_A_ADDR:%.*]] = getelementptr inbounds [[S1]]* [[GS1_ADDR]], i{{.*}} 0, i{{.*}} 0
// CHECK-NEXT: [[GS1_A:%.*]] = load [[INT]]* [[GS1_A_ADDR]]
// CHECK-NEXT: invoke {{.*}} [[SMAIN_CTOR:.*]]([[SMAIN]]* [[SM]], [[INT]] {{.*}}[[GS1_A]])
// CHECK:      call {{.*}}void @__cxa_guard_release
// CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
// CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC3]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
// CHECK-DEBUG-NEXT: [[THREAD_NUM:%.+]] = call {{.*}}i32 @__kmpc_global_thread_num([[IDENT]]* [[KMPC_LOC_ADDR]])
// CHECK-DEBUG:      call {{.*}}i{{.*}} @__cxa_guard_acquire
// CHECK-DEBUG:      call {{.*}}i32 @__kmpc_global_thread_num([[IDENT]]* [[KMPC_LOC_ADDR]])
// CHECK-DEBUG:      call {{.*}}void @__kmpc_threadprivate_register([[IDENT]]* [[KMPC_LOC_ADDR]], i8* bitcast ([[SMAIN]]* [[SM]] to i8*), i8* (i8*)* [[SM_CTOR:@\.__kmpc_global_ctor_\..+]], i8* (i8*, i8*)* null, void (i8*)* [[SM_DTOR:@\.__kmpc_global_dtor_\..+]])
// CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
// CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC3]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
// CHECK-DEBUG:      [[GS1_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([[S1]]* [[GS1]] to i8*), i{{.*}} {{[0-9]+}}, i8***
// CHECK-DEBUG-NEXT: [[GS1_ADDR:%.*]] = bitcast i8* [[GS1_TEMP_ADDR]] to [[S1]]*
// CHECK-DEBUG-NEXT: [[GS1_A_ADDR:%.*]] = getelementptr inbounds [[S1]]* [[GS1_ADDR]], i{{.*}} 0, i{{.*}} 0
// CHECK-DEBUG-NEXT: [[GS1_A:%.*]] = load [[INT]]* [[GS1_A_ADDR]]
// CHECK-DEBUG-NEXT: invoke {{.*}} [[SMAIN_CTOR:.*]]([[SMAIN]]* [[SM]], [[INT]] {{.*}}[[GS1_A]])
// CHECK-DEBUG:      call {{.*}}void @__cxa_guard_release
#pragma omp threadprivate(sm)
  // CHECK:      [[STATIC_S_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([[S3]]* [[STATIC_S]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[STATIC_S]].cache.)
  // CHECK-NEXT: [[STATIC_S_ADDR:%.*]] = bitcast i8* [[STATIC_S_TEMP_ADDR]] to [[S3]]*
  // CHECK-NEXT: [[STATIC_S_A_ADDR:%.*]] = getelementptr inbounds [[S3]]* [[STATIC_S_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-NEXT: [[STATIC_S_A:%.*]] = load [[INT]]* [[STATIC_S_A_ADDR]]
  // CHECK-NEXT: store [[INT]] [[STATIC_S_A]], [[INT]]* [[RES_ADDR:[^,]+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC5]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[STATIC_S_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([[S3]]* [[STATIC_S]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[STATIC_S_ADDR:%.*]] = bitcast i8* [[STATIC_S_TEMP_ADDR]] to [[S3]]*
  // CHECK-DEBUG-NEXT: [[STATIC_S_A_ADDR:%.*]] = getelementptr inbounds [[S3]]* [[STATIC_S_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-DEBUG-NEXT: [[STATIC_S_A:%.*]] = load [[INT]]* [[STATIC_S_A_ADDR]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[STATIC_S_A]], [[INT]]* [[RES_ADDR:[^,]+]]
  Res = Static::s.a;
  // CHECK:      [[SM_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([[SMAIN]]* [[SM]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[SM]].cache.)
  // CHECK-NEXT: [[SM_ADDR:%.*]] = bitcast i8* [[SM_TEMP_ADDR]] to [[SMAIN]]*
  // CHECK-NEXT: [[SM_A_ADDR:%.*]] = getelementptr inbounds [[SMAIN]]* [[SM_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-NEXT: [[SM_A:%.*]] = load [[INT]]* [[SM_A_ADDR]]
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[SM_A]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC6]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[SM_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([[SMAIN]]* [[SM]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[SM_ADDR:%.*]] = bitcast i8* [[SM_TEMP_ADDR]] to [[SMAIN]]*
  // CHECK-DEBUG-NEXT: [[SM_A_ADDR:%.*]] = getelementptr inbounds [[SMAIN]]* [[SM_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-DEBUG-NEXT: [[SM_A:%.*]] = load [[INT]]* [[SM_A_ADDR]]
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[SM_A]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += sm.a;
  // CHECK:      [[GS1_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([[S1]]* [[GS1]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[GS1]].cache.)
  // CHECK-NEXT: [[GS1_ADDR:%.*]] = bitcast i8* [[GS1_TEMP_ADDR]] to [[S1]]*
  // CHECK-NEXT: [[GS1_A_ADDR:%.*]] = getelementptr inbounds [[S1]]* [[GS1_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-NEXT: [[GS1_A:%.*]] = load [[INT]]* [[GS1_A_ADDR]]
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[GS1_A]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC7]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[GS1_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([[S1]]* [[GS1]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[GS1_ADDR:%.*]] = bitcast i8* [[GS1_TEMP_ADDR]] to [[S1]]*
  // CHECK-DEBUG-NEXT: [[GS1_A_ADDR:%.*]] = getelementptr inbounds [[S1]]* [[GS1_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-DEBUG-NEXT: [[GS1_A:%.*]] = load [[INT]]* [[GS1_A_ADDR]]
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[GS1_A]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += gs1.a;
  // CHECK:      [[GS2_A:%.*]] = load [[INT]]* getelementptr inbounds ([[S2]]* [[GS2]], i{{.*}} 0, i{{.*}} 0)
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[GS2_A]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[GS2_A:%.*]] = load [[INT]]* getelementptr inbounds ([[S2]]* [[GS2]], i{{.*}} 0, i{{.*}} 0)
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[GS2_A]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += gs2.a;
  // CHECK:      [[GS3_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([[S5]]* [[GS3]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[GS3]].cache.)
  // CHECK-NEXT: [[GS3_ADDR:%.*]] = bitcast i8* [[GS3_TEMP_ADDR]] to [[S5]]*
  // CHECK-NEXT: [[GS3_A_ADDR:%.*]] = getelementptr inbounds [[S5]]* [[GS3_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-NEXT: [[GS3_A:%.*]] = load [[INT]]* [[GS3_A_ADDR]]
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[GS3_A]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC8]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[GS3_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([[S5]]* [[GS3]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[GS3_ADDR:%.*]] = bitcast i8* [[GS3_TEMP_ADDR]] to [[S5]]*
  // CHECK-DEBUG-NEXT: [[GS3_A_ADDR:%.*]] = getelementptr inbounds [[S5]]* [[GS3_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-DEBUG-NEXT: [[GS3_A:%.*]] = load [[INT]]* [[GS3_A_ADDR]]
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[GS3_A]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += gs3.a;
  // CHECK:      [[ARR_X_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([2 x [3 x [[S1]]]]* [[ARR_X]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[ARR_X]].cache.)
  // CHECK-NEXT: [[ARR_X_ADDR:%.*]] = bitcast i8* [[ARR_X_TEMP_ADDR]] to [2 x [3 x [[S1]]]]*
  // CHECK-NEXT: [[ARR_X_1_ADDR:%.*]] = getelementptr inbounds [2 x [3 x [[S1]]]]* [[ARR_X_ADDR]], i{{.*}} 0, i{{.*}} 1
  // CHECK-NEXT: [[ARR_X_1_1_ADDR:%.*]] = getelementptr inbounds [3 x [[S1]]]* [[ARR_X_1_ADDR]], i{{.*}} 0, i{{.*}} 1
  // CHECK-NEXT: [[ARR_X_1_1_A_ADDR:%.*]] = getelementptr inbounds [[S1]]* [[ARR_X_1_1_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-NEXT: [[ARR_X_1_1_A:%.*]] = load [[INT]]* [[ARR_X_1_1_A_ADDR]]
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[ARR_X_1_1_A]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC9]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT:      [[ARR_X_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([2 x [3 x [[S1]]]]* [[ARR_X]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[ARR_X_ADDR:%.*]] = bitcast i8* [[ARR_X_TEMP_ADDR]] to [2 x [3 x [[S1]]]]*
  // CHECK-DEBUG-NEXT: [[ARR_X_1_ADDR:%.*]] = getelementptr inbounds [2 x [3 x [[S1]]]]* [[ARR_X_ADDR]], i{{.*}} 0, i{{.*}} 1
  // CHECK-DEBUG-NEXT: [[ARR_X_1_1_ADDR:%.*]] = getelementptr inbounds [3 x [[S1]]]* [[ARR_X_1_ADDR]], i{{.*}} 0, i{{.*}} 1
  // CHECK-DEBUG-NEXT: [[ARR_X_1_1_A_ADDR:%.*]] = getelementptr inbounds [[S1]]* [[ARR_X_1_1_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-DEBUG-NEXT: [[ARR_X_1_1_A:%.*]] = load [[INT]]* [[ARR_X_1_1_A_ADDR]]
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[ARR_X_1_1_A]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += arr_x[1][1].a;
  // CHECK:      [[ST_INT_ST_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([[INT]]* [[ST_INT_ST]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[ST_INT_ST]].cache.)
  // CHECK-NEXT: [[ST_INT_ST_ADDR:%.*]] = bitcast i8* [[ST_INT_ST_TEMP_ADDR]] to [[INT]]*
  // CHECK-NEXT: [[ST_INT_ST_VAL:%.*]] = load [[INT]]* [[ST_INT_ST_ADDR]]
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[ST_INT_ST_VAL]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC10]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[ST_INT_ST_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([[INT]]* [[ST_INT_ST]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[ST_INT_ST_ADDR:%.*]] = bitcast i8* [[ST_INT_ST_TEMP_ADDR]] to [[INT]]*
  // CHECK-DEBUG-NEXT: [[ST_INT_ST_VAL:%.*]] = load [[INT]]* [[ST_INT_ST_ADDR]]
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[ST_INT_ST_VAL]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += ST<int>::st;
  // CHECK:      [[ST_FLOAT_ST_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast (float* [[ST_FLOAT_ST]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[ST_FLOAT_ST]].cache.)
  // CHECK-NEXT: [[ST_FLOAT_ST_ADDR:%.*]] = bitcast i8* [[ST_FLOAT_ST_TEMP_ADDR]] to float*
  // CHECK-NEXT: [[ST_FLOAT_ST_VAL:%.*]] = load float* [[ST_FLOAT_ST_ADDR]]
  // CHECK-NEXT: [[FLOAT_TO_INT_CONV:%.*]] = fptosi float [[ST_FLOAT_ST_VAL]] to [[INT]]
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[FLOAT_TO_INT_CONV]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC11]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[ST_FLOAT_ST_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast (float* [[ST_FLOAT_ST]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[ST_FLOAT_ST_ADDR:%.*]] = bitcast i8* [[ST_FLOAT_ST_TEMP_ADDR]] to float*
  // CHECK-DEBUG-NEXT: [[ST_FLOAT_ST_VAL:%.*]] = load float* [[ST_FLOAT_ST_ADDR]]
  // CHECK-DEBUG-NEXT: [[FLOAT_TO_INT_CONV:%.*]] = fptosi float [[ST_FLOAT_ST_VAL]] to [[INT]]
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[FLOAT_TO_INT_CONV]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += static_cast<int>(ST<float>::st);
  // CHECK:      [[ST_S4_ST_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([[S4]]* [[ST_S4_ST]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[ST_S4_ST]].cache.)
  // CHECK-NEXT: [[ST_S4_ST_ADDR:%.*]] = bitcast i8* [[ST_S4_ST_TEMP_ADDR]] to [[S4]]*
  // CHECK-NEXT: [[ST_S4_ST_A_ADDR:%.*]] = getelementptr inbounds [[S4]]* [[ST_S4_ST_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-NEXT: [[ST_S4_ST_A:%.*]] = load [[INT]]* [[ST_S4_ST_A_ADDR]]
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[ST_S4_ST_A]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC12]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[ST_S4_ST_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([[S4]]* [[ST_S4_ST]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[ST_S4_ST_ADDR:%.*]] = bitcast i8* [[ST_S4_ST_TEMP_ADDR]] to [[S4]]*
  // CHECK-DEBUG-NEXT: [[ST_S4_ST_A_ADDR:%.*]] = getelementptr inbounds [[S4]]* [[ST_S4_ST_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-DEBUG-NEXT: [[ST_S4_ST_A:%.*]] = load [[INT]]* [[ST_S4_ST_A_ADDR]]
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[ST_S4_ST_A]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += ST<S4>::st.a;
  // CHECK:      [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: ret [[INT]] [[RES]]
  // CHECK-DEBUG:      [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: ret [[INT]] [[RES]]
  return Res;
}
// CHECK: }

// CHECK:      define internal {{.*}}i8* [[SM_CTOR]](i8*)
// CHECK:      [[THREAD_NUM:%.+]] = call {{.*}}i32 @__kmpc_global_thread_num([[IDENT]]* [[DEFAULT_LOC]])
// CHECK:      store i8* %0, i8** [[ARG_ADDR:%.*]],
// CHECK:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK:      [[RES:%.*]] = bitcast i8* [[ARG]] to [[SMAIN]]*
// CHECK:      [[GS1_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([[S1]]* [[GS1]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[GS1]].cache.)
// CHECK-NEXT: [[GS1_ADDR:%.*]] = bitcast i8* [[GS1_TEMP_ADDR]] to [[S1]]*
// CHECK-NEXT: [[GS1_A_ADDR:%.*]] = getelementptr inbounds [[S1]]* [[GS1_ADDR]], i{{.*}} 0, i{{.*}} 0
// CHECK-NEXT: [[GS1_A:%.*]] = load [[INT]]* [[GS1_A_ADDR]]
// CHECK-NEXT: call {{.*}} [[SMAIN_CTOR:@.+]]([[SMAIN]]* [[RES]], [[INT]] {{.*}}[[GS1_A]])
// CHECK:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK-NEXT: ret i8* [[ARG]]
// CHECK-NEXT: }
// CHECK:      define {{.*}} [[SMAIN_CTOR]]([[SMAIN]]* {{.*}},
// CHECK:      define internal {{.*}}void [[SM_DTOR]](i8*)
// CHECK:      store i8* %0, i8** [[ARG_ADDR:%.*]],
// CHECK:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK:      [[RES:%.*]] = bitcast i8* [[ARG]] to [[SMAIN]]*
// CHECK-NEXT: call {{.*}} [[SMAIN_DTOR:@.+]]([[SMAIN]]* [[RES]])
// CHECK-NEXT: ret void
// CHECK-NEXT: }
// CHECK:      define {{.*}} [[SMAIN_DTOR]]([[SMAIN]]* {{.*}})
// CHECK-DEBUG:      define internal {{.*}}i8* [[SM_CTOR]](i8*)
// CHECK-DEBUG:      [[KMPC_LOC_ADDR:%.*]] = alloca [[IDENT]]
// CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
// CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC3]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
// CHECK-DEBUG-NEXT: [[THREAD_NUM:%.+]] = call {{.*}}i32 @__kmpc_global_thread_num([[IDENT]]* [[KMPC_LOC_ADDR]])
// CHECK-DEBUG:      store i8* %0, i8** [[ARG_ADDR:%.*]],
// CHECK-DEBUG:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK-DEBUG:      [[RES:%.*]] = bitcast i8* [[ARG]] to [[SMAIN]]*
// CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
// CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC3]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
// CHECK-DEBUG:      [[GS1_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([[S1]]* [[GS1]] to i8*), i{{.*}} {{[0-9]+}}, i8***
// CHECK-DEBUG-NEXT: [[GS1_ADDR:%.*]] = bitcast i8* [[GS1_TEMP_ADDR]] to [[S1]]*
// CHECK-DEBUG-NEXT: [[GS1_A_ADDR:%.*]] = getelementptr inbounds [[S1]]* [[GS1_ADDR]], i{{.*}} 0, i{{.*}} 0
// CHECK-DEBUG-NEXT: [[GS1_A:%.*]] = load [[INT]]* [[GS1_A_ADDR]]
// CHECK-DEBUG-NEXT: call {{.*}} [[SMAIN_CTOR:@.+]]([[SMAIN]]* [[RES]], [[INT]] {{.*}}[[GS1_A]])
// CHECK-DEBUG:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK-DEBUG-NEXT: ret i8* [[ARG]]
// CHECK-DEBUG-NEXT: }
// CHECK-DEBUG:      define {{.*}} [[SMAIN_CTOR]]([[SMAIN]]* {{.*}},
// CHECK-DEBUG:      define internal {{.*}} [[SM_DTOR:@.+]](i8*)
// CHECK-DEBUG:      call {{.*}} [[SMAIN_DTOR:@.+]]([[SMAIN]]*
// CHECK-DEBUG:      }
// CHECK-DEBUG:      define {{.*}} [[SMAIN_DTOR]]([[SMAIN]]* {{.*}})

#endif

#ifdef BODY
// CHECK-LABEL:  @{{.*}}foobar{{.*}}()
// CHECK-DEBUG-LABEL: @{{.*}}foobar{{.*}}()
int foobar() {
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR:%.*]] = alloca [[IDENT]]
  int Res;
  // CHECK:      [[THREAD_NUM:%.+]] = call {{.*}}i32 @__kmpc_global_thread_num([[IDENT]]* [[DEFAULT_LOC]])
  // CHECK:      [[STATIC_S_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([[S3]]* [[STATIC_S]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[STATIC_S]].cache.)
  // CHECK-NEXT: [[STATIC_S_ADDR:%.*]] = bitcast i8* [[STATIC_S_TEMP_ADDR]] to [[S3]]*
  // CHECK-NEXT: [[STATIC_S_A_ADDR:%.*]] = getelementptr inbounds [[S3]]* [[STATIC_S_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-NEXT: [[STATIC_S_A:%.*]] = load [[INT]]* [[STATIC_S_A_ADDR]]
  // CHECK-NEXT: store [[INT]] [[STATIC_S_A]], [[INT]]* [[RES_ADDR:[^,]+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC13]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[THREAD_NUM:%.+]] = call {{.*}}i32 @__kmpc_global_thread_num([[IDENT]]* [[KMPC_LOC_ADDR]])
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC13]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[STATIC_S_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([[S3]]* [[STATIC_S]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[STATIC_S_ADDR:%.*]] = bitcast i8* [[STATIC_S_TEMP_ADDR]] to [[S3]]*
  // CHECK-DEBUG-NEXT: [[STATIC_S_A_ADDR:%.*]] = getelementptr inbounds [[S3]]* [[STATIC_S_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-DEBUG-NEXT: [[STATIC_S_A:%.*]] = load [[INT]]* [[STATIC_S_A_ADDR]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[STATIC_S_A]], [[INT]]* [[RES_ADDR:[^,]+]]
  Res = Static::s.a;
  // CHECK:      [[GS1_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([[S1]]* [[GS1]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[GS1]].cache.)
  // CHECK-NEXT: [[GS1_ADDR:%.*]] = bitcast i8* [[GS1_TEMP_ADDR]] to [[S1]]*
  // CHECK-NEXT: [[GS1_A_ADDR:%.*]] = getelementptr inbounds [[S1]]* [[GS1_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-NEXT: [[GS1_A:%.*]] = load [[INT]]* [[GS1_A_ADDR]]
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[GS1_A]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC14]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[GS1_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([[S1]]* [[GS1]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[GS1_ADDR:%.*]] = bitcast i8* [[GS1_TEMP_ADDR]] to [[S1]]*
  // CHECK-DEBUG-NEXT: [[GS1_A_ADDR:%.*]] = getelementptr inbounds [[S1]]* [[GS1_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-DEBUG-NEXT: [[GS1_A:%.*]] = load [[INT]]* [[GS1_A_ADDR]]
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[GS1_A]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += gs1.a;
  // CHECK:      [[GS2_A:%.*]] = load [[INT]]* getelementptr inbounds ([[S2]]* [[GS2]], i{{.*}} 0, i{{.*}} 0)
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[GS2_A]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[GS2_A:%.*]] = load [[INT]]* getelementptr inbounds ([[S2]]* [[GS2]], i{{.*}} 0, i{{.*}} 0)
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[GS2_A]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += gs2.a;
  // CHECK:      [[GS3_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([[S5]]* [[GS3]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[GS3]].cache.)
  // CHECK-NEXT: [[GS3_ADDR:%.*]] = bitcast i8* [[GS3_TEMP_ADDR]] to [[S5]]*
  // CHECK-NEXT: [[GS3_A_ADDR:%.*]] = getelementptr inbounds [[S5]]* [[GS3_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-NEXT: [[GS3_A:%.*]] = load [[INT]]* [[GS3_A_ADDR]]
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[GS3_A]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC15]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[GS3_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([[S5]]* [[GS3]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[GS3_ADDR:%.*]] = bitcast i8* [[GS3_TEMP_ADDR]] to [[S5]]*
  // CHECK-DEBUG-NEXT: [[GS3_A_ADDR:%.*]] = getelementptr inbounds [[S5]]* [[GS3_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-DEBUG-NEXT: [[GS3_A:%.*]] = load [[INT]]* [[GS3_A_ADDR]]
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[GS3_A]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += gs3.a;
  // CHECK:      [[ARR_X_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([2 x [3 x [[S1]]]]* [[ARR_X]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[ARR_X]].cache.)
  // CHECK-NEXT: [[ARR_X_ADDR:%.*]] = bitcast i8* [[ARR_X_TEMP_ADDR]] to [2 x [3 x [[S1]]]]*
  // CHECK-NEXT: [[ARR_X_1_ADDR:%.*]] = getelementptr inbounds [2 x [3 x [[S1]]]]* [[ARR_X_ADDR]], i{{.*}} 0, i{{.*}} 1
  // CHECK-NEXT: [[ARR_X_1_1_ADDR:%.*]] = getelementptr inbounds [3 x [[S1]]]* [[ARR_X_1_ADDR]], i{{.*}} 0, i{{.*}} 1
  // CHECK-NEXT: [[ARR_X_1_1_A_ADDR:%.*]] = getelementptr inbounds [[S1]]* [[ARR_X_1_1_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-NEXT: [[ARR_X_1_1_A:%.*]] = load [[INT]]* [[ARR_X_1_1_A_ADDR]]
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[ARR_X_1_1_A]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC16]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT:      [[ARR_X_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([2 x [3 x [[S1]]]]* [[ARR_X]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[ARR_X_ADDR:%.*]] = bitcast i8* [[ARR_X_TEMP_ADDR]] to [2 x [3 x [[S1]]]]*
  // CHECK-DEBUG-NEXT: [[ARR_X_1_ADDR:%.*]] = getelementptr inbounds [2 x [3 x [[S1]]]]* [[ARR_X_ADDR]], i{{.*}} 0, i{{.*}} 1
  // CHECK-DEBUG-NEXT: [[ARR_X_1_1_ADDR:%.*]] = getelementptr inbounds [3 x [[S1]]]* [[ARR_X_1_ADDR]], i{{.*}} 0, i{{.*}} 1
  // CHECK-DEBUG-NEXT: [[ARR_X_1_1_A_ADDR:%.*]] = getelementptr inbounds [[S1]]* [[ARR_X_1_1_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-DEBUG-NEXT: [[ARR_X_1_1_A:%.*]] = load [[INT]]* [[ARR_X_1_1_A_ADDR]]
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[ARR_X_1_1_A]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += arr_x[1][1].a;
  // CHECK:      [[ST_INT_ST_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([[INT]]* [[ST_INT_ST]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[ST_INT_ST]].cache.)
  // CHECK-NEXT: [[ST_INT_ST_ADDR:%.*]] = bitcast i8* [[ST_INT_ST_TEMP_ADDR]] to [[INT]]*
  // CHECK-NEXT: [[ST_INT_ST_VAL:%.*]] = load [[INT]]* [[ST_INT_ST_ADDR]]
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[ST_INT_ST_VAL]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC17]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[ST_INT_ST_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([[INT]]* [[ST_INT_ST]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[ST_INT_ST_ADDR:%.*]] = bitcast i8* [[ST_INT_ST_TEMP_ADDR]] to [[INT]]*
  // CHECK-DEBUG-NEXT: [[ST_INT_ST_VAL:%.*]] = load [[INT]]* [[ST_INT_ST_ADDR]]
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[ST_INT_ST_VAL]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += ST<int>::st;
  // CHECK:      [[ST_FLOAT_ST_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast (float* [[ST_FLOAT_ST]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[ST_FLOAT_ST]].cache.)
  // CHECK-NEXT: [[ST_FLOAT_ST_ADDR:%.*]] = bitcast i8* [[ST_FLOAT_ST_TEMP_ADDR]] to float*
  // CHECK-NEXT: [[ST_FLOAT_ST_VAL:%.*]] = load float* [[ST_FLOAT_ST_ADDR]]
  // CHECK-NEXT: [[FLOAT_TO_INT_CONV:%.*]] = fptosi float [[ST_FLOAT_ST_VAL]] to [[INT]]
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[FLOAT_TO_INT_CONV]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC18]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[ST_FLOAT_ST_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast (float* [[ST_FLOAT_ST]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[ST_FLOAT_ST_ADDR:%.*]] = bitcast i8* [[ST_FLOAT_ST_TEMP_ADDR]] to float*
  // CHECK-DEBUG-NEXT: [[ST_FLOAT_ST_VAL:%.*]] = load float* [[ST_FLOAT_ST_ADDR]]
  // CHECK-DEBUG-NEXT: [[FLOAT_TO_INT_CONV:%.*]] = fptosi float [[ST_FLOAT_ST_VAL]] to [[INT]]
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[FLOAT_TO_INT_CONV]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += static_cast<int>(ST<float>::st);
  // CHECK:      [[ST_S4_ST_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[DEFAULT_LOC]], i32 [[THREAD_NUM]], i8* bitcast ([[S4]]* [[ST_S4_ST]] to i8*), i{{.*}} {{[0-9]+}}, i8*** [[ST_S4_ST]].cache.)
  // CHECK-NEXT: [[ST_S4_ST_ADDR:%.*]] = bitcast i8* [[ST_S4_ST_TEMP_ADDR]] to [[S4]]*
  // CHECK-NEXT: [[ST_S4_ST_A_ADDR:%.*]] = getelementptr inbounds [[S4]]* [[ST_S4_ST_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-NEXT: [[ST_S4_ST_A:%.*]] = load [[INT]]* [[ST_S4_ST_A_ADDR]]
  // CHECK-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[ST_S4_ST_A]]
  // CHECK-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  // CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
  // CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC19]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
  // CHECK-DEBUG-NEXT: [[ST_S4_ST_TEMP_ADDR:%.*]] = call {{.*}}i8* @__kmpc_threadprivate_cached([[IDENT]]* [[KMPC_LOC_ADDR]], i32 [[THREAD_NUM]], i8* bitcast ([[S4]]* [[ST_S4_ST]] to i8*), i{{.*}} {{[0-9]+}}, i8***
  // CHECK-DEBUG-NEXT: [[ST_S4_ST_ADDR:%.*]] = bitcast i8* [[ST_S4_ST_TEMP_ADDR]] to [[S4]]*
  // CHECK-DEBUG-NEXT: [[ST_S4_ST_A_ADDR:%.*]] = getelementptr inbounds [[S4]]* [[ST_S4_ST_ADDR]], i{{.*}} 0, i{{.*}} 0
  // CHECK-DEBUG-NEXT: [[ST_S4_ST_A:%.*]] = load [[INT]]* [[ST_S4_ST_A_ADDR]]
  // CHECK-DEBUG-NEXT: [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: [[ADD:%.*]] = add {{.*}} [[INT]] [[RES]], [[ST_S4_ST_A]]
  // CHECK-DEBUG-NEXT: store [[INT]] [[ADD]], [[INT]]* [[RES:.+]]
  Res += ST<S4>::st.a;
  // CHECK:      [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-NEXT: ret [[INT]] [[RES]]
  // CHECK-DEBUG:      [[RES:%.*]] = load [[INT]]* [[RES_ADDR]]
  // CHECK-DEBUG-NEXT: ret [[INT]] [[RES]]
  return Res;
}
#endif

// CHECK:      call {{.*}}void @__kmpc_threadprivate_register([[IDENT]]* [[DEFAULT_LOC]], i8* bitcast ([[S4]]* [[ST_S4_ST]] to i8*), i8* (i8*)* [[ST_S4_ST_CTOR:@\.__kmpc_global_ctor_\..+]], i8* (i8*, i8*)* null, void (i8*)* [[ST_S4_ST_DTOR:@\.__kmpc_global_dtor_\..+]])
// CHECK:      define internal {{.*}}i8* [[ST_S4_ST_CTOR]](i8*)
// CHECK:      store i8* %0, i8** [[ARG_ADDR:%.*]],
// CHECK:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK:      [[RES:%.*]] = bitcast i8* [[ARG]] to [[S4]]*
// CHECK-NEXT: call {{.*}} [[S4_CTOR:@.+]]([[S4]]* [[RES]], {{.*}} 23)
// CHECK:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK-NEXT: ret i8* [[ARG]]
// CHECK-NEXT: }
// CHECK:      define {{.*}} [[S4_CTOR]]([[S4]]* {{.*}},
// CHECK:      define internal {{.*}}void [[ST_S4_ST_DTOR]](i8*)
// CHECK:      store i8* %0, i8** [[ARG_ADDR:%.*]],
// CHECK:      [[ARG:%.+]] = load i8** [[ARG_ADDR]]
// CHECK:      [[RES:%.*]] = bitcast i8* [[ARG]] to [[S4]]*
// CHECK-NEXT: call {{.*}} [[S4_DTOR:@.+]]([[S4]]* [[RES]])
// CHECK-NEXT: ret void
// CHECK-NEXT: }
// CHECK:      define {{.*}} [[S4_DTOR]]([[S4]]* {{.*}})
// CHECK-DEBUG:      [[KMPC_LOC_ADDR:%.*]] = alloca [[IDENT]]
// CHECK-DEBUG:      [[KMPC_LOC_ADDR_PSOURCE:%.*]] = getelementptr inbounds [[IDENT]]* [[KMPC_LOC_ADDR]], i{{.*}} 0, i{{.*}} 4
// CHECK-DEBUG-NEXT: store i8* getelementptr inbounds ([{{.*}} x i8]* [[LOC20]], i{{.*}} 0, i{{.*}} 0), i8** [[KMPC_LOC_ADDR_PSOURCE]]
// CHECK-DEBUG:      @__kmpc_global_thread_num
// CHECK-DEBUG:      call {{.*}}void @__kmpc_threadprivate_register([[IDENT]]* [[KMPC_LOC_ADDR]], i8* bitcast ([[S4]]* [[ST_S4_ST]] to i8*), i8* (i8*)* [[ST_S4_ST_CTOR:@\.__kmpc_global_ctor_\..+]], i8* (i8*, i8*)* null, void (i8*)* [[ST_S4_ST_DTOR:@\.__kmpc_global_dtor_\..+]])
// CHECK-DEBUG:      define internal {{.*}}i8* [[ST_S4_ST_CTOR]](i8*)
// CHECK-DEBUG:      }
// CHECK-DEBUG:      define {{.*}} [[S4_CTOR:@.*]]([[S4]]* {{.*}},
// CHECK-DEBUG:      define internal {{.*}}void [[ST_S4_ST_DTOR]](i8*)
// CHECK-DEBUG:      }
// CHECK-DEBUG:      define {{.*}} [[S4_DTOR:@.*]]([[S4]]* {{.*}})

// CHECK:      define internal {{.*}}void {{@.*}}()
// CHECK-DAG:  call {{.*}}void [[GS1_INIT]]()
// CHECK-DAG:  call {{.*}}void [[ARR_X_INIT]]()
// CHECK:      ret void
// CHECK-DEBUG:      define internal {{.*}}void {{@.*}}()
// CHECK-DEBUG:      ret void
