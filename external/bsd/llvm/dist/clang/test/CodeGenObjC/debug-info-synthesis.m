// RUN: %clang_cc1 -emit-llvm -g -w -triple x86_64-apple-darwin10 %s -o - | FileCheck %s
# 1 "foo.m" 1
# 1 "foo.m" 2
# 1 "./foo.h" 1
@interface NSObject {
  struct objc_object *isa;
}
@end
@class NSDictionary;

@interface Foo : NSObject {}
@property (strong, nonatomic) NSDictionary *dict;
@end
# 2 "foo.m" 2




@implementation Foo
@synthesize dict = _dict;

- (void) bork {
}
@end

int main(int argc, char *argv[]) {
  @autoreleasepool {
    Foo *f = [Foo new];
    [f bork];
  }
}

// CHECK: ![[FILE:.*]] = {{.*}}[ DW_TAG_file_type ] [{{.*}}/foo.h]
// CHECK: ![[FILE]], {{.*}} ; [ DW_TAG_subprogram ] [line 8] [local] [def] [-[Foo dict]]
