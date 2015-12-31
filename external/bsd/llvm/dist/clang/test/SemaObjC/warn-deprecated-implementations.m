// RUN: %clang_cc1 -fsyntax-only -Wdeprecated-implementations -verify -Wno-objc-root-class %s
// rdar://8973810
// rdar://12717705

@protocol P
- (void) D __attribute__((deprecated)); // expected-note {{method 'D' declared here}}
@end

@interface A <P>
+ (void)F __attribute__((deprecated));
@end

@interface A()
- (void) E __attribute__((deprecated));
@end

@implementation A
+ (void)F { }	// No warning, implementing its own deprecated method
- (void) D {} //  expected-warning {{Implementing deprecated method}}
- (void) E {} // No warning, implementing deprecated method in its class extension.
@end

@interface A(CAT)
- (void) G __attribute__((deprecated)); 
@end

@implementation A(CAT)
- (void) G {} 	// No warning, implementing its own deprecated method
@end

__attribute__((deprecated))
@interface CL // expected-note 2 {{class declared here}} // expected-note 2 {{'CL' has been explicitly marked deprecated here}}
@end

@implementation CL // expected-warning {{Implementing deprecated class}}
@end

@implementation CL ( SomeCategory ) // expected-warning {{'CL' is deprecated}} \
                                    // expected-warning {{Implementing deprecated category}}
@end

@interface CL_SUB : CL // expected-warning {{'CL' is deprecated}}
@end

@interface BASE
- (void) B __attribute__((deprecated)); // expected-note {{method 'B' declared here}}
@end

@interface SUB : BASE
@end

@implementation SUB
- (void) B {} // expected-warning {{Implementing deprecated method}}
@end

@interface Test
@end

@interface Test()
- (id)initSpecialInPrivateHeader __attribute__((deprecated));
@end

@implementation Test
- (id)initSpecialInPrivateHeader {
  return (void *)0;
}
@end
