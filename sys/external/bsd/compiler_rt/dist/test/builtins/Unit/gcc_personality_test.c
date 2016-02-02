/* ===-- gcc_personality_test.c - Tests __gcc_personality_v0 -------------===
 *
 *                     The LLVM Compiler Infrastructure
 *
 * This file is dual licensed under the MIT and the University of Illinois Open
 * Source Licenses. See LICENSE.TXT for details.
 *
 * ===----------------------------------------------------------------------===
 */


#include <stdlib.h>
#include <stdio.h>

extern void foo_clean(void* x);
extern void bar_clean(void* x);
extern void register_foo_local(int* x);
extern void register_bar_local(int* x);
extern void done_foo();
extern void done_bar();


/*
 * foo() is called by main() in gcc_personality_test_helper.cxx.
 * done_bar() is implemented in C++ and will throw an exception.
 * main() will catch the exception and verify that the cleanup
 * routines for foo() and bar() were called by the personality
 * function.
 */

void bar() {
    int x __attribute__((cleanup(bar_clean))) = 0;
    register_bar_local(&x);
    done_bar();
}

void foo() {
    int x __attribute__((cleanup(foo_clean))) = 0;
    register_foo_local(&x);
    bar();
    done_foo();
}
