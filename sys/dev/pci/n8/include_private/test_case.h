/*-
 * Copyright (C) 2001-2003 by NBMK Encryption Technologies.
 * All rights reserved.
 *
 * NBMK Encryption Technologies provides no support of any kind for
 * this software.  Questions or concerns about it may be addressed to
 * the members of the relevant open-source community at
 * <tech-crypto@netbsd.org>.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TEST_CASE_H
#define TEST_CASE_H

#include <stdio.h>
#include "n8_pub_buffer.h"

#define OPCODE_STR_LENGTH        4

typedef struct{
   char token[OPCODE_STR_LENGTH + 2];
   int seq_number;
   int use_sks;
   int sks_addr;
   ATTR m;
   ATTR a;
   ATTR b;
   ATTR c;
   ATTR r;
   ATTR opnd[7];
} TEST_CASE;

#define RSA_P_Operand               0
#define RSA_Q_Operand               1
#define RSA_D_Operand               2
#define RSA_N_Operand               3
#define RSA_U_Operand               4
#define RSA_E_Operand               5
#define RSA_I_Operand               6

#define DSA_N_Operand               0
#define DSA_E_Operand               1
#define DSA_P_Operand               2
#define DSA_G_Operand               3
#define DSA_Q_Operand               4
#define DSA_X_Operand               5
#define DSA_Y_Operand               6

#define TC_SUCCESS                  0
#define TC_GENERIC_ERROR           -1
#define TC_IMPROPER_IO_FORMAT      -2
#define TC_OPEN_ERR                -3
#define TC_READ_ERR                -4
#define TC_WRITE_ERR               -5
#define TC_FSCANF_ERR              -6
#define TC_MALLOC_ERR              -7
#define TC_EOF                     -8

int read_test_case(FILE *, TEST_CASE *);
#endif                                                      
