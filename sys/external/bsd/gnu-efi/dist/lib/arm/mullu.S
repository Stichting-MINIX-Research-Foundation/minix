/*	$NetBSD: mullu.S,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
#
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
#------------------------------------------------------------------------------

#include "edk2asm.h"

.text
GCC_ASM_EXPORT(__aeabi_lmul)
#
#INT64
#EFIAPI
#__aeabi_lmul (
#  IN INT64   Multiplicand
#  IN INT64   Multiplier
#  );
#
ASM_PFX(__aeabi_lmul):
  stmdb   sp!, {lr}
  mov     lr, r0
  umull   r0, ip, r2, lr
  mla     r1, r2, r1, ip
  mla     r1, r3, lr, r1
  ldmia   sp!, {pc}
