/* gmp-mparam.h -- Compiler/machine parameter header file.

Copyright 1991, 1993, 1994, 1999, 2000, 2001, 2002, 2003, 2009, 2010 Free
Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at your
option) any later version.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
License for more details.

You should have received a copy of the GNU Lesser General Public License
along with the GNU MP Library.  If not, see http://www.gnu.org/licenses/.  */

#define GMP_LIMB_BITS 32
#define BYTES_PER_MP_LIMB 4

/* 593MHz ARM (gcc50.fsffrance.org) */

#define DIVREM_1_NORM_THRESHOLD              0  /* preinv always */
#define DIVREM_1_UNNORM_THRESHOLD            0  /* always */
#define MOD_1_NORM_THRESHOLD                 0  /* always */
#define MOD_1_UNNORM_THRESHOLD               0  /* always */
#define MOD_1N_TO_MOD_1_1_THRESHOLD         17
#define MOD_1U_TO_MOD_1_1_THRESHOLD          9
#define MOD_1_1_TO_MOD_1_2_THRESHOLD     MP_SIZE_T_MAX
#define MOD_1_2_TO_MOD_1_4_THRESHOLD     MP_SIZE_T_MAX
#define PREINV_MOD_1_TO_MOD_1_THRESHOLD     27
#define USE_PREINV_DIVREM_1                  1  /* preinv always */
#define DIVREM_2_THRESHOLD                   0  /* preinv always */
#define DIVEXACT_1_THRESHOLD                 0  /* always */
#define BMOD_1_TO_MOD_1_THRESHOLD           44

#define MUL_TOOM22_THRESHOLD                34
#define MUL_TOOM33_THRESHOLD               121
#define MUL_TOOM44_THRESHOLD               191
#define MUL_TOOM6H_THRESHOLD               366
#define MUL_TOOM8H_THRESHOLD               547

#define MUL_TOOM32_TO_TOOM43_THRESHOLD     129
#define MUL_TOOM32_TO_TOOM53_THRESHOLD     191
#define MUL_TOOM42_TO_TOOM53_THRESHOLD     117
#define MUL_TOOM42_TO_TOOM63_THRESHOLD     137

#define SQR_BASECASE_THRESHOLD              13
#define SQR_TOOM2_THRESHOLD                 78
#define SQR_TOOM3_THRESHOLD                141
#define SQR_TOOM4_THRESHOLD                212
#define SQR_TOOM6_THRESHOLD                330
#define SQR_TOOM8_THRESHOLD                422

#define MULMOD_BNM1_THRESHOLD               21
#define SQRMOD_BNM1_THRESHOLD               25

#define MUL_FFT_MODF_THRESHOLD             404  /* k = 5 */
#define MUL_FFT_TABLE3                                      \
  { {    404, 5}, {     21, 6}, {     11, 5}, {     25, 6}, \
    {     13, 5}, {     27, 6}, {     28, 7}, {     15, 6}, \
    {     32, 7}, {     17, 6}, {     35, 7}, {     19, 6}, \
    {     39, 7}, {     21, 6}, {     43, 7}, {     29, 8}, \
    {     15, 7}, {     35, 8}, {     19, 7}, {     41, 8}, \
    {     23, 7}, {     49, 8}, {     27, 7}, {     55, 9}, \
    {     15, 8}, {     31, 7}, {     63, 8}, {     43, 9}, \
    {     23, 8}, {     55, 9}, {     31, 8}, {     71, 9}, \
    {     39, 8}, {     83, 9}, {     47, 8}, {     99, 9}, \
    {     55,10}, {     31, 9}, {     63, 8}, {    127, 9}, \
    {     79,10}, {     47, 9}, {    103,11}, {     31,10}, \
    {     63, 9}, {    135,10}, {     95, 9}, {    191,10}, \
    {    111,11}, {     63,10}, {    127, 9}, {    255,10}, \
    {    143, 9}, {    287,10}, {    159, 9}, {    319,11}, \
    {     95,10}, {    191, 9}, {    383,10}, {    207,12}, \
    {     63,11}, {    127,10}, {    287,11}, {    159,10}, \
    {    351,11}, {    191,10}, {    415,11}, {    223,12}, \
    {    127,11}, {    255,10}, {    511,11}, {    319,10}, \
    {    639,11}, {    351,12}, {    191,11}, {    415,13}, \
    {   8192,14}, {  16384,15}, {  32768,16} }
#define MUL_FFT_TABLE3_SIZE 79
#define MUL_FFT_THRESHOLD                 5760

#define SQR_FFT_MODF_THRESHOLD             400  /* k = 5 */
#define SQR_FFT_TABLE3                                      \
  { {    400, 5}, {     25, 6}, {     13, 5}, {     27, 6}, \
    {     25, 7}, {     13, 6}, {     28, 7}, {     15, 6}, \
    {     32, 7}, {     19, 6}, {     39, 7}, {     29, 8}, \
    {     15, 7}, {     35, 8}, {     19, 7}, {     41, 8}, \
    {     23, 7}, {     47, 8}, {     27, 7}, {     55, 9}, \
    {     15, 8}, {     39, 9}, {     23, 8}, {     55,10}, \
    {     15, 9}, {     31, 8}, {     67, 9}, {     39, 8}, \
    {     79, 9}, {     47, 8}, {     95, 9}, {     55,10}, \
    {     31, 9}, {     79,10}, {     47, 9}, {    103,11}, \
    {     31,10}, {     63, 9}, {    135,10}, {     79, 9}, \
    {    159, 8}, {    319,10}, {     95, 9}, {    191,10}, \
    {    111,11}, {     63,10}, {    127, 9}, {    271,10}, \
    {    143, 9}, {    303,10}, {    159,11}, {     95,10}, \
    {    191, 9}, {    383,10}, {    207,12}, {     63,11}, \
    {    127,10}, {    303,11}, {    159,10}, {    367,11}, \
    {    191,10}, {    415,11}, {    223,10}, {    447,12}, \
    {    127,11}, {    255,10}, {    511,11}, {    287,10}, \
    {    607,11}, {    319,10}, {    639,11}, {    351,12}, \
    {    191,11}, {    447,13}, {   8192,14}, {  16384,15}, \
    {  32768,16} }
#define SQR_FFT_TABLE3_SIZE 77
#define SQR_FFT_THRESHOLD                 3136

#define MULLO_BASECASE_THRESHOLD             0  /* always */
#define MULLO_DC_THRESHOLD                 120
#define MULLO_MUL_N_THRESHOLD            11317

#define DC_DIV_QR_THRESHOLD                134
#define DC_DIVAPPR_Q_THRESHOLD             442
#define DC_BDIV_QR_THRESHOLD               127
#define DC_BDIV_Q_THRESHOLD                296

#define INV_MULMOD_BNM1_THRESHOLD           66
#define INV_NEWTON_THRESHOLD               458
#define INV_APPR_THRESHOLD                 454

#define BINV_NEWTON_THRESHOLD              494
#define REDC_1_TO_REDC_N_THRESHOLD         116

#define MU_DIV_QR_THRESHOLD               2914
#define MU_DIVAPPR_Q_THRESHOLD            3091
#define MUPI_DIV_QR_THRESHOLD              221
#define MU_BDIV_QR_THRESHOLD              2259
#define MU_BDIV_Q_THRESHOLD               2747

#define MATRIX22_STRASSEN_THRESHOLD         17
#define HGCD_THRESHOLD                     109
#define GCD_DC_THRESHOLD                   697
#define GCDEXT_DC_THRESHOLD                535
#define JACOBI_BASE_METHOD                   2

#define GET_STR_DC_THRESHOLD                14
#define GET_STR_PRECOMPUTE_THRESHOLD        29
#define SET_STR_DC_THRESHOLD               321
#define SET_STR_PRECOMPUTE_THRESHOLD      1037
