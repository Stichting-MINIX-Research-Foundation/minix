// RUN: %clang_cc1 -verify -fopenmp=libiomp5 -x c++ -triple %itanium_abi_triple -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -fopenmp=libiomp5 -x c++ -std=c++11 -triple %itanium_abi_triple -emit-pch -o %t %s
// RUN: %clang_cc1 -fopenmp=libiomp5 -x c++ -triple %itanium_abi_triple -std=c++11 -include-pch %t -verify %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 -verify -fopenmp=libiomp5 -x c++ -std=c++11 -DLAMBDA -triple %itanium_abi_triple -emit-llvm %s -o - | FileCheck -check-prefix=LAMBDA %s
// RUN: %clang_cc1 -verify -fopenmp=libiomp5 -x c++ -fblocks -DBLOCKS -triple %itanium_abi_triple -emit-llvm %s -o - | FileCheck -check-prefix=BLOCKS %s
// expected-no-diagnostics
#ifndef HEADER
#define HEADER

struct St {
  int a, b;
  St() : a(0), b(0) {}
  St(const St &st) : a(st.a + st.b), b(0) {}
  ~St() {}
};

volatile int g = 1212;

template <class T>
struct S {
  T f;
  S(T a) : f(a + g) {}
  S() : f(g) {}
  S(const S &s, St t = St()) : f(s.f + t.a) {}
  operator T() { return T(); }
  ~S() {}
};

// CHECK-DAG: [[S_FLOAT_TY:%.+]] = type { float }
// CHECK-DAG: [[S_INT_TY:%.+]] = type { i{{[0-9]+}} }
// CHECK-DAG: [[ST_TY:%.+]] = type { i{{[0-9]+}}, i{{[0-9]+}} }
// CHECK-DAG: [[CAP_MAIN_TY:%.+]] = type { [2 x i{{[0-9]+}}]*, i{{[0-9]+}}*, [2 x [[S_FLOAT_TY]]]*, [[S_FLOAT_TY]]* }
// CHECK-DAG: [[CAP_TMAIN_TY:%.+]] = type { [2 x i{{[0-9]+}}]*, i{{[0-9]+}}*, [2 x [[S_INT_TY]]]*, [[S_INT_TY]]* }
// CHECK-DAG: [[IMPLICIT_BARRIER_LOC:@.+]] = private unnamed_addr constant %{{.+}} { i32 0, i32 66, i32 0, i32 0, i8*

template <typename T>
T tmain() {
  S<T> test;
  T t_var = T();
  T vec[] = {1, 2};
  S<T> s_arr[] = {1, 2};
  S<T> var(3);
#pragma omp parallel firstprivate(t_var, vec, s_arr, var)
  {
    vec[0] = t_var;
    s_arr[0] = var;
  }
  return T();
}

int main() {
#ifdef LAMBDA
  // LAMBDA: [[G:@.+]] = global i{{[0-9]+}} 1212,
  // LAMBDA-LABEL: @main
  // LAMBDA: call void [[OUTER_LAMBDA:@.+]](
  [&]() {
  // LAMBDA: define{{.*}} internal{{.*}} void [[OUTER_LAMBDA]](
  // LAMBDA: [[G_LOCAL_REF:%.+]] = getelementptr inbounds %{{.+}}* [[AGG_CAPTURED:%.+]], i{{[0-9]+}} 0, i{{[0-9]+}} 0
  // LAMBDA: store i{{[0-9]+}}* [[G]], i{{[0-9]+}}** [[G_LOCAL_REF]]
  // LAMBDA: [[ARG:%.+]] = bitcast %{{.+}}* [[AGG_CAPTURED]] to i8*
  // LAMBDA: call void {{.+}}* @__kmpc_fork_call({{.+}}, i32 1, {{.+}}* [[OMP_REGION:@.+]] to {{.+}}, i8* [[ARG]])
#pragma omp parallel firstprivate(g)
  {
    // LAMBDA: define{{.*}} internal{{.*}} void [[OMP_REGION]](i32* %{{.+}}, i32* %{{.+}}, %{{.+}}* [[ARG:%.+]])
    // LAMBDA: [[G_PRIVATE_ADDR:%.+]] = alloca i{{[0-9]+}},
    // LAMBDA: store %{{.+}}* [[ARG]], %{{.+}}** [[ARG_REF:%.+]],
    // LAMBDA: [[ARG:%.+]] = load %{{.+}}** [[ARG_REF]]
    // LAMBDA: [[G_REF_ADDR:%.+]] = getelementptr inbounds %{{.+}}* [[ARG]], i{{[0-9]+}} 0, i{{[0-9]+}} 0
    // LAMBDA: [[G_REF:%.+]] = load i{{[0-9]+}}** [[G_REF_ADDR]]
    // LAMBDA: [[G_VAL:%.+]] = load volatile i{{[0-9]+}}* [[G_REF]]
    // LAMBDA: store volatile i{{[0-9]+}} [[G_VAL]], i{{[0-9]+}}* [[G_PRIVATE_ADDR]]
    // LAMBDA: call i32 @__kmpc_cancel_barrier(
    g = 1;
    // LAMBDA: store volatile i{{[0-9]+}} 1, i{{[0-9]+}}* [[G_PRIVATE_ADDR]],
    // LAMBDA: [[G_PRIVATE_ADDR_REF:%.+]] = getelementptr inbounds %{{.+}}* [[ARG:%.+]], i{{[0-9]+}} 0, i{{[0-9]+}} 0
    // LAMBDA: store i{{[0-9]+}}* [[G_PRIVATE_ADDR]], i{{[0-9]+}}** [[G_PRIVATE_ADDR_REF]]
    // LAMBDA: call void [[INNER_LAMBDA:@.+]](%{{.+}}* [[ARG]])
    [&]() {
      // LAMBDA: define {{.+}} void [[INNER_LAMBDA]](%{{.+}}* [[ARG_PTR:%.+]])
      // LAMBDA: store %{{.+}}* [[ARG_PTR]], %{{.+}}** [[ARG_PTR_REF:%.+]],
      g = 2;
      // LAMBDA: [[ARG_PTR:%.+]] = load %{{.+}}** [[ARG_PTR_REF]]
      // LAMBDA: [[G_PTR_REF:%.+]] = getelementptr inbounds %{{.+}}* [[ARG_PTR]], i{{[0-9]+}} 0, i{{[0-9]+}} 0
      // LAMBDA: [[G_REF:%.+]] = load i{{[0-9]+}}** [[G_PTR_REF]]
      // LAMBDA: store volatile i{{[0-9]+}} 2, i{{[0-9]+}}* [[G_REF]]
    }();
  }
  }();
  return 0;
#elif defined(BLOCKS)
  // BLOCKS: [[G:@.+]] = global i{{[0-9]+}} 1212,
  // BLOCKS-LABEL: @main
  // BLOCKS: call void {{%.+}}(i8*
  ^{
  // BLOCKS: define{{.*}} internal{{.*}} void {{.+}}(i8*
  // BLOCKS: [[G_LOCAL_REF:%.+]] = getelementptr inbounds %{{.+}}* [[AGG_CAPTURED:%.+]], i{{[0-9]+}} 0, i{{[0-9]+}} 0
  // BLOCKS: store i{{[0-9]+}}* [[G]], i{{[0-9]+}}** [[G_LOCAL_REF]]
  // BLOCKS: [[ARG:%.+]] = bitcast %{{.+}}* [[AGG_CAPTURED]] to i8*
  // BLOCKS: call void {{.+}}* @__kmpc_fork_call({{.+}}, i32 1, {{.+}}* [[OMP_REGION:@.+]] to {{.+}}, i8* [[ARG]])
#pragma omp parallel firstprivate(g)
  {
    // BLOCKS: define{{.*}} internal{{.*}} void [[OMP_REGION]](i32* %{{.+}}, i32* %{{.+}}, %{{.+}}* [[ARG:%.+]])
    // BLOCKS: [[G_PRIVATE_ADDR:%.+]] = alloca i{{[0-9]+}},
    // BLOCKS: store %{{.+}}* [[ARG]], %{{.+}}** [[ARG_REF:%.+]],
    // BLOCKS: [[ARG:%.+]] = load %{{.+}}** [[ARG_REF]]
    // BLOCKS: [[G_REF_ADDR:%.+]] = getelementptr inbounds %{{.+}}* [[ARG]], i{{[0-9]+}} 0, i{{[0-9]+}} 0
    // BLOCKS: [[G_REF:%.+]] = load i{{[0-9]+}}** [[G_REF_ADDR]]
    // BLOCKS: [[G_VAL:%.+]] = load volatile i{{[0-9]+}}* [[G_REF]]
    // BLOCKS: store volatile i{{[0-9]+}} [[G_VAL]], i{{[0-9]+}}* [[G_PRIVATE_ADDR]]
    // BLOCKS: call i32 @__kmpc_cancel_barrier(
    g = 1;
    // BLOCKS: store volatile i{{[0-9]+}} 1, i{{[0-9]+}}* [[G_PRIVATE_ADDR]],
    // BLOCKS-NOT: [[G]]{{[[^:word:]]}}
    // BLOCKS: i{{[0-9]+}}* [[G_PRIVATE_ADDR]]
    // BLOCKS-NOT: [[G]]{{[[^:word:]]}}
    // BLOCKS: call void {{%.+}}(i8*
    ^{
      // BLOCKS: define {{.+}} void {{@.+}}(i8*
      g = 2;
      // BLOCKS-NOT: [[G]]{{[[^:word:]]}}
      // BLOCKS: store volatile i{{[0-9]+}} 2, i{{[0-9]+}}*
      // BLOCKS-NOT: [[G]]{{[[^:word:]]}}
      // BLOCKS: ret
    }();
  }
  }();
  return 0;
#else
  S<float> test;
  int t_var = 0;
  int vec[] = {1, 2};
  S<float> s_arr[] = {1, 2};
  S<float> var(3);
#pragma omp parallel firstprivate(t_var, vec, s_arr, var)
  {
    vec[0] = t_var;
    s_arr[0] = var;
  }
  return tmain<int>();
#endif
}

// CHECK: define {{.*}}i{{[0-9]+}} @main()
// CHECK: [[TEST:%.+]] = alloca [[S_FLOAT_TY]],
// CHECK: call {{.*}} [[S_FLOAT_TY_DEF_CONSTR:@.+]]([[S_FLOAT_TY]]* [[TEST]])
// CHECK: %{{.+}} = bitcast [[CAP_MAIN_TY]]*
// CHECK: call void (%{{.+}}*, i{{[0-9]+}}, void (i{{[0-9]+}}*, i{{[0-9]+}}*, ...)*, ...)* @__kmpc_fork_call(%{{.+}}* @{{.+}}, i{{[0-9]+}} 1, void (i{{[0-9]+}}*, i{{[0-9]+}}*, ...)* bitcast (void (i{{[0-9]+}}*, i{{[0-9]+}}*, [[CAP_MAIN_TY]]*)* [[MAIN_MICROTASK:@.+]] to void
// CHECK: = call {{.*}}i{{.+}} [[TMAIN_INT:@.+]]()
// CHECK: call {{.*}} [[S_FLOAT_TY_DESTR:@.+]]([[S_FLOAT_TY]]*
// CHECK: ret
//
// CHECK: define internal void [[MAIN_MICROTASK]](i{{[0-9]+}}* [[GTID_ADDR:%.+]], i{{[0-9]+}}* %{{.+}}, [[CAP_MAIN_TY]]* %{{.+}})
// CHECK: [[T_VAR_PRIV:%.+]] = alloca i{{[0-9]+}},
// CHECK: [[VEC_PRIV:%.+]] = alloca [2 x i{{[0-9]+}}],
// CHECK: [[S_ARR_PRIV:%.+]] = alloca [2 x [[S_FLOAT_TY]]],
// CHECK: [[VAR_PRIV:%.+]] = alloca [[S_FLOAT_TY]],
// CHECK: store i{{[0-9]+}}* [[GTID_ADDR]], i{{[0-9]+}}** [[GTID_ADDR_ADDR:%.+]],
// CHECK: [[T_VAR_PTR_REF:%.+]] = getelementptr inbounds [[CAP_MAIN_TY]]* %{{.+}}, i{{[0-9]+}} 0, i{{[0-9]+}} 1
// CHECK: [[T_VAR_REF:%.+]] = load i{{[0-9]+}}** [[T_VAR_PTR_REF]],
// CHECK: [[T_VAR_VAL:%.+]] = load i{{[0-9]+}}* [[T_VAR_REF]],
// CHECK: store i{{[0-9]+}} [[T_VAR_VAL]], i{{[0-9]+}}* [[T_VAR_PRIV]],
// CHECK: [[VEC_PTR_REF:%.+]] = getelementptr inbounds [[CAP_MAIN_TY]]* %{{.+}}, i{{[0-9]+}} 0, i{{[0-9]+}} 0
// CHECK: [[VEC_REF:%.+]] = load [2 x i{{[0-9]+}}]** [[VEC_PTR_REF:%.+]],
// CHECK: br label %[[VEC_PRIV_INIT:.+]]
// CHECK: [[VEC_PRIV_INIT]]
// CHECK: [[VEC_DEST:%.+]] = bitcast [2 x i{{[0-9]+}}]* [[VEC_PRIV]] to i8*
// CHECK: [[VEC_SRC:%.+]] = bitcast [2 x i{{[0-9]+}}]* [[VEC_REF]] to i8*
// CHECK: call void @llvm.memcpy.{{.+}}(i8* [[VEC_DEST]], i8* [[VEC_SRC]],
// CHECK: br label %[[VEC_PRIV_INIT_END:.+]]
// CHECK: [[VEC_PRIV_INIT_END]]
// CHECK: [[S_ARR_REF_PTR:%.+]] = getelementptr inbounds [[CAP_MAIN_TY]]* %{{.+}}, i{{[0-9]+}} 0, i{{[0-9]+}} 2
// CHECK: [[S_ARR_REF:%.+]] = load [2 x [[S_FLOAT_TY]]]** [[S_ARR_REF_PTR]],
// CHECK: br label %[[S_ARR_PRIV_INIT:.+]]
// CHECK: [[S_ARR_PRIV_INIT]]
// CHECK: [[S_ARR_BEGIN:%.+]] = getelementptr inbounds [2 x [[S_FLOAT_TY]]]* [[S_ARR_REF]], i{{[0-9]+}} 0, i{{[0-9]+}} 0
// CHECK: [[S_ARR_PRIV_BEGIN:%.+]] = getelementptr inbounds [2 x [[S_FLOAT_TY]]]* [[S_ARR_PRIV]], i{{[0-9]+}} 0, i{{[0-9]+}} 0
// CHECK: [[S_ARR_END:%.+]] = getelementptr [[S_FLOAT_TY]]* [[S_ARR_BEGIN]], i{{[0-9]+}} 2
// CHECK: [[S_ARR_PRIV_END:%.+]] = getelementptr [[S_FLOAT_TY]]* [[S_ARR_PRIV_BEGIN]], i{{[0-9]+}} 2
// CHECK: [[IS_EMPTY:%.+]] = icmp eq [[S_FLOAT_TY]]* [[S_ARR_PRIV_BEGIN]], [[S_ARR_PRIV_END]]
// CHECK: br i1 [[IS_EMPTY]], label %[[S_ARR_BODY_DONE:.+]], label %[[S_ARR_BODY:.+]]
// CHECK: [[S_ARR_BODY]]
// CHECK: call {{.*}} [[ST_TY_DEFAULT_CONSTR:@.+]]([[ST_TY]]* [[ST_TY_TEMP:%.+]])
// CHECK: call {{.*}} [[S_FLOAT_TY_COPY_CONSTR:@.+]]([[S_FLOAT_TY]]* {{.+}}, [[S_FLOAT_TY]]* {{.+}}, [[ST_TY]]* [[ST_TY_TEMP]])
// CHECK: call {{.*}} [[ST_TY_DESTR:@.+]]([[ST_TY]]* [[ST_TY_TEMP]])
// CHECK: br i1 {{.+}}, label %{{.+}}, label %[[S_ARR_BODY]]
// CHECK: br label %[[S_ARR_PRIV_INIT_END:.+]]
// CHECK: [[S_ARR_PRIV_INIT_END]]
// CHECK: [[VAR_REF_PTR:%.+]] = getelementptr inbounds [[CAP_MAIN_TY]]* %{{.+}}, i{{[0-9]+}} 0, i{{[0-9]+}} 3
// CHECK: [[VAR_REF:%.+]] = load [[S_FLOAT_TY]]** [[VAR_REF_PTR]],
// CHECK: call {{.*}} [[ST_TY_DEFAULT_CONSTR]]([[ST_TY]]* [[ST_TY_TEMP:%.+]])
// CHECK: call {{.*}} [[S_FLOAT_TY_COPY_CONSTR]]([[S_FLOAT_TY]]* [[VAR_PRIV]], [[S_FLOAT_TY]]* {{.*}} [[VAR_REF]], [[ST_TY]]* [[ST_TY_TEMP]])
// CHECK: call {{.*}} [[ST_TY_DESTR]]([[ST_TY]]* [[ST_TY_TEMP]])
// CHECK: [[GTID_REF:%.+]] = load i{{[0-9]+}}** [[GTID_ADDR_ADDR]]
// CHECK: [[GTID:%.+]] = load i{{[0-9]+}}* [[GTID_REF]]
// CHECK: call i32 @__kmpc_cancel_barrier(%{{.+}}* [[IMPLICIT_BARRIER_LOC]], i{{[0-9]+}} [[GTID]])
// CHECK-DAG: call {{.*}} [[S_FLOAT_TY_DESTR]]([[S_FLOAT_TY]]* [[VAR_PRIV]])
// CHECK-DAG: call {{.*}} [[S_FLOAT_TY_DESTR]]([[S_FLOAT_TY]]*
// CHECK: ret void

// CHECK: define {{.*}} i{{[0-9]+}} [[TMAIN_INT]]()
// CHECK: [[TEST:%.+]] = alloca [[S_INT_TY]],
// CHECK: call {{.*}} [[S_INT_TY_DEF_CONSTR:@.+]]([[S_INT_TY]]* [[TEST]])
// CHECK: call void (%{{.+}}*, i{{[0-9]+}}, void (i{{[0-9]+}}*, i{{[0-9]+}}*, ...)*, ...)* @__kmpc_fork_call(%{{.+}}* @{{.+}}, i{{[0-9]+}} 1, void (i{{[0-9]+}}*, i{{[0-9]+}}*, ...)* bitcast (void (i{{[0-9]+}}*, i{{[0-9]+}}*, [[CAP_TMAIN_TY]]*)* [[TMAIN_MICROTASK:@.+]] to void
// CHECK: call {{.*}} [[S_INT_TY_DESTR:@.+]]([[S_INT_TY]]*
// CHECK: ret
//
// CHECK: define internal void [[TMAIN_MICROTASK]](i{{[0-9]+}}* [[GTID_ADDR:%.+]], i{{[0-9]+}}* %{{.+}}, [[CAP_TMAIN_TY]]* %{{.+}})
// CHECK: [[T_VAR_PRIV:%.+]] = alloca i{{[0-9]+}},
// CHECK: [[VEC_PRIV:%.+]] = alloca [2 x i{{[0-9]+}}],
// CHECK: [[S_ARR_PRIV:%.+]] = alloca [2 x [[S_INT_TY]]],
// CHECK: [[VAR_PRIV:%.+]] = alloca [[S_INT_TY]],
// CHECK: store i{{[0-9]+}}* [[GTID_ADDR]], i{{[0-9]+}}** [[GTID_ADDR_ADDR:%.+]],
// CHECK: [[T_VAR_PTR_REF:%.+]] = getelementptr inbounds [[CAP_TMAIN_TY]]* %{{.+}}, i{{[0-9]+}} 0, i{{[0-9]+}} 1
// CHECK: [[T_VAR_REF:%.+]] = load i{{[0-9]+}}** [[T_VAR_PTR_REF]],
// CHECK: [[T_VAR_VAL:%.+]] = load i{{[0-9]+}}* [[T_VAR_REF]],
// CHECK: store i{{[0-9]+}} [[T_VAR_VAL]], i{{[0-9]+}}* [[T_VAR_PRIV]],
// CHECK: [[VEC_PTR_REF:%.+]] = getelementptr inbounds [[CAP_TMAIN_TY]]* %{{.+}}, i{{[0-9]+}} 0, i{{[0-9]+}} 0
// CHECK: [[VEC_REF:%.+]] = load [2 x i{{[0-9]+}}]** [[VEC_PTR_REF:%.+]],
// CHECK: br label %[[VEC_PRIV_INIT:.+]]
// CHECK: [[VEC_PRIV_INIT]]
// CHECK: [[VEC_DEST:%.+]] = bitcast [2 x i{{[0-9]+}}]* [[VEC_PRIV]] to i8*
// CHECK: [[VEC_SRC:%.+]] = bitcast [2 x i{{[0-9]+}}]* [[VEC_REF]] to i8*
// CHECK: call void @llvm.memcpy.{{.+}}(i8* [[VEC_DEST]], i8* [[VEC_SRC]],
// CHECK: br label %[[VEC_PRIV_INIT_END:.+]]
// CHECK: [[VEC_PRIV_INIT_END]]
// CHECK: [[S_ARR_REF_PTR:%.+]] = getelementptr inbounds [[CAP_TMAIN_TY]]* %{{.+}}, i{{[0-9]+}} 0, i{{[0-9]+}} 2
// CHECK: [[S_ARR_REF:%.+]] = load [2 x [[S_INT_TY]]]** [[S_ARR_REF_PTR]],
// CHECK: br label %[[S_ARR_PRIV_INIT:.+]]
// CHECK: [[S_ARR_PRIV_INIT]]
// CHECK: [[S_ARR_BEGIN:%.+]] = getelementptr inbounds [2 x [[S_INT_TY]]]* [[S_ARR_REF]], i{{[0-9]+}} 0, i{{[0-9]+}} 0
// CHECK: [[S_ARR_PRIV_BEGIN:%.+]] = getelementptr inbounds [2 x [[S_INT_TY]]]* [[S_ARR_PRIV]], i{{[0-9]+}} 0, i{{[0-9]+}} 0
// CHECK: [[S_ARR_END:%.+]] = getelementptr [[S_INT_TY]]* [[S_ARR_BEGIN]], i{{[0-9]+}} 2
// CHECK: [[S_ARR_PRIV_END:%.+]] = getelementptr [[S_INT_TY]]* [[S_ARR_PRIV_BEGIN]], i{{[0-9]+}} 2
// CHECK: [[IS_EMPTY:%.+]] = icmp eq [[S_INT_TY]]* [[S_ARR_PRIV_BEGIN]], [[S_ARR_PRIV_END]]
// CHECK: br i1 [[IS_EMPTY]], label %[[S_ARR_BODY_DONE:.+]], label %[[S_ARR_BODY:.+]]
// CHECK: [[S_ARR_BODY]]
// CHECK: call {{.*}} [[ST_TY_DEFAULT_CONSTR]]([[ST_TY]]* [[ST_TY_TEMP:%.+]])
// CHECK: call {{.*}} [[S_INT_TY_COPY_CONSTR:@.+]]([[S_INT_TY]]* {{.+}}, [[S_INT_TY]]* {{.+}}, [[ST_TY]]* [[ST_TY_TEMP]])
// CHECK: call {{.*}} [[ST_TY_DESTR]]([[ST_TY]]* [[ST_TY_TEMP]])
// CHECK: br i1 {{.+}}, label %{{.+}}, label %[[S_ARR_BODY]]
// CHECK: br label %[[S_ARR_PRIV_INIT_END:.+]]
// CHECK: [[S_ARR_PRIV_INIT_END]]
// CHECK: [[VAR_REF_PTR:%.+]] = getelementptr inbounds [[CAP_TMAIN_TY]]* %{{.+}}, i{{[0-9]+}} 0, i{{[0-9]+}} 3
// CHECK: [[VAR_REF:%.+]] = load [[S_INT_TY]]** [[VAR_REF_PTR]],
// CHECK: call {{.*}} [[ST_TY_DEFAULT_CONSTR]]([[ST_TY]]* [[ST_TY_TEMP:%.+]])
// CHECK: call {{.*}} [[S_INT_TY_COPY_CONSTR]]([[S_INT_TY]]* [[VAR_PRIV]], [[S_INT_TY]]* {{.*}} [[VAR_REF]], [[ST_TY]]* [[ST_TY_TEMP]])
// CHECK: call {{.*}} [[ST_TY_DESTR]]([[ST_TY]]* [[ST_TY_TEMP]])
// CHECK: [[GTID_REF:%.+]] = load i{{[0-9]+}}** [[GTID_ADDR_ADDR]]
// CHECK: [[GTID:%.+]] = load i{{[0-9]+}}* [[GTID_REF]]
// CHECK: call i32 @__kmpc_cancel_barrier(%{{.+}}* [[IMPLICIT_BARRIER_LOC]], i{{[0-9]+}} [[GTID]])
// CHECK-DAG: call {{.*}} [[S_INT_TY_DESTR]]([[S_INT_TY]]* [[VAR_PRIV]])
// CHECK-DAG: call {{.*}} [[S_INT_TY_DESTR]]([[S_INT_TY]]*
// CHECK: ret void
#endif

