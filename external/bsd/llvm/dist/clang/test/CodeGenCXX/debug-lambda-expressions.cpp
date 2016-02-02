// RUN: %clang_cc1 -triple x86_64-apple-darwin10.0.0 -emit-llvm -o - %s -fexceptions -std=c++11 -g | FileCheck %s

auto var = [](int i) { return i+1; };
void *use = &var;

extern "C" auto cvar = []{};

int a() { return []{ return 1; }(); }

int b(int x) { return [x]{return x;}(); }

int c(int x) { return [&x]{return x;}(); }

struct D { D(); D(const D&); int x; };
int d(int x) { D y[10]; return [x,y] { return y[x].x; }(); }

// Randomness for file. -- 6
// CHECK: [[FILE:.*]] = {{.*}} [ DW_TAG_file_type ] [{{.*}}debug-lambda-expressions.cpp]

// A: 10
// CHECK: [[A_FUNC:.*]] = {{.*}} [ DW_TAG_subprogram ] [line [[A_LINE:.*]]] [def] [a]

// B: 14
// CHECK: [[B_FUNC:.*]] = {{.*}} [ DW_TAG_subprogram ] [line [[B_LINE:.*]]] [def] [b]

// C: 17
// CHECK: [[C_FUNC:.*]] = {{.*}} [ DW_TAG_subprogram ] [line [[C_LINE:.*]]] [def] [c]

// D: 18
// CHECK: [[D_FUNC:.*]] = {{.*}} [ DW_TAG_subprogram ] [line [[D_LINE:.*]]] [def] [d]

// Back to D. -- 24
// CHECK: [[LAM_D:.*]] = {{.*}}, [[D_FUNC]], {{.*}}, [[LAM_D_ARGS:.*]], null, null, null} ; [ DW_TAG_class_type ] [line [[D_LINE]],
// CHECK: [[LAM_D_ARGS]] = !{[[CAP_D_X:.*]], [[CAP_D_Y:.*]], [[CON_LAM_D:.*]]}
// CHECK: [[CAP_D_X]] = {{.*}}, [[LAM_D]], {{.*}} [ DW_TAG_member ] [x] [line [[D_LINE]],
// CHECK: [[CAP_D_Y]] = {{.*}}, [[LAM_D]], {{.*}} [ DW_TAG_member ] [y] [line [[D_LINE]],
// CHECK: [[CON_LAM_D]] = {{.*}}, [[LAM_D]], {{.*}} [ DW_TAG_subprogram ] [line [[D_LINE]]] [public] [operator()]


// Back to C. -- 55
// CHECK: [[LAM_C:.*]] = {{.*}}, [[C_FUNC]], {{.*}}, [[LAM_C_ARGS:.*]], null, null, null} ; [ DW_TAG_class_type ] [line [[C_LINE]],
// CHECK: [[LAM_C_ARGS]] = !{[[CAP_C:.*]], [[CON_LAM_C:.*]]}
// Ignoring the member type for now.
// CHECK: [[CAP_C]] = {{.*}}, [[LAM_C]], {{.*}}} ; [ DW_TAG_member ] [x] [line [[C_LINE]],
// CHECK: [[CON_LAM_C]] = {{.*}}, [[LAM_C]], {{.*}} [ DW_TAG_subprogram ] [line [[C_LINE]]] [public] [operator()]


// Back to B. -- 67
// CHECK: [[LAM_B:.*]] = {{.*}}, [[B_FUNC]], {{.*}}, [[LAM_B_ARGS:.*]], null, null, null} ; [ DW_TAG_class_type ] [line [[B_LINE]],
// CHECK: [[LAM_B_ARGS]] = !{[[CAP_B:.*]], [[CON_LAM_B:.*]]}
// CHECK: [[CAP_B]] = {{.*}}, [[LAM_B]], {{.*}}} ; [ DW_TAG_member ] [x] [line [[B_LINE]],
// CHECK: [[CON_LAM_B]] = {{.*}}, [[LAM_B]], {{.*}} [ DW_TAG_subprogram ] [line [[B_LINE]]] [public] [operator()]

// Back to A. -- 78
// CHECK: [[LAM_A:.*]] = {{.*}}, [[A_FUNC]], {{.*}}, [[LAM_A_ARGS:.*]], null, null, null} ; [ DW_TAG_class_type ] [line [[A_LINE]],
// CHECK: [[LAM_A_ARGS]] = !{[[CON_LAM_A:.*]]}
// CHECK: [[CON_LAM_A]] = {{.*}}, [[LAM_A]], {{.*}} [ DW_TAG_subprogram ] [line [[A_LINE]]] [public] [operator()]

// CVAR:
// CHECK: {{.*}} [[CVAR_T:![0-9]*]], {{.*}} ; [ DW_TAG_variable ] [cvar] [line [[CVAR_LINE:[0-9]*]]] 
// CHECK: [[CVAR_T]] = {{.*}}, ![[CVAR_ARGS:.*]], null, null, null} ; [ DW_TAG_class_type ] [line [[CVAR_LINE]],
// CHECK: [[CVAR_ARGS]] = !{!{{.*}}}

// VAR:
// CHECK: {{.*}} [[VAR_T:![0-9]*]], {{.*}} ; [ DW_TAG_variable ] [var] [line [[VAR_LINE:[0-9]*]]]
// CHECK: [[VAR_T]] = {{.*}}, [[VAR_ARGS:![0-9]*]], null, null, null} ; [ DW_TAG_class_type ] [line [[VAR_LINE]],
// CHECK: [[VAR_ARGS]] = !{!{{.*}}}
