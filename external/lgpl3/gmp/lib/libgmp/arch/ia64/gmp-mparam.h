/* gmp-mparam.h -- Compiler/machine parameter header file.

Copyright 2000, 2001, 2002, 2003, 2004, 2005, 2009, 2010 Free Software
Foundation, Inc.

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

#define GMP_LIMB_BITS 64
#define BYTES_PER_MP_LIMB 8

/* 1300MHz Itanium2 (babe.fsffrance.org) */


#define MOD_1_NORM_THRESHOLD                 0  /* always */
#define MOD_1_UNNORM_THRESHOLD               0  /* always */
#define MOD_1N_TO_MOD_1_1_THRESHOLD         14
#define MOD_1U_TO_MOD_1_1_THRESHOLD          8
#define MOD_1_1_TO_MOD_1_2_THRESHOLD         0
#define MOD_1_2_TO_MOD_1_4_THRESHOLD        14
#define PREINV_MOD_1_TO_MOD_1_THRESHOLD     22
#define USE_PREINV_DIVREM_1                  1  /* native */
#define DIVEXACT_1_THRESHOLD                 0  /* always (native) */
#define BMOD_1_TO_MOD_1_THRESHOLD        MP_SIZE_T_MAX  /* never */

#define MUL_TOOM22_THRESHOLD                44
#define MUL_TOOM33_THRESHOLD                89
#define MUL_TOOM44_THRESHOLD               232
#define MUL_TOOM6H_THRESHOLD               351
#define MUL_TOOM8H_THRESHOLD               454

#define MUL_TOOM32_TO_TOOM43_THRESHOLD     101
#define MUL_TOOM32_TO_TOOM53_THRESHOLD     160
#define MUL_TOOM42_TO_TOOM53_THRESHOLD     138
#define MUL_TOOM42_TO_TOOM63_THRESHOLD     159

#define SQR_BASECASE_THRESHOLD              26
#define SQR_TOOM2_THRESHOLD                119
#define SQR_TOOM3_THRESHOLD                141
#define SQR_TOOM4_THRESHOLD                282
#define SQR_TOOM6_THRESHOLD                375
#define SQR_TOOM8_THRESHOLD                527

#define MULMOD_BNM1_THRESHOLD               24
#define SQRMOD_BNM1_THRESHOLD               19

#define MUL_FFT_MODF_THRESHOLD             888  /* k = 5 */
#define MUL_FFT_TABLE3                                      \
  { {    888, 5}, {     31, 6}, {     16, 5}, {     33, 6}, \
    {     17, 5}, {     35, 6}, {     28, 7}, {     15, 6}, \
    {     33, 7}, {     17, 6}, {     35, 7}, {     19, 6}, \
    {     39, 7}, {     29, 8}, {     15, 7}, {     33, 8}, \
    {     17, 7}, {     37, 8}, {     19, 7}, {     41, 8}, \
    {     21, 7}, {     43, 8}, {     23, 7}, {     47, 8}, \
    {     27, 7}, {     55, 8}, {     31, 7}, {     63, 8}, \
    {     37, 9}, {     19, 8}, {     43, 9}, {     23, 8}, \
    {     51, 9}, {     27, 8}, {     55, 9}, {     31, 8}, \
    {     63, 9}, {     35, 8}, {     71, 9}, {     39, 8}, \
    {     79, 9}, {     43,10}, {     23, 9}, {     47, 8}, \
    {     95, 9}, {     55,10}, {     31, 9}, {     71,10}, \
    {     39, 9}, {     83,10}, {     47, 9}, {     99,10}, \
    {     55,11}, {     31,10}, {     63, 9}, {    127,10}, \
    {     71, 9}, {    143,10}, {     87,11}, {     47,10}, \
    {    111,12}, {     31,11}, {     63,10}, {    143,11}, \
    {     79,10}, {    167,11}, {     95,10}, {    199,11}, \
    {    111,12}, {     63,11}, {    127,10}, {    255,11}, \
    {    143,10}, {    287,11}, {    159,10}, {    319,12}, \
    {     95,11}, {    223,13}, {     63,12}, {    127,11}, \
    {    287,12}, {    159,11}, {    335,12}, {    191,11}, \
    {    383,10}, {    767,11}, {    399,12}, {    223,13}, \
    {    127,12}, {    255,11}, {    511,10}, {   1023,12}, \
    {    287,11}, {    575,10}, {   1151,12}, {    319,11}, \
    {    639,10}, {   1279,11}, {    671,13}, {    191,12}, \
    {    383,11}, {    767,10}, {   1535,12}, {    415,11}, \
    {    831,14}, {    127,13}, {    255,12}, {    511,11}, \
    {   1023,12}, {    543,11}, {   1087,12}, {    575,13}, \
    {    319,12}, {    639,11}, {   1279,12}, {    671,11}, \
    {   1343,12}, {    703,11}, {   1471,13}, {    383,12}, \
    {    767,11}, {   1535,12}, {    799,11}, {   1599,12}, \
    {    831,13}, {    447,12}, {    959,14}, {    255,13}, \
    {    511,12}, {   1055,11}, {   2111,12}, {   1087,13}, \
    {    575,12}, {   1215,11}, {   2431,12}, {   1247,13}, \
    {    639,12}, {   1279,11}, {   2559,12}, {   1343,13}, \
    {    703,12}, {   1471,14}, {    383,13}, {    767,12}, \
    {   1599,13}, {    831,12}, {   1663,11}, {   3327,12}, \
    {   1727,13}, {    895,12}, {   1791,13}, {    959,15}, \
    {    255,14}, {    511,13}, {   1023,12}, {   2047,13}, \
    {   1087,12}, {   2175,13}, {   1151,12}, {   2303,13}, \
    {   1215,11}, {   4863,12}, {   2495,14}, {    639,13}, \
    {   1343,12}, {   2687,13}, {   1471,12}, {   2943,14}, \
    {    767,13}, {   1599,12}, {   3199,13}, {   1727,12}, \
    {   3455,14}, {    895,13}, {   1983,12}, {   3967,15}, \
    {    511,14}, {   1023,13}, {   2111,12}, {   4223,13}, \
    {   2239,12}, {   4479,13}, {   2495,14}, {   1279,13}, \
    {   2751,14}, {   1407,13}, {   2943,15}, {    767,14}, \
    {   1535,13}, {   3199,14}, {   1663,13}, {   3455,14}, \
    {   1791,12}, {   7167,14}, {   1919,13}, {   3967,16}, \
    {    511,15}, {   1023,14}, {   2175,13}, {   4351,14}, \
    {   2431,15}, {   1279,14}, {   2943,13}, {   5887,15}, \
    {   1535,14}, {   3199,13}, {   6399,14}, {  16384,15}, \
    {  32768,16}, {  65536,17}, { 131072,18}, { 262144,19}, \
    { 524288,20}, {1048576,21}, {2097152,22}, {4194304,23}, \
    {8388608,24} }
#define MUL_FFT_TABLE3_SIZE 217
#define MUL_FFT_THRESHOLD                 9856

#define SQR_FFT_MODF_THRESHOLD             751  /* k = 5 */
#define SQR_FFT_TABLE3                                      \
  { {    751, 5}, {     35, 6}, {     18, 5}, {     37, 6}, \
    {     29, 7}, {     15, 6}, {     33, 7}, {     17, 6}, \
    {     35, 7}, {     29, 8}, {     15, 7}, {     37, 8}, \
    {     19, 7}, {     41, 8}, {     21, 7}, {     43, 8}, \
    {     23, 7}, {     47, 8}, {     43, 9}, {     23, 8}, \
    {     51, 9}, {     27, 8}, {     55, 9}, {     31, 8}, \
    {     63, 9}, {     39, 8}, {     79, 9}, {     43,10}, \
    {     23, 9}, {     47, 8}, {     95, 9}, {     55,10}, \
    {     31, 9}, {     67,10}, {     39, 9}, {     83,10}, \
    {     47, 9}, {     99,10}, {     55,11}, {     31,10}, \
    {     63, 9}, {    127,10}, {     79,11}, {     47,10}, \
    {    103,12}, {     31,11}, {     63,10}, {    143,11}, \
    {     79,10}, {    159,11}, {     95,10}, {    199,11}, \
    {    111,12}, {     63,11}, {    127,10}, {    255,11}, \
    {    143,10}, {    287,11}, {    159,12}, {     95,11}, \
    {    191,10}, {    383,11}, {    207,13}, {     63,12}, \
    {    127,11}, {    255,10}, {    511,11}, {    271,12}, \
    {    159,11}, {    319,10}, {    639,11}, {    335,12}, \
    {    191,11}, {    383,10}, {    767,12}, {    223,13}, \
    {    127,11}, {    511,10}, {   1023,11}, {    527,12}, \
    {    287,11}, {    575,10}, {   1151,11}, {    591,12}, \
    {    319,11}, {    639,13}, {    191,12}, {    383,11}, \
    {    767,10}, {   1535,11}, {    799,10}, {   1599, 9}, \
    {   3199,14}, {    127,13}, {    255,12}, {    511, 9}, \
    {   4095,10}, {   2111,12}, {    543,11}, {   1087,10}, \
    {   2239,12}, {    575,10}, {   2303,13}, {    319,12}, \
    {    671,11}, {   1471,13}, {    383,11}, {   1599,12}, \
    {    831,11}, {   1663,12}, {    863,10}, {   3455,13}, \
    {    447,12}, {    895,11}, {   1791,14}, {    255,13}, \
    {    511,12}, {   1023,11}, {   2111,12}, {   1087,11}, \
    {   2239,13}, {    575,12}, {   1215,11}, {   2495,13}, \
    {    639,12}, {   1343,13}, {    703,12}, {   1407,14}, \
    {    383,13}, {    767,12}, {   1599,13}, {    831,12}, \
    {   1727,11}, {   3455,12}, {   1791,15}, {    255,14}, \
    {    511,13}, {   1023,12}, {   2111,11}, {   4223,12}, \
    {   2239,11}, {   4479,10}, {   8959,11}, {   4607,13}, \
    {   1215,14}, {    639,13}, {   1343,12}, {   2815,13}, \
    {   1471,12}, {   2943,14}, {    767,13}, {   1599,12}, \
    {   3199,13}, {   1727,12}, {   3455,14}, {    895,13}, \
    {   1855,12}, {   3711,13}, {   1983,12}, {   3967,15}, \
    {    511,14}, {   1023,13}, {   2111,12}, {   4223,13}, \
    {   2239,12}, {   4479,14}, {   1151,13}, {   2495,14}, \
    {   1279,13}, {   2687,14}, {   1407,13}, {   2943,15}, \
    {    767,14}, {   1535,13}, {   3071,14}, {   1663,13}, \
    {   3327,14}, {   1791,16}, {    511,15}, {   1023,14}, \
    {   2047,13}, {   4223,14}, {   2175,13}, {   4479,12}, \
    {   8959,14}, {   2303,13}, {   4735,14}, {   2431,15}, \
    {   1279,14}, {   2943,15}, {   1535,14}, {   3071,13}, \
    {   6143,14}, {  16384,15}, {  32768,16}, {  65536,17}, \
    { 131072,18}, { 262144,19}, { 524288,20}, {1048576,21}, \
    {2097152,22}, {4194304,23}, {8388608,24} }
#define SQR_FFT_TABLE3_SIZE 203
#define SQR_FFT_THRESHOLD                 7552

#define MULLO_BASECASE_THRESHOLD            17
#define MULLO_DC_THRESHOLD                  91
#define MULLO_MUL_N_THRESHOLD            19187

#define DC_DIV_QR_THRESHOLD                 72
#define DC_DIVAPPR_Q_THRESHOLD             254
#define DC_BDIV_QR_THRESHOLD               117
#define DC_BDIV_Q_THRESHOLD                292

#define INV_MULMOD_BNM1_THRESHOLD          103
#define INV_NEWTON_THRESHOLD               178
#define INV_APPR_THRESHOLD                 179

#define BINV_NEWTON_THRESHOLD              300
#define REDC_1_TO_REDC_2_THRESHOLD          10
#define REDC_2_TO_REDC_N_THRESHOLD         167

#define MU_DIV_QR_THRESHOLD               1787
#define MU_DIVAPPR_Q_THRESHOLD            1470
#define MUPI_DIV_QR_THRESHOLD                0  /* always */
#define MU_BDIV_QR_THRESHOLD              1787
#define MU_BDIV_Q_THRESHOLD               2089

#define MATRIX22_STRASSEN_THRESHOLD         27
#define HGCD_THRESHOLD                     139
#define GCD_DC_THRESHOLD                   469
#define GCDEXT_DC_THRESHOLD                496
#define JACOBI_BASE_METHOD                   1

#define GET_STR_DC_THRESHOLD                14
#define GET_STR_PRECOMPUTE_THRESHOLD        22
#define SET_STR_DC_THRESHOLD              1474
#define SET_STR_PRECOMPUTE_THRESHOLD      3495
