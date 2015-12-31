// RUN: %clang_cc1  -fsyntax-only  -triple x86_64-apple-darwin10 -verify %s
// rdar://9092208

__attribute__((unavailable("not available")))
@interface MyClass { // expected-note 8 {{'MyClass' has been explicitly marked unavailable here}}
@public
    void *_test;
    MyClass *ivar; // no error.
}

- (id)self;
- new;
+ (void)addObject:(id)anObject;
- (MyClass *)meth; // no error.

@end

@interface Gorf {
  MyClass *ivar; // expected-error {{unavailable}}
}
- (MyClass *)meth; // expected-error {{unavailable}}
@end

@interface MyClass (Cat1)
- (MyClass *)meth; // no error.
@end

@interface MyClass (Cat2) // no error.
@end

@implementation MyClass (Cat2) // expected-error {{unavailable}}
@end

int main() {
 [MyClass new]; // expected-error {{'MyClass' is unavailable: not available}}
 [MyClass self]; // expected-error {{'MyClass' is unavailable: not available}}
 [MyClass addObject:((void *)0)]; // expected-error {{'MyClass' is unavailable: not available}}

 MyClass *foo = [MyClass new]; // expected-error 2 {{'MyClass' is unavailable: not available}}

 return 0;
}

// rdar://16681279
@interface NSObject @end

__attribute__((visibility("default"))) __attribute__((availability(macosx,unavailable)))
@interface Foo : NSObject @end // expected-note 3 {{'Foo' has been explicitly marked unavailable here}}
@interface AppDelegate  : NSObject
@end

@class Foo;

@implementation AppDelegate
- (void) applicationDidFinishLaunching
{
  Foo *foo = 0; // expected-error {{'Foo' is unavailable}}
}
@end

@class Foo;
Foo *g_foo = 0; // expected-error {{'Foo' is unavailable}}

@class Foo;
@class Foo;
@class Foo;
Foo * f_func() { // expected-error {{'Foo' is unavailable}}
  return 0; 
}
