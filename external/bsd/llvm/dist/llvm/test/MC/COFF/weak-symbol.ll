; Test that weak functions and globals are placed into selectany COMDAT
; sections with the mangled name as suffix. Ensure that the weak linkage
; type is not ignored by the backend if the section was specialized.
;
; RUN: llc -mtriple=i686-pc-win32 %s     -o - | FileCheck %s --check-prefix=X86
; RUN: llc -mtriple=i686-pc-mingw32 %s   -o - | FileCheck %s --check-prefix=X86
; RUN: llc -mtriple=x86_64-pc-win32 %s   -o - | FileCheck %s --check-prefix=X64
; RUN: llc -mtriple=x86_64-pc-mingw32 %s -o - | FileCheck %s --check-prefix=X64

; Mangled function
; X86: .section .text,"xr",discard,__Z3foo
; X86: .globl __Z3foo
;
; X64: .section .text,"xr",discard,_Z3foo
; X64: .globl _Z3foo
define weak void @_Z3foo() {
  ret void
}

; Unmangled function
; X86: .section .sect,"xr",discard,_f
; X86: .globl _f
;
; X64: .section .sect,"xr",discard,f
; X64: .globl f
define weak void @f() section ".sect" {
  ret void
}

; Weak global
; X86: .section .data,"dr",discard,_a
; X86: .globl _a
; X86: .zero 12
;
; X64: .section .data,"dr",discard,a
; X64: .globl a
; X64: .zero 12
@a = weak unnamed_addr constant { i32, i32, i32 } { i32 0, i32 0, i32 0}, section ".data"

; X86:  .section        .tls$,"dw",discard,_b
; X86:  .globl  _b
; X86:  .long   0
;
; X64:  .section        .tls$,"dw",discard,b
; X64:  .globl  b
; X64:  .long   0

@b = weak_odr thread_local global i32 0, align 4
