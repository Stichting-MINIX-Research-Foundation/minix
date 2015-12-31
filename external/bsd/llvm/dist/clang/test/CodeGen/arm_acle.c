// RUN: %clang_cc1 -ffreestanding -triple armv8-eabi -target-cpu cortex-a57 -O -S -emit-llvm -o - %s | FileCheck %s -check-prefix=ARM -check-prefix=AArch32
// RUN: %clang_cc1 -ffreestanding -triple aarch64-eabi -target-cpu cortex-a57 -target-feature +neon -target-feature +crc -target-feature +crypto -O -S -emit-llvm -o - %s | FileCheck %s -check-prefix=ARM -check-prefix=AArch64

#include <arm_acle.h>

/* 8 SYNCHRONIZATION, BARRIER AND HINT INTRINSICS */
/* 8.3 Memory Barriers */
// ARM-LABEL: test_dmb
// AArch32: call void @llvm.arm.dmb(i32 1)
// AArch64: call void @llvm.aarch64.dmb(i32 1)
void test_dmb(void) {
  __dmb(1);
}

// ARM-LABEL: test_dsb
// AArch32: call void @llvm.arm.dsb(i32 2)
// AArch64: call void @llvm.aarch64.dsb(i32 2)
void test_dsb(void) {
  __dsb(2);
}

// ARM-LABEL: test_isb
// AArch32: call void @llvm.arm.isb(i32 3)
// AArch64: call void @llvm.aarch64.isb(i32 3)
void test_isb(void) {
  __isb(3);
}

/* 8.4 Hints */
// ARM-LABEL: test_yield
// AArch32: call void @llvm.arm.hint(i32 1)
// AArch64: call void @llvm.aarch64.hint(i32 1)
void test_yield(void) {
  __yield();
}

// ARM-LABEL: test_wfe
// AArch32: call void @llvm.arm.hint(i32 2)
// AArch64: call void @llvm.aarch64.hint(i32 2)
void test_wfe(void) {
  __wfe();
}

// ARM-LABEL: test_wfi
// AArch32: call void @llvm.arm.hint(i32 3)
// AArch64: call void @llvm.aarch64.hint(i32 3)
void test_wfi(void) {
  __wfi();
}

// ARM-LABEL: test_sev
// AArch32: call void @llvm.arm.hint(i32 4)
// AArch64: call void @llvm.aarch64.hint(i32 4)
void test_sev(void) {
  __sev();
}

// ARM-LABEL: test_sevl
// AArch32: call void @llvm.arm.hint(i32 5)
// AArch64: call void @llvm.aarch64.hint(i32 5)
void test_sevl(void) {
  __sevl();
}

#if __ARM_32BIT_STATE
// AArch32-LABEL: test_dbg
// AArch32: call void @llvm.arm.dbg(i32 0)
void test_dbg(void) {
  __dbg(0);
}
#endif

/* 8.5 Swap */
// ARM-LABEL: test_swp
// AArch32: call i32 @llvm.arm.ldrex
// AArch32: call i32 @llvm.arm.strex
// AArch64: call i64 @llvm.aarch64.ldxr
// AArch64: call i32 @llvm.aarch64.stxr
uint32_t test_swp(uint32_t x, volatile void *p) {
  __swp(x, p);
}

/* 8.6 Memory prefetch intrinsics */
/* 8.6.1 Data prefetch */
// ARM-LABEL: test_pld
// ARM: call void @llvm.prefetch(i8* null, i32 0, i32 3, i32 1)
void test_pld() {
  __pld(0);
}

// ARM-LABEL: test_pldx
// AArch32: call void @llvm.prefetch(i8* null, i32 1, i32 3, i32 1)
// AArch64: call void @llvm.prefetch(i8* null, i32 1, i32 1, i32 1)
void test_pldx() {
  __pldx(1, 2, 0, 0);
}

/* 8.6.2 Instruction prefetch */
// ARM-LABEL: test_pli
// ARM: call void @llvm.prefetch(i8* null, i32 0, i32 3, i32 0)
void test_pli() {
  __pli(0);
}

// ARM-LABEL: test_plix
// AArch32: call void @llvm.prefetch(i8* null, i32 0, i32 3, i32 0)
// AArch64: call void @llvm.prefetch(i8* null, i32 0, i32 1, i32 0)
void test_plix() {
  __plix(2, 0, 0);
}

/* 8.7 NOP */
// ARM-LABEL: test_nop
// AArch32: call void @llvm.arm.hint(i32 0)
// AArch64: call void @llvm.aarch64.hint(i32 0)
void test_nop(void) {
  __nop();
}

/* 9 DATA-PROCESSING INTRINSICS */
/* 9.2 Miscellaneous data-processing intrinsics */
// ARM-LABEL: test_ror
// ARM: lshr
// ARM: sub
// ARM: shl
// ARM: or
uint32_t test_ror(uint32_t x, uint32_t y) {
  return __ror(x, y);
}

// ARM-LABEL: test_rorl
// ARM: lshr
// ARM: sub
// ARM: shl
// ARM: or
unsigned long test_rorl(unsigned long x, uint32_t y) {
  return __rorl(x, y);
}

// ARM-LABEL: test_rorll
// ARM: lshr
// ARM: sub
// ARM: shl
// ARM: or
uint64_t test_rorll(uint64_t x, uint32_t y) {
  return __rorll(x, y);
}

// ARM-LABEL: test_clz
// ARM: call i32 @llvm.ctlz.i32(i32 %t, i1 false)
uint32_t test_clz(uint32_t t) {
  return __clz(t);
}

// ARM-LABEL: test_clzl
// AArch32: call i32 @llvm.ctlz.i32(i32 %t, i1 false)
// AArch64: call i64 @llvm.ctlz.i64(i64 %t, i1 false)
long test_clzl(long t) {
  return __clzl(t);
}

// ARM-LABEL: test_clzll
// ARM: call i64 @llvm.ctlz.i64(i64 %t, i1 false)
uint64_t test_clzll(uint64_t t) {
  return __clzll(t);
}

// ARM-LABEL: test_rev
// ARM: call i32 @llvm.bswap.i32(i32 %t)
uint32_t test_rev(uint32_t t) {
  return __rev(t);
}

// ARM-LABEL: test_revl
// AArch32: call i32 @llvm.bswap.i32(i32 %t)
// AArch64: call i64 @llvm.bswap.i64(i64 %t)
long test_revl(long t) {
  return __revl(t);
}

// ARM-LABEL: test_revll
// ARM: call i64 @llvm.bswap.i64(i64 %t)
uint64_t test_revll(uint64_t t) {
  return __revll(t);
}

// ARM-LABEL: test_rev16
// ARM: llvm.bswap
// ARM: lshr
// ARM: shl
// ARM: or
uint32_t test_rev16(uint32_t t) {
  return __rev16(t);
}

// ARM-LABEL: test_rev16l
// ARM: llvm.bswap
// ARM: lshr
// ARM: shl
// ARM: or
long test_rev16l(long t) {
  return __rev16l(t);
}

// ARM-LABEL: test_rev16ll
// ARM: llvm.bswap
// ARM: lshr
// ARM: shl
// ARM: or
uint64_t test_rev16ll(uint64_t t) {
  return __rev16ll(t);
}

// ARM-LABEL: test_revsh
// ARM: call i16 @llvm.bswap.i16(i16 %t)
int16_t test_revsh(int16_t t) {
  return __revsh(t);
}

// ARM-LABEL: test_rbit
// AArch32: call i32 @llvm.arm.rbit
// AArch64: call i32 @llvm.aarch64.rbit.i32
uint32_t test_rbit(uint32_t t) {
  return __rbit(t);
}

// ARM-LABEL: test_rbitl
// AArch32: call i32 @llvm.arm.rbit
// AArch64: call i64 @llvm.aarch64.rbit.i64
long test_rbitl(long t) {
  return __rbitl(t);
}

// ARM-LABEL: test_rbitll
// AArch32: call i32 @llvm.arm.rbit
// AArch32: call i32 @llvm.arm.rbit
// AArch64: call i64 @llvm.aarch64.rbit.i64
uint64_t test_rbitll(uint64_t t) {
  return __rbitll(t);
}

/* 9.4 Saturating intrinsics */
#ifdef __ARM_32BIT_STATE

/* 9.4.1 Width-specified saturation intrinsics */
// AArch32-LABEL: test_ssat
// AArch32: call i32 @llvm.arm.ssat(i32 %t, i32 1)
int32_t test_ssat(int32_t t) {
  return __ssat(t, 1);
}

// AArch32-LABEL: test_usat
// AArch32: call i32 @llvm.arm.usat(i32 %t, i32 2)
int32_t test_usat(int32_t t) {
  return __usat(t, 2);
}

/* 9.4.2 Saturating addition and subtraction intrinsics */
// AArch32-LABEL: test_qadd
// AArch32: call i32 @llvm.arm.qadd(i32 %a, i32 %b)
int32_t test_qadd(int32_t a, int32_t b) {
  return __qadd(a, b);
}

// AArch32-LABEL: test_qsub
// AArch32: call i32 @llvm.arm.qsub(i32 %a, i32 %b)
int32_t test_qsub(int32_t a, int32_t b) {
  return __qsub(a, b);
}

extern int32_t f();
// AArch32-LABEL: test_qdbl
// AArch32: [[VAR:%[a-z0-9]+]] = {{.*}} call {{.*}} @f
// AArch32-NOT: call {{.*}} @f
// AArch32: call i32 @llvm.arm.qadd(i32 [[VAR]], i32 [[VAR]])
int32_t test_qdbl() {
  return __qdbl(f());
}
#endif

/* 9.7 CRC32 intrinsics */
// ARM-LABEL: test_crc32b
// AArch32: call i32 @llvm.arm.crc32b
// AArch64: call i32 @llvm.aarch64.crc32b
uint32_t test_crc32b(uint32_t a, uint8_t b) {
  return __crc32b(a, b);
}

// ARM-LABEL: test_crc32h
// AArch32: call i32 @llvm.arm.crc32h
// AArch64: call i32 @llvm.aarch64.crc32h
uint32_t test_crc32h(uint32_t a, uint16_t b) {
  return __crc32h(a, b);
}

// ARM-LABEL: test_crc32w
// AArch32: call i32 @llvm.arm.crc32w
// AArch64: call i32 @llvm.aarch64.crc32w
uint32_t test_crc32w(uint32_t a, uint32_t b) {
  return __crc32w(a, b);
}

// ARM-LABEL: test_crc32d
// AArch32: call i32 @llvm.arm.crc32w
// AArch32: call i32 @llvm.arm.crc32w
// AArch64: call i32 @llvm.aarch64.crc32x
uint32_t test_crc32d(uint32_t a, uint64_t b) {
  return __crc32d(a, b);
}

// ARM-LABEL: test_crc32cb
// AArch32: call i32 @llvm.arm.crc32cb
// AArch64: call i32 @llvm.aarch64.crc32cb
uint32_t test_crc32cb(uint32_t a, uint8_t b) {
  return __crc32cb(a, b);
}

// ARM-LABEL: test_crc32ch
// AArch32: call i32 @llvm.arm.crc32ch
// AArch64: call i32 @llvm.aarch64.crc32ch
uint32_t test_crc32ch(uint32_t a, uint16_t b) {
  return __crc32ch(a, b);
}

// ARM-LABEL: test_crc32cw
// AArch32: call i32 @llvm.arm.crc32cw
// AArch64: call i32 @llvm.aarch64.crc32cw
uint32_t test_crc32cw(uint32_t a, uint32_t b) {
  return __crc32cw(a, b);
}

// ARM-LABEL: test_crc32cd
// AArch32: call i32 @llvm.arm.crc32cw
// AArch32: call i32 @llvm.arm.crc32cw
// AArch64: call i32 @llvm.aarch64.crc32cx
uint32_t test_crc32cd(uint32_t a, uint64_t b) {
  return __crc32cd(a, b);
}
