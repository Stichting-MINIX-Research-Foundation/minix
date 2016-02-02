// RUN: %clang_cc1 -verify -fopenmp=libiomp5 -x c++ -emit-llvm %s -fexceptions -fcxx-exceptions -o - | FileCheck %s
// RUN: %clang_cc1 -fopenmp=libiomp5 -x c++ -std=c++11 -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -emit-pch -o %t %s
// RUN: %clang_cc1 -fopenmp=libiomp5 -x c++ -triple x86_64-unknown-unknown -fexceptions -fcxx-exceptions -g -std=c++11 -include-pch %t -verify %s -emit-llvm -o - | FileCheck --check-prefix=CHECK-DEBUG %s
// expected-no-diagnostics
#ifndef HEADER
#define HEADER

// CHECK-DAG: %ident_t = type { i32, i32, i32, i32, i8* }
// CHECK-DAG: %struct.anon = type { i32* }
// CHECK-DAG: %struct.anon.0 = type { i8*** }
// CHECK-DAG: [[STR:@.+]] = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00"
// CHECK-DAG: [[DEF_LOC_2:@.+]] = private unnamed_addr constant %ident_t { i32 0, i32 2, i32 0, i32 0, i8* getelementptr inbounds ([23 x i8]* [[STR]], i32 0, i32 0) }
// CHECK-DEBUG-DAG: %ident_t = type { i32, i32, i32, i32, i8* }
// CHECK-DEBUG-DAG: %struct.anon = type { i32* }
// CHECK-DEBUG-DAG: %struct.anon.0 = type { i8*** }
// CHECK-DEBUG-DAG: [[STR:@.+]] = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00"
// CHECK-DEBUG-DAG: [[DEF_LOC_2:@.+]] = private unnamed_addr constant %ident_t { i32 0, i32 2, i32 0, i32 0, i8* getelementptr inbounds ([23 x i8]* [[STR]], i32 0, i32 0) }
// CHECK-DEBUG-DAG: [[LOC1:@.+]] = private unnamed_addr constant [{{.+}} x i8] c";{{.*}}parallel_codegen.cpp;main;[[@LINE+14]];9;;\00"
// CHECK-DEBUG-DAG: [[LOC2:@.+]] = private unnamed_addr constant [{{.+}} x i8] c";{{.*}}parallel_codegen.cpp;tmain;[[@LINE+7]];9;;\00"

template <class T>
void foo(T argc) {}

template <typename T>
int tmain(T argc) {
#pragma omp parallel
  foo(argc);
  return 0;
}

int main (int argc, char **argv) {
#pragma omp parallel
  foo(argc);
  return tmain(argv);
}

// CHECK-LABEL: define {{[a-z]*[ ]?i32}} @main({{i32[ ]?[a-z]*}} %argc, i8** %argv)
// CHECK:       [[AGG_CAPTURED:%.+]] = alloca %struct.anon
// CHECK:       [[ARGC_REF:%.+]] = getelementptr inbounds %struct.anon* [[AGG_CAPTURED]], i32 0, i32 0
// CHECK-NEXT:  store i32* {{%[a-z0-9.]+}}, i32** [[ARGC_REF]]
// CHECK-NEXT:  [[BITCAST:%.+]] = bitcast %struct.anon* [[AGG_CAPTURED]] to i8*
// CHECK-NEXT:  call void (%ident_t*, i32, void (i32*, i32*, ...)*, ...)* @__kmpc_fork_call(%ident_t* [[DEF_LOC_2]], i32 1, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, %struct.anon*)* @.omp_outlined. to void (i32*, i32*, ...)*), i8* [[BITCAST]])
// CHECK-NEXT:  [[ARGV:%.+]] = load i8*** {{%[a-z0-9.]+}}
// CHECK-NEXT:  [[RET:%.+]] = call {{[a-z]*[ ]?i32}} [[TMAIN:@.+tmain.+]](i8** [[ARGV]])
// CHECK-NEXT:  ret i32 [[RET]]
// CHECK-NEXT:  }
// CHECK-DEBUG-LABEL: define i32 @main(i32 %argc, i8** %argv)
// CHECK-DEBUG-DAG:   [[AGG_CAPTURED:%.+]] = alloca %struct.anon
// CHECK-DEBUG-DAG:   [[LOC_2_ADDR:%.+]] = alloca %ident_t
// CHECK-DEBUG:       [[KMPC_LOC_VOIDPTR:%.+]] = bitcast %ident_t* [[LOC_2_ADDR]] to i8*
// CHECK-DEBUG-NEXT:  [[KMPC_DEFAULT_LOC_VOIDPTR:%.+]] = bitcast %ident_t* [[DEF_LOC_2]] to i8*
// CHECK-DEBUG-NEXT:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* [[KMPC_LOC_VOIDPTR]], i8* [[KMPC_DEFAULT_LOC_VOIDPTR]], i64 ptrtoint (%ident_t* getelementptr (%ident_t* null, i32 1) to i64), i32 8, i1 false)
// CHECK-DEBUG:       [[ARGC_REF:%.+]] = getelementptr inbounds %struct.anon* [[AGG_CAPTURED]], i32 0, i32 0
// CHECK-DEBUG-NEXT:  store i32* {{%[a-z0-9.]+}}, i32** [[ARGC_REF]]
// CHECK-DEBUG-NEXT:  [[KMPC_LOC_PSOURCE_REF:%.+]] = getelementptr inbounds %ident_t* [[LOC_2_ADDR]], i32 0, i32 4
// CHECK-DEBUG-NEXT:  store i8* getelementptr inbounds ([{{.+}} x i8]* [[LOC1]], i32 0, i32 0), i8** [[KMPC_LOC_PSOURCE_REF]]
// CHECK-DEBUG-NEXT:  [[BITCAST:%.+]] = bitcast %struct.anon* [[AGG_CAPTURED]] to i8*
// CHECK-DEBUG-NEXT:  call void (%ident_t*, i32, void (i32*, i32*, ...)*, ...)* @__kmpc_fork_call(%ident_t* [[LOC_2_ADDR]], i32 1, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, %struct.anon*)* @.omp_outlined. to void (i32*, i32*, ...)*), i8* [[BITCAST]])
// CHECK-DEBUG-NEXT:  [[ARGV:%.+]] = load i8*** {{%[a-z0-9.]+}}
// CHECK-DEBUG-NEXT:  [[RET:%.+]] = call i32 [[TMAIN:@.+tmain.+]](i8** [[ARGV]])
// CHECK-DEBUG-NEXT:  ret i32 [[RET]]
// CHECK-DEBUG-NEXT:  }

// CHECK-LABEL: define internal void @.omp_outlined.(i32* %.global_tid., i32* %.bound_tid., %struct.anon* %__context)
// CHECK:       [[CONTEXT_ADDR:%.+]] = alloca %struct.anon*
// CHECK:       store %struct.anon* %__context, %struct.anon** [[CONTEXT_ADDR]]
// CHECK:       [[CONTEXT_PTR:%.+]] = load %struct.anon** [[CONTEXT_ADDR]]
// CHECK-NEXT:  [[ARGC_PTR_REF:%.+]] = getelementptr inbounds %struct.anon* [[CONTEXT_PTR]], i32 0, i32 0
// CHECK-NEXT:  [[ARGC_REF:%.+]] = load i32** [[ARGC_PTR_REF]]
// CHECK-NEXT:  [[ARGC:%.+]] = load i32* [[ARGC_REF]]
// CHECK-NEXT:  invoke void [[FOO:@.+foo.+]](i32{{[ ]?[a-z]*}} [[ARGC]])
// CHECK:       ret void
// CHECK:       call void @{{.+terminate.*}}(
// CHECK-NEXT:  unreachable
// CHECK-NEXT:  }
// CHECK-DEBUG-LABEL: define internal void @.omp_outlined.(i32* %.global_tid., i32* %.bound_tid., %struct.anon* %__context)
// CHECK-DEBUG:       [[CONTEXT_ADDR:%.+]] = alloca %struct.anon*
// CHECK-DEBUG:       store %struct.anon* %__context, %struct.anon** [[CONTEXT_ADDR]]
// CHECK-DEBUG:       [[CONTEXT_PTR:%.+]] = load %struct.anon** [[CONTEXT_ADDR]]
// CHECK-DEBUG-NEXT:  [[ARGC_PTR_REF:%.+]] = getelementptr inbounds %struct.anon* [[CONTEXT_PTR]], i32 0, i32 0
// CHECK-DEBUG-NEXT:  [[ARGC_REF:%.+]] = load i32** [[ARGC_PTR_REF]]
// CHECK-DEBUG-NEXT:  [[ARGC:%.+]] = load i32* [[ARGC_REF]]
// CHECK-DEBUG-NEXT:  invoke void [[FOO:@.+foo.+]](i32 [[ARGC]])
// CHECK-DEBUG:       ret void
// CHECK-DEBUG:       call void @{{.+terminate.*}}(
// CHECK-DEBUG-NEXT:  unreachable
// CHECK-DEBUG-NEXT:  }

// CHECK-DAG: define linkonce_odr void [[FOO]]({{i32[ ]?[a-z]*}} %argc)
// CHECK-DAG: declare void @__kmpc_fork_call(%ident_t*, i32, void (i32*, i32*, ...)*, ...)
// CHECK-DEBUG-DAG: define linkonce_odr void [[FOO]](i32 %argc)
// CHECK-DEBUG-DAG: declare void @__kmpc_fork_call(%ident_t*, i32, void (i32*, i32*, ...)*, ...)

// CHECK:       define linkonce_odr {{[a-z]*[ ]?i32}} [[TMAIN]](i8** %argc)
// CHECK:       [[AGG_CAPTURED:%.+]] = alloca %struct.anon.0
// CHECK:       [[ARGC_REF:%.+]] = getelementptr inbounds %struct.anon.0* [[AGG_CAPTURED]], i32 0, i32 0
// CHECK-NEXT:  store i8*** {{%[a-z0-9.]+}}, i8**** [[ARGC_REF]]
// CHECK-NEXT:  [[BITCAST:%.+]] = bitcast %struct.anon.0* [[AGG_CAPTURED]] to i8*
// CHECK-NEXT:  call void (%ident_t*, i32, void (i32*, i32*, ...)*, ...)* @__kmpc_fork_call(%ident_t* [[DEF_LOC_2]], i32 1, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, %struct.anon.0*)* @.omp_outlined.1 to void (i32*, i32*, ...)*), i8* [[BITCAST]])
// CHECK-NEXT:  ret i32 0
// CHECK-NEXT:  }
// CHECK-DEBUG:       define linkonce_odr i32 [[TMAIN]](i8** %argc)
// CHECK-DEBUG-DAG:   [[AGG_CAPTURED:%.+]] = alloca %struct.anon.0
// CHECK-DEBUG-DAG:   [[LOC_2_ADDR:%.+]] = alloca %ident_t
// CHECK-DEBUG:       [[KMPC_LOC_VOIDPTR:%.+]] = bitcast %ident_t* [[LOC_2_ADDR]] to i8*
// CHECK-DEBUG-NEXT:  [[KMPC_DEFAULT_LOC_VOIDPTR:%.+]] = bitcast %ident_t* [[DEF_LOC_2]] to i8*
// CHECK-DEBUG-NEXT:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* [[KMPC_LOC_VOIDPTR]], i8* [[KMPC_DEFAULT_LOC_VOIDPTR]], i64 ptrtoint (%ident_t* getelementptr (%ident_t* null, i32 1) to i64), i32 8, i1 false)
// CHECK-DEBUG:       [[ARGC_REF:%.+]] = getelementptr inbounds %struct.anon.0* [[AGG_CAPTURED]], i32 0, i32 0
// CHECK-DEBUG-NEXT:  store i8*** {{%[a-z0-9.]+}}, i8**** [[ARGC_REF]]
// CHECK-DEBUG-NEXT:  [[KMPC_LOC_PSOURCE_REF:%.+]] = getelementptr inbounds %ident_t* [[LOC_2_ADDR]], i32 0, i32 4
// CHECK-DEBUG-NEXT:  store i8* getelementptr inbounds ([{{.+}} x i8]* [[LOC2]], i32 0, i32 0), i8** [[KMPC_LOC_PSOURCE_REF]]
// CHECK-DEBUG-NEXT:  [[BITCAST:%.+]] = bitcast %struct.anon.0* [[AGG_CAPTURED]] to i8*
// CHECK-DEBUG-NEXT:  call void (%ident_t*, i32, void (i32*, i32*, ...)*, ...)* @__kmpc_fork_call(%ident_t* [[LOC_2_ADDR]], i32 1, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, %struct.anon.0*)* @.omp_outlined.1 to void (i32*, i32*, ...)*), i8* [[BITCAST]])
// CHECK-DEBUG-NEXT:  ret i32 0
// CHECK-DEBUG-NEXT:  }

// CHECK-LABEL: define internal void @.omp_outlined.1(i32* %.global_tid., i32* %.bound_tid., %struct.anon.0* %__context)
// CHECK:       [[CONTEXT_ADDR:%.+]] = alloca %struct.anon.0*
// CHECK:       store %struct.anon.0* %__context, %struct.anon.0** [[CONTEXT_ADDR]]
// CHECK:       [[CONTEXT_PTR:%.+]] = load %struct.anon.0** [[CONTEXT_ADDR]]
// CHECK-NEXT:  [[ARGC_PTR_REF:%.+]] = getelementptr inbounds %struct.anon.0* [[CONTEXT_PTR]], i32 0, i32 0
// CHECK-NEXT:  [[ARGC_REF:%.+]] = load i8**** [[ARGC_PTR_REF]]
// CHECK-NEXT:  [[ARGC:%.+]] = load i8*** [[ARGC_REF]]
// CHECK-NEXT:  invoke void [[FOO1:@.+foo.+]](i8** [[ARGC]])
// CHECK:       ret void
// CHECK:       call void @{{.+terminate.*}}(
// CHECK-NEXT:  unreachable
// CHECK-NEXT:  }
// CHECK-DEBUG-LABEL: define internal void @.omp_outlined.1(i32* %.global_tid., i32* %.bound_tid., %struct.anon.0* %__context)
// CHECK-DEBUG:       [[CONTEXT_ADDR:%.+]] = alloca %struct.anon.0*
// CHECK-DEBUG:       store %struct.anon.0* %__context, %struct.anon.0** [[CONTEXT_ADDR]]
// CHECK-DEBUG:       [[CONTEXT_PTR:%.+]] = load %struct.anon.0** [[CONTEXT_ADDR]]
// CHECK-DEBUG-NEXT:  [[ARGC_PTR_REF:%.+]] = getelementptr inbounds %struct.anon.0* [[CONTEXT_PTR]], i32 0, i32 0
// CHECK-DEBUG-NEXT:  [[ARGC_REF:%.+]] = load i8**** [[ARGC_PTR_REF]]
// CHECK-DEBUG-NEXT:  [[ARGC:%.+]] = load i8*** [[ARGC_REF]]
// CHECK-DEBUG-NEXT:  invoke void [[FOO1:@.+foo.+]](i8** [[ARGC]])
// CHECK-DEBUG:       ret void
// CHECK-DEBUG:       call void @{{.+terminate.*}}(
// CHECK-DEBUG-NEXT:  unreachable
// CHECK-DEBUG-NEXT:  }

// CHECK: define linkonce_odr void [[FOO1]](i8** %argc)
// CHECK-DEBUG: define linkonce_odr void [[FOO1]](i8** %argc)

#endif
