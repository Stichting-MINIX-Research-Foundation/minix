// RUN: %clang_cc1 %s -fno-rtti -triple=i386-pc-win32 -emit-llvm -o - -O1 -disable-llvm-optzns | FileCheck %s -check-prefix=NO-RTTI
// RUN: %clang_cc1 %s -triple=i386-pc-win32 -emit-llvm -o - -O1 -disable-llvm-optzns | FileCheck %s -check-prefix=RTTI

// RTTI-DAG: $"\01??_7S@@6B@" = comdat largest
// RTTI-DAG: $"\01??_7V@@6B@" = comdat largest
// RTTI-DAG: $"\01??_7W@?A@@6B@" = comdat largest

struct S {
  virtual ~S();
} s;

// RTTI-DAG: [[VTABLE_S:@.*]] = private unnamed_addr constant [2 x i8*] [i8* bitcast ({{.*}} @"\01??_R4S@@6B@" to i8*), i8* bitcast ({{.*}} @"\01??_GS@@UAEPAXI@Z" to i8*)], comdat($"\01??_7S@@6B@")
// RTTI-DAG: @"\01??_7S@@6B@" = unnamed_addr alias getelementptr inbounds ([2 x i8*]* [[VTABLE_S]], i32 0, i32 1)

// NO-RTTI-DAG: @"\01??_7S@@6B@" = linkonce_odr unnamed_addr constant [1 x i8*] [i8* bitcast ({{.*}} @"\01??_GS@@UAEPAXI@Z" to i8*)]

struct __declspec(dllimport) U {
  virtual ~U();
} u;

// RTTI-DAG: @"\01??_7U@@6B@" = available_externally dllimport unnamed_addr constant [1 x i8*] [i8* bitcast ({{.*}} @"\01??_GU@@UAEPAXI@Z" to i8*)]

// NO-RTTI-DAG: @"\01??_7U@@6B@" = available_externally dllimport unnamed_addr constant [1 x i8*] [i8* bitcast ({{.*}} @"\01??_GU@@UAEPAXI@Z" to i8*)]

struct __declspec(dllexport) V {
  virtual ~V();
} v;

// RTTI-DAG: [[VTABLE_V:@.*]] = private unnamed_addr constant [2 x i8*] [i8* bitcast ({{.*}} @"\01??_R4V@@6B@" to i8*), i8* bitcast ({{.*}} @"\01??_GV@@UAEPAXI@Z" to i8*)], comdat($"\01??_7V@@6B@")
// RTTI-DAG: @"\01??_7V@@6B@" = dllexport unnamed_addr alias getelementptr inbounds ([2 x i8*]* [[VTABLE_V]], i32 0, i32 1)

// NO-RTTI-DAG: @"\01??_7V@@6B@" = weak_odr dllexport unnamed_addr constant [1 x i8*] [i8* bitcast ({{.*}} @"\01??_GV@@UAEPAXI@Z" to i8*)]

namespace {
struct W {
  virtual ~W();
} w;
}
// RTTI-DAG: [[VTABLE_W:@.*]] = private unnamed_addr constant [2 x i8*] [i8* bitcast ({{.*}} @"\01??_R4W@?A@@6B@" to i8*), i8* bitcast ({{.*}} @"\01??_GW@?A@@UAEPAXI@Z" to i8*)], comdat($"\01??_7W@?A@@6B@")
// RTTI-DAG: @"\01??_7W@?A@@6B@" = internal unnamed_addr alias getelementptr inbounds ([2 x i8*]* @1, i32 0, i32 1)

// NO-RTTI-DAG: @"\01??_7W@?A@@6B@" = internal unnamed_addr constant [1 x i8*] [i8* bitcast ({{.*}} @"\01??_GW@?A@@UAEPAXI@Z" to i8*)]
