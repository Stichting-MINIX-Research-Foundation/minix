// RUN: %clang_cc1 -triple x86_64-apple-darwin10 -emit-llvm -fblocks -fobjc-arc -fobjc-runtime-has-weak -O2 -disable-llvm-optzns -o - %s | FileCheck %s

#define PRECISE_LIFETIME __attribute__((objc_precise_lifetime))

id test0_helper(void) __attribute__((ns_returns_retained));
void test0() {
  PRECISE_LIFETIME id x = test0_helper();
  x = 0;
  // CHECK:      [[X:%.*]] = alloca i8*
  // CHECK-NEXT: [[CALL:%.*]] = call i8* @test0_helper()
  // CHECK-NEXT: store i8* [[CALL]], i8** [[X]]

  // CHECK-NEXT: [[T1:%.*]] = load i8** [[X]]
  // CHECK-NEXT: store i8* null, i8** [[X]]
  // CHECK-NEXT: call void @objc_release(i8* [[T1]]) [[NUW:#[0-9]+]]
  // CHECK-NOT:  clang.imprecise_release

  // CHECK-NEXT: [[T1:%.*]] = load i8** [[X]]
  // CHECK-NEXT: call void @objc_release(i8* [[T1]]) [[NUW:#[0-9]+]]
  // CHECK-NOT:  clang.imprecise_release

  // CHECK-NEXT: ret void
}

// rdar://problem/9821110
@interface Test1
- (char*) interior __attribute__((objc_returns_inner_pointer));
// Should we allow this on properties? Yes! see // rdar://14990439
@property (nonatomic, readonly) char * PropertyReturnsInnerPointer __attribute__((objc_returns_inner_pointer));
@end
extern Test1 *test1_helper(void);

// CHECK-LABEL: define void @test1a()
void test1a(void) {
  // CHECK:      [[T0:%.*]] = call [[TEST1:%.*]]* @test1_helper()
  // CHECK-NEXT: [[T1:%.*]] = bitcast [[TEST1]]* [[T0]] to i8*
  // CHECK-NEXT: [[T2:%.*]] = call i8* @objc_retainAutoreleasedReturnValue(i8* [[T1]])
  // CHECK-NEXT: [[T3:%.*]] = bitcast i8* [[T2]] to [[TEST1]]*
  // CHECK-NEXT: store [[TEST1]]* [[T3]]
  // CHECK-NEXT: [[T0:%.*]] = load [[TEST1]]**
  // CHECK-NEXT: [[T1:%.*]] = bitcast [[TEST1]]* [[T0]] to i8*
  // CHECK-NEXT: [[T2:%.*]] = call i8* @objc_retainAutorelease(i8* [[T1]])
  // CHECK-NEXT: [[T3:%.*]] = bitcast i8* [[T2]] to [[TEST1]]*
  // CHECK-NEXT: [[T4:%.*]] = load i8** @OBJC_SELECTOR_REFERENCES_
  // CHECK-NEXT: [[T5:%.*]] = bitcast [[TEST1]]* [[T3]] to i8*
  // CHECK-NEXT: [[T6:%.*]] = call i8* bitcast
  // CHECK-NEXT: store i8* [[T6]], i8**
  // CHECK-NEXT: [[T0:%.*]] = load [[TEST1]]**
  // CHECK-NEXT: [[T1:%.*]] = bitcast [[TEST1]]* [[T0]] to i8*
  // CHECK-NEXT: call void @objc_release(i8* [[T1]]) [[NUW]], !clang.imprecise_release
  // CHECK-NEXT: ret void
  Test1 *ptr = test1_helper();
  char *c = [(ptr) interior];
}

// CHECK-LABEL: define void @test1b()
void test1b(void) {
  // CHECK:      [[T0:%.*]] = call [[TEST1:%.*]]* @test1_helper()
  // CHECK-NEXT: [[T1:%.*]] = bitcast [[TEST1]]* [[T0]] to i8*
  // CHECK-NEXT: [[T2:%.*]] = call i8* @objc_retainAutoreleasedReturnValue(i8* [[T1]])
  // CHECK-NEXT: [[T3:%.*]] = bitcast i8* [[T2]] to [[TEST1]]*
  // CHECK-NEXT: store [[TEST1]]* [[T3]]
  // CHECK-NEXT: [[T0:%.*]] = load [[TEST1]]**
  // CHECK-NEXT: [[T1:%.*]] = load i8** @OBJC_SELECTOR_REFERENCES_
  // CHECK-NEXT: [[T2:%.*]] = bitcast [[TEST1]]* [[T0]] to i8*
  // CHECK-NEXT: [[T3:%.*]] = call i8* bitcast
  // CHECK-NEXT: store i8* [[T3]], i8**
  // CHECK-NEXT: [[T0:%.*]] = load [[TEST1]]**
  // CHECK-NEXT: [[T1:%.*]] = bitcast [[TEST1]]* [[T0]] to i8*
  // CHECK-NEXT: call void @objc_release(i8* [[T1]]) [[NUW]]
  // CHECK-NOT:  clang.imprecise_release
  // CHECK-NEXT: ret void
  __attribute__((objc_precise_lifetime)) Test1 *ptr = test1_helper();
  char *c = [ptr interior];
}

void test1c(void) {
  // CHECK:      [[T0:%.*]] = call [[TEST1:%.*]]* @test1_helper()
  // CHECK-NEXT: [[T1:%.*]] = bitcast [[TEST1]]* [[T0]] to i8*
  // CHECK-NEXT: [[T2:%.*]] = call i8* @objc_retainAutoreleasedReturnValue(i8* [[T1]])
  // CHECK-NEXT: [[T3:%.*]] = bitcast i8* [[T2]] to [[TEST1]]*
  // CHECK-NEXT: store [[TEST1]]* [[T3]]
  // CHECK-NEXT: [[T0:%.*]] = load [[TEST1]]**
  // CHECK-NEXT: [[T1:%.*]] = bitcast [[TEST1]]* [[T0]] to i8*
  // CHECK-NEXT: [[T2:%.*]] = call i8* @objc_retainAutorelease(i8* [[T1]])
  // CHECK-NEXT: [[T3:%.*]] = bitcast i8* [[T2]] to [[TEST1]]*
  // CHECK-NEXT: [[T4:%.*]] = load i8** @OBJC_SELECTOR_REFERENCES_
  // CHECK-NEXT: [[T5:%.*]] = bitcast [[TEST1]]* [[T3]] to i8*
  // CHECK-NEXT: [[T6:%.*]] = call i8* bitcast
  // CHECK-NEXT: store i8* [[T6]], i8**
  // CHECK-NEXT: [[T0:%.*]] = load [[TEST1]]**
  // CHECK-NEXT: [[T1:%.*]] = bitcast [[TEST1]]* [[T0]] to i8*
  // CHECK-NEXT: call void @objc_release(i8* [[T1]]) [[NUW]], !clang.imprecise_release
  // CHECK-NEXT: ret void
  Test1 *ptr = test1_helper();
  char *pc = ptr.PropertyReturnsInnerPointer;
}

void test1d(void) {
  // CHECK:      [[T0:%.*]] = call [[TEST1:%.*]]* @test1_helper()
  // CHECK-NEXT: [[T1:%.*]] = bitcast [[TEST1]]* [[T0]] to i8*
  // CHECK-NEXT: [[T2:%.*]] = call i8* @objc_retainAutoreleasedReturnValue(i8* [[T1]])
  // CHECK-NEXT: [[T3:%.*]] = bitcast i8* [[T2]] to [[TEST1]]*
  // CHECK-NEXT: store [[TEST1]]* [[T3]]
  // CHECK-NEXT: [[T0:%.*]] = load [[TEST1]]**
  // CHECK-NEXT: [[T2:%.*]] = bitcast [[TEST1]]* [[T0]] to i8*
  // CHECK-NEXT: [[T3:%.*]] = call i8* @objc_retainAutorelease
  // CHECK-NEXT: [[SIX:%.*]] = bitcast i8* [[T3]] to [[TEST1]]*
  // CHECK-NEXT: [[SEVEN:%.*]] = load i8** @OBJC_SELECTOR_REFERENCES_
  // CHECK-NEXT: [[EIGHT:%.*]] = bitcast [[TEST1]]* [[SIX]] to i8*
  // CHECK-NEXT: [[CALL1:%.*]] = call i8* bitcast (i8* (i8*, i8*, ...)* @objc_msgSend to i8* (i8*, i8*)*)(i8* [[EIGHT]], i8* [[SEVEN]])
  // CHECK-NEXT: store i8* [[CALL1]], i8**
  // CHECK-NEXT: [[NINE:%.*]] = load [[TEST1]]** 
  // CHECK-NEXT: [[TEN:%.*]] = bitcast [[TEST1]]* [[NINE]] to i8*
  // CHECK-NEXT: call void @objc_release(i8* [[TEN]])
  // CHECK-NEXT: ret void
  __attribute__((objc_precise_lifetime)) Test1 *ptr = test1_helper();
  char *pc = ptr.PropertyReturnsInnerPointer;
}

@interface Test2 {
@public
  id ivar;
}
@end
// CHECK-LABEL:      define void @test2(
void test2(Test2 *x) {
  x->ivar = 0;
  // CHECK:      [[X:%.*]] = alloca [[TEST2:%.*]]*
  // CHECK-NEXT: [[T0:%.*]] = bitcast [[TEST2]]* {{%.*}} to i8*
  // CHECK-NEXT: [[T1:%.*]] = call i8* @objc_retain(i8* [[T0]]) [[NUW]]
  // CHECK-NEXT: [[T2:%.*]] = bitcast i8* [[T1]] to [[TEST2]]*
  // CHECK-NEXT: store [[TEST2]]* [[T2]], [[TEST2]]** [[X]],

  // CHECK-NEXT: [[T0:%.*]] = load [[TEST2]]** [[X]],
  // CHECK-NEXT: [[OFFSET:%.*]] = load i64* @"OBJC_IVAR_$_Test2.ivar"
  // CHECK-NEXT: [[T1:%.*]] = bitcast [[TEST2]]* [[T0]] to i8*
  // CHECK-NEXT: [[T2:%.*]] = getelementptr inbounds i8* [[T1]], i64 [[OFFSET]]
  // CHECK-NEXT: [[T3:%.*]] = bitcast i8* [[T2]] to i8**
  // CHECK-NEXT: [[T4:%.*]] = load i8** [[T3]],
  // CHECK-NEXT: store i8* null, i8** [[T3]],
  // CHECK-NEXT: call void @objc_release(i8* [[T4]]) [[NUW]]
  // CHECK-NOT:  imprecise

  // CHECK-NEXT: [[T0:%.*]] = load [[TEST2]]** [[X]]
  // CHECK-NEXT: [[T1:%.*]] = bitcast [[TEST2]]* [[T0]] to i8*
  // CHECK-NEXT: call void @objc_release(i8* [[T1]]) [[NUW]], !clang.imprecise_release

  // CHECK-NEXT: ret void
}

// CHECK-LABEL:      define void @test3(i8*
void test3(PRECISE_LIFETIME id x) {
  // CHECK:      [[X:%.*]] = alloca i8*,
  // CHECK-NEXT: [[T0:%.*]] = call i8* @objc_retain(i8* {{%.*}}) [[NUW]]
  // CHECK-NEXT: store i8* [[T0]], i8** [[X]],

  // CHECK-NEXT: [[T0:%.*]] = load i8** [[X]]
  // CHECK-NEXT: call void @objc_release(i8* [[T0]]) [[NUW]]
  // CHECK-NOT:  imprecise_release

  // CHECK-NEXT: ret void  
}

// CHECK: attributes [[NUW]] = { nounwind }
