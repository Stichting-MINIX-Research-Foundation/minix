// RUN: %clang_cc1 %s -triple=x86_64-apple-darwin10 -emit-llvm -g -o - | FileCheck %s

class S {
public:
	S& operator = (const S&);
	S (const S&);
	S ();
};

struct CGRect {
	CGRect & operator = (const CGRect &);
};

@interface I {
  S position;
  CGRect bounds;
}

@property(assign, nonatomic) S position;
@property CGRect bounds;
@property CGRect frame;
- (void)setFrame:(CGRect)frameRect;
- (CGRect)frame;
- (void) initWithOwner;
- (CGRect)extent;
- (void)dealloc;
@end

@implementation I
@synthesize position;
@synthesize bounds;
@synthesize frame;

// CHECK: define internal void @"\01-[I setPosition:]"
// CHECK: call dereferenceable({{[0-9]+}}) %class.S* @_ZN1SaSERKS_
// CHECK-NEXT: ret void

// Don't attach debug locations to the prologue instructions. These were
// leaking over from the previous function emission by accident.
// CHECK: define internal void @"\01-[I setBounds:]"
// CHECK-NOT: !dbg
// CHECK: call void @llvm.dbg.declare
- (void)setFrame:(CGRect)frameRect {}
- (CGRect)frame {return bounds;}

- (void)initWithOwner {
  I* _labelLayer;
  CGRect labelLayerFrame = self.bounds;
  labelLayerFrame = self.bounds;
  _labelLayer.frame = labelLayerFrame;
}

// rdar://8366604
- (void)dealloc
  {
      CGRect cgrect = self.extent;
  }
- (struct CGRect)extent {return bounds;}

@end

// CHECK-LABEL: define i32 @main
// CHECK: call void @_ZN1SC1ERKS_(%class.S* [[AGGTMP:%[a-zA-Z0-9\.]+]], %class.S* dereferenceable({{[0-9]+}}) {{%[a-zA-Z0-9\.]+}})
// CHECK: call void bitcast (i8* (i8*, i8*, ...)* @objc_msgSend to void (i8*, i8*, %class.S*)*)(i8* {{%[a-zA-Z0-9\.]+}}, i8* {{%[a-zA-Z0-9\.]+}}, %class.S* [[AGGTMP]])
// CHECK-NEXT: ret i32 0
int main() {
  I *i;
  S s1;
  i.position = s1;
  return 0;
}

// rdar://8379892
// CHECK-LABEL: define void @_Z1fP1A
// CHECK: call void @_ZN1XC1Ev(%struct.X* [[LVTEMP:%[a-zA-Z0-9\.]+]])
// CHECK: call void @_ZN1XC1ERKS_(%struct.X* [[AGGTMP:%[a-zA-Z0-9\.]+]], %struct.X* dereferenceable({{[0-9]+}}) [[LVTEMP]])
// CHECK: call void bitcast (i8* (i8*, i8*, ...)* @objc_msgSend to void (i8*, i8*, %struct.X*)*)({{.*}} %struct.X* [[AGGTMP]])
struct X {
  X();
  X(const X&);
  ~X();
};

@interface A {
  X xval;
}
- (X)x;
- (void)setX:(X)x;
@end

void f(A* a) {
  a.x = X();
}
