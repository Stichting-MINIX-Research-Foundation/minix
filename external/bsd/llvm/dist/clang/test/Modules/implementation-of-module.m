// RUN: not %clang_cc1 -fmodule-implementation-of Foo -fmodule-name=Bar %s 2>&1 \
// RUN:     | FileCheck -check-prefix=CHECK-IMPL-OF-ERR %s
// CHECK-IMPL-OF-ERR: conflicting module names specified: '-fmodule-name=Bar' and '-fmodule-implementation-of Foo'

// RUN: rm -rf %t
// RUN: %clang_cc1 -fmodules -fmodules-cache-path=%t -w -Werror=auto-import %s -I %S/Inputs \
// RUN:     -fmodule-implementation-of category_right -fsyntax-only

// RUN: %clang_cc1 -fmodules -fmodules-cache-path=%t -w -Werror=auto-import %s -I %S/Inputs \
// RUN:     -fmodule-implementation-of category_right -dM -E -o - 2>&1 | FileCheck %s
// CHECK-NOT: __building_module

// RUN: %clang_cc1 -fmodules -fmodules-cache-path=%t -w -Werror=auto-import %s -I %S/Inputs \
// RUN:     -fmodule-implementation-of category_left -verify

// RUN: %clang_cc1 -x objective-c-header -fmodules -fmodules-cache-path=%t -w -Werror=auto-import %s -I %S/Inputs \
// RUN:     -fmodule-implementation-of category_right -emit-pch -o %t.pch
// RUN: %clang_cc1 -x objective-c-header -fmodules -fmodules-cache-path=%t -w -Werror=auto-import %s -I %S/Inputs \
// RUN:     -DWITH_PREFIX -include-pch %t.pch -fmodule-implementation-of category_right

#ifndef WITH_PREFIX

@import category_left; // expected-error{{@import of module 'category_left' in implementation of 'category_left'; use #import}}
@import category_left.sub; // expected-error{{@import of module 'category_left.sub' in implementation of 'category_left'; use #import}}
#import "category_right.h" // expected-error{{treating}}
#import "category_right_sub.h" // expected-error{{treating}}

#endif

