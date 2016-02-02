// RUN: %clang_cc1 -g -emit-llvm -o - %s | FileCheck %s
// The DWARF standard says the underlying data type of an enum may be
// stored in an DW_AT_type entry in the enum DIE. This is useful to have
// so the debugger knows about the signedness of the underlying type.

typedef long NSInteger;
#define NS_ENUM(_type, _name) enum _name : _type _name; enum _name : _type

// Enum with no specified underlying type
typedef enum {
  Enum0One,
  Enum0Two
} Enum0;

// Enum declared with the NS_ENUM macro
typedef NS_ENUM(NSInteger, Enum1) {
  Enum1One = -1,
  Enum1Two
};

// Enum declared with a fixed underlying type
typedef enum : NSInteger {
  Enum2One = -1,
  Enum2Two
} Enum2;

// Typedef and declaration separately
enum : NSInteger
{
  Enum3One = -1,
  Enum3Two
};
typedef NSInteger Enum3;

int main() {
  Enum0 e0 = Enum0One;
  // CHECK: call void @llvm.dbg.declare(metadata {{.*}}, metadata ![[ENUM0:[0-9]+]], metadata !{{.*}})
  Enum1 e1 = Enum1One;
  // CHECK: call void @llvm.dbg.declare(metadata {{.*}}, metadata ![[ENUM1:[0-9]+]], metadata !{{.*}})
  Enum2 e2 = Enum2One;
  // CHECK: call void @llvm.dbg.declare(metadata {{.*}}, metadata ![[ENUM2:[0-9]+]], metadata !{{.*}})
  Enum3 e3 = Enum3One;
  // CHECK: call void @llvm.dbg.declare(metadata {{.*}}, metadata ![[ENUM3:[0-9]+]], metadata !{{.*}})

  // -Werror and the following line ensures that these enums are not
  // -treated as C++11 strongly typed enums.
  return e0 != e1 && e1 == e2 && e2 == e3;
}
// CHECK: ![[ENUMERATOR0:[0-9]+]] = {{.*}}; [ DW_TAG_enumeration_type ] [line 10
// CHECK: ![[ENUMERATOR1:[0-9]+]] = {{.*}}; [ DW_TAG_enumeration_type ] [Enum1] [line 16{{.*}}] [from NSInteger]
// CHECK: ![[ENUMERATOR3:[0-9]+]] = {{.*}}; [ DW_TAG_typedef ] [NSInteger] [line 6{{.*}}] [from long int]
// CHECK: ![[ENUMERATOR2:[0-9]+]] = {{.*}}; [ DW_TAG_enumeration_type ] [line 22{{.*}}] [from NSInteger]

// CHECK: ![[ENUM0]] = !{!"0x100\00e0\00{{[^,]*}}"{{, [^,]+, [^,]+}}, ![[TYPE0:[0-9]+]]} ; [ DW_TAG_auto_variable ]
// CHECK: ![[TYPE0]] = !{!"0x16\00Enum0\00{{.*}}", {{.*}}, ![[ENUMERATOR0]]} ; [ DW_TAG_typedef ] [Enum0]

// CHECK: ![[ENUM1]] = !{!"0x100\00e1\00{{[^,]*}}"{{, [^,]+, [^,]+}}, ![[TYPE1:[0-9]+]]} ; [ DW_TAG_auto_variable ]
// CHECK: ![[TYPE1]] = !{!"0x16\00Enum1\00{{.*}}", {{.*}}, ![[ENUMERATOR1]]} ; [ DW_TAG_typedef ] [Enum1]

// CHECK: ![[ENUM2]] = !{!"0x100\00e2\00{{[^,]*}}"{{, [^,]+, [^,]+}}, ![[TYPE2:[0-9]+]]} ; [ DW_TAG_auto_variable ]
// CHECK: ![[TYPE2]] = !{!"0x16\00Enum2\00{{.*}}", {{.*}}, ![[ENUMERATOR2]]} ; [ DW_TAG_typedef ] [Enum2]

// CHECK: ![[ENUM3]] = !{!"0x100\00e3\00{{[^,]*}}"{{, [^,]+, [^,]+}}, ![[TYPE3:[0-9]+]]} ; [ DW_TAG_auto_variable ]
// CHECK: ![[TYPE3]] = !{!"0x16\00Enum3\00{{.*}}", {{.*}}, ![[ENUMERATOR3]]} ; [ DW_TAG_typedef ] [Enum3]
