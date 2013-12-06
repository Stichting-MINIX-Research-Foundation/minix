/* config.h.  Generated from config.in by configure.  */
/* config.in.  Generated from configure.ac by autoheader.  */

/*

Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
2007, 2008, 2009, 2010, 2011, 2012, 2013 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published
by the Free Software Foundation; either version 3 of the License, or (at
your option) any later version.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
License for more details.

You should have received a copy of the GNU Lesser General Public License
along with the GNU MP Library.  If not, see http://www.gnu.org/licenses/.
*/

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* The gmp-mparam.h file (a string) the tune program should suggest updating.
   */
#define GMP_MPARAM_H_SUGGEST "./mpn/generic/gmp-mparam.h"

/* Define to 1 if you have the `alarm' function. */
#define HAVE_ALARM 1

/* Define to 1 if alloca() works (via gmp-impl.h). */
#define HAVE_ALLOCA 1

/* Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
   */
/* #undef HAVE_ALLOCA_H */

/* Define to 1 if the compiler accepts gcc style __attribute__ ((const)) */
#define HAVE_ATTRIBUTE_CONST 1

/* Define to 1 if the compiler accepts gcc style __attribute__ ((malloc)) */
#define HAVE_ATTRIBUTE_MALLOC 1

/* Define to 1 if the compiler accepts gcc style __attribute__ ((mode (XX)))
   */
#define HAVE_ATTRIBUTE_MODE 1

/* Define to 1 if the compiler accepts gcc style __attribute__ ((noreturn)) */
#define HAVE_ATTRIBUTE_NORETURN 1

/* Define to 1 if you have the `attr_get' function. */
/* #undef HAVE_ATTR_GET */

/* Define to 1 if tests/libtests has calling conventions checking for the CPU
   */
/* #undef HAVE_CALLING_CONVENTIONS */

/* Define to 1 if you have the `clock' function. */
#define HAVE_CLOCK 1

/* Define to 1 if you have the `clock_gettime' function */
#define HAVE_CLOCK_GETTIME 1

/* Define to 1 if you have the `cputime' function. */
/* #undef HAVE_CPUTIME */

/* Define to 1 if you have the declaration of `fgetc', and to 0 if you don't.
   */
#define HAVE_DECL_FGETC 1

/* Define to 1 if you have the declaration of `fscanf', and to 0 if you don't.
   */
#define HAVE_DECL_FSCANF 1

/* Define to 1 if you have the declaration of `optarg', and to 0 if you don't.
   */
#define HAVE_DECL_OPTARG 1

/* Define to 1 if you have the declaration of `sys_errlist', and to 0 if you
   don't. */
#define HAVE_DECL_SYS_ERRLIST 1

/* Define to 1 if you have the declaration of `sys_nerr', and to 0 if you
   don't. */
#define HAVE_DECL_SYS_NERR 1

/* Define to 1 if you have the declaration of `ungetc', and to 0 if you don't.
   */
#define HAVE_DECL_UNGETC 1

/* Define to 1 if you have the declaration of `vfprintf', and to 0 if you
   don't. */
#define HAVE_DECL_VFPRINTF 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define one of the following to 1 for the format of a `double'.
   If your format is not among these choices, or you don't know what it is,
   then leave all undefined.
   IEEE_LITTLE_SWAPPED means little endian, but with the two 4-byte halves
   swapped, as used by ARM CPUs in little endian mode.  */
/* #undef HAVE_DOUBLE_IEEE_BIG_ENDIAN */
#define HAVE_DOUBLE_IEEE_LITTLE_ENDIAN 1
/* #undef HAVE_DOUBLE_IEEE_LITTLE_SWAPPED */
/* #undef HAVE_DOUBLE_VAX_D */
/* #undef HAVE_DOUBLE_VAX_G */
/* #undef HAVE_DOUBLE_CRAY_CFP */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the <float.h> header file. */
#define HAVE_FLOAT_H 1

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the `getrusage' function. */
#define HAVE_GETRUSAGE 1

/* Define to 1 if you have the `getsysinfo' function. */
/* #undef HAVE_GETSYSINFO */

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define one of these to 1 for the host CPU family.
   If your CPU is not in any of these families, leave all undefined.
   For an AMD64 chip, define "x86" in ABI=32, but not in ABI=64. */
/* #undef HAVE_HOST_CPU_FAMILY_alpha */
/* #undef HAVE_HOST_CPU_FAMILY_m68k */
/* #undef HAVE_HOST_CPU_FAMILY_power */
/* #undef HAVE_HOST_CPU_FAMILY_powerpc */
/* #undef HAVE_HOST_CPU_FAMILY_x86 */
/* #undef HAVE_HOST_CPU_FAMILY_x86_64 */

/* Define one of the following to 1 for the host CPU, as per the output of
   ./config.guess.  If your CPU is not listed here, leave all undefined.  */
/* #undef HAVE_HOST_CPU_alphaev67 */
/* #undef HAVE_HOST_CPU_alphaev68 */
/* #undef HAVE_HOST_CPU_alphaev7 */
/* #undef HAVE_HOST_CPU_m68020 */
/* #undef HAVE_HOST_CPU_m68030 */
/* #undef HAVE_HOST_CPU_m68040 */
/* #undef HAVE_HOST_CPU_m68060 */
/* #undef HAVE_HOST_CPU_m68360 */
/* #undef HAVE_HOST_CPU_powerpc604 */
/* #undef HAVE_HOST_CPU_powerpc604e */
/* #undef HAVE_HOST_CPU_powerpc750 */
/* #undef HAVE_HOST_CPU_powerpc7400 */
/* #undef HAVE_HOST_CPU_supersparc */
/* #undef HAVE_HOST_CPU_i386 */
/* #undef HAVE_HOST_CPU_i586 */
/* #undef HAVE_HOST_CPU_i686 */
/* #undef HAVE_HOST_CPU_pentium */
/* #undef HAVE_HOST_CPU_pentiummmx */
/* #undef HAVE_HOST_CPU_pentiumpro */
/* #undef HAVE_HOST_CPU_pentium2 */
/* #undef HAVE_HOST_CPU_pentium3 */
/* #undef HAVE_HOST_CPU_s390_z900 */
/* #undef HAVE_HOST_CPU_s390_z990 */
/* #undef HAVE_HOST_CPU_s390_z9 */
/* #undef HAVE_HOST_CPU_s390_z10 */
/* #undef HAVE_HOST_CPU_s390_z196 */

/* Define to 1 iff we have a s390 with 64-bit registers.  */
/* #undef HAVE_HOST_CPU_s390_zarch */

/* Define to 1 if the system has the type `intmax_t'. */
#define HAVE_INTMAX_T 1

/* Define to 1 if the system has the type `intptr_t'. */
#define HAVE_INTPTR_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <invent.h> header file. */
/* #undef HAVE_INVENT_H */

/* Define to 1 if you have the <langinfo.h> header file. */
#define HAVE_LANGINFO_H 1

/* Define one of these to 1 for the endianness of `mp_limb_t'.
   If the endianness is not a simple big or little, or you don't know what
   it is, then leave both undefined. */
/* #undef HAVE_LIMB_BIG_ENDIAN */
#define HAVE_LIMB_LITTLE_ENDIAN 1

/* Define to 1 if you have the `localeconv' function. */
#define HAVE_LOCALECONV 1

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if the system has the type `long double'. */
#define HAVE_LONG_DOUBLE 1

/* Define to 1 if the system has the type `long long'. */
#define HAVE_LONG_LONG 1

/* Define to 1 if you have the <machine/hal_sysinfo.h> header file. */
/* #undef HAVE_MACHINE_HAL_SYSINFO_H */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the `mmap' function. */
#define HAVE_MMAP 1

/* Define to 1 if you have the `mprotect' function. */
#define HAVE_MPROTECT 1

/* Define to 1 each of the following for which a native (ie. CPU specific)
    implementation of the corresponding routine exists.  */
/* #undef HAVE_NATIVE_mpn_add_n */
/* #undef HAVE_NATIVE_mpn_add_n_sub_n */
/* #undef HAVE_NATIVE_mpn_add_nc */
/* #undef HAVE_NATIVE_mpn_addaddmul_1msb0 */
/* #undef HAVE_NATIVE_mpn_addcnd_n */
/* #undef HAVE_NATIVE_mpn_addlsh1_n */
/* #undef HAVE_NATIVE_mpn_addlsh2_n */
/* #undef HAVE_NATIVE_mpn_addlsh_n */
/* #undef HAVE_NATIVE_mpn_addlsh1_nc */
/* #undef HAVE_NATIVE_mpn_addlsh2_nc */
/* #undef HAVE_NATIVE_mpn_addlsh_nc */
/* #undef HAVE_NATIVE_mpn_addlsh1_n_ip1 */
/* #undef HAVE_NATIVE_mpn_addlsh2_n_ip1 */
/* #undef HAVE_NATIVE_mpn_addlsh_n_ip1 */
/* #undef HAVE_NATIVE_mpn_addlsh1_nc_ip1 */
/* #undef HAVE_NATIVE_mpn_addlsh2_nc_ip1 */
/* #undef HAVE_NATIVE_mpn_addlsh_nc_ip1 */
/* #undef HAVE_NATIVE_mpn_addlsh1_n_ip2 */
/* #undef HAVE_NATIVE_mpn_addlsh2_n_ip2 */
/* #undef HAVE_NATIVE_mpn_addlsh_n_ip2 */
/* #undef HAVE_NATIVE_mpn_addlsh1_nc_ip2 */
/* #undef HAVE_NATIVE_mpn_addlsh2_nc_ip2 */
/* #undef HAVE_NATIVE_mpn_addlsh_nc_ip2 */
/* #undef HAVE_NATIVE_mpn_addmul_1c */
/* #undef HAVE_NATIVE_mpn_addmul_2 */
/* #undef HAVE_NATIVE_mpn_addmul_3 */
/* #undef HAVE_NATIVE_mpn_addmul_4 */
/* #undef HAVE_NATIVE_mpn_addmul_5 */
/* #undef HAVE_NATIVE_mpn_addmul_6 */
/* #undef HAVE_NATIVE_mpn_addmul_7 */
/* #undef HAVE_NATIVE_mpn_addmul_8 */
/* #undef HAVE_NATIVE_mpn_addmul_2s */
/* #undef HAVE_NATIVE_mpn_and_n */
/* #undef HAVE_NATIVE_mpn_andn_n */
/* #undef HAVE_NATIVE_mpn_bdiv_dbm1c */
/* #undef HAVE_NATIVE_mpn_bdiv_q_1 */
/* #undef HAVE_NATIVE_mpn_pi1_bdiv_q_1 */
/* #undef HAVE_NATIVE_mpn_com */
/* #undef HAVE_NATIVE_mpn_copyd */
/* #undef HAVE_NATIVE_mpn_copyi */
/* #undef HAVE_NATIVE_mpn_div_qr_2 */
/* #undef HAVE_NATIVE_mpn_divexact_1 */
/* #undef HAVE_NATIVE_mpn_divexact_by3c */
/* #undef HAVE_NATIVE_mpn_divrem_1 */
/* #undef HAVE_NATIVE_mpn_divrem_1c */
/* #undef HAVE_NATIVE_mpn_divrem_2 */
/* #undef HAVE_NATIVE_mpn_gcd_1 */
/* #undef HAVE_NATIVE_mpn_hamdist */
/* #undef HAVE_NATIVE_mpn_invert_limb */
/* #undef HAVE_NATIVE_mpn_ior_n */
/* #undef HAVE_NATIVE_mpn_iorn_n */
/* #undef HAVE_NATIVE_mpn_lshift */
/* #undef HAVE_NATIVE_mpn_lshiftc */
/* #undef HAVE_NATIVE_mpn_lshsub_n */
/* #undef HAVE_NATIVE_mpn_mod_1 */
/* #undef HAVE_NATIVE_mpn_mod_1_1p */
/* #undef HAVE_NATIVE_mpn_mod_1c */
/* #undef HAVE_NATIVE_mpn_mod_1s_2p */
/* #undef HAVE_NATIVE_mpn_mod_1s_4p */
/* #undef HAVE_NATIVE_mpn_mod_34lsub1 */
/* #undef HAVE_NATIVE_mpn_modexact_1_odd */
/* #undef HAVE_NATIVE_mpn_modexact_1c_odd */
/* #undef HAVE_NATIVE_mpn_mul_1 */
/* #undef HAVE_NATIVE_mpn_mul_1c */
/* #undef HAVE_NATIVE_mpn_mul_2 */
/* #undef HAVE_NATIVE_mpn_mul_3 */
/* #undef HAVE_NATIVE_mpn_mul_4 */
/* #undef HAVE_NATIVE_mpn_mul_5 */
/* #undef HAVE_NATIVE_mpn_mul_6 */
/* #undef HAVE_NATIVE_mpn_mul_basecase */
/* #undef HAVE_NATIVE_mpn_nand_n */
/* #undef HAVE_NATIVE_mpn_nior_n */
/* #undef HAVE_NATIVE_mpn_popcount */
/* #undef HAVE_NATIVE_mpn_preinv_divrem_1 */
/* #undef HAVE_NATIVE_mpn_preinv_mod_1 */
/* #undef HAVE_NATIVE_mpn_redc_1 */
/* #undef HAVE_NATIVE_mpn_redc_2 */
/* #undef HAVE_NATIVE_mpn_rsblsh1_n */
/* #undef HAVE_NATIVE_mpn_rsblsh2_n */
/* #undef HAVE_NATIVE_mpn_rsblsh_n */
/* #undef HAVE_NATIVE_mpn_rsblsh1_nc */
/* #undef HAVE_NATIVE_mpn_rsblsh2_nc */
/* #undef HAVE_NATIVE_mpn_rsblsh_nc */
/* #undef HAVE_NATIVE_mpn_rsh1add_n */
/* #undef HAVE_NATIVE_mpn_rsh1add_nc */
/* #undef HAVE_NATIVE_mpn_rsh1sub_n */
/* #undef HAVE_NATIVE_mpn_rsh1sub_nc */
/* #undef HAVE_NATIVE_mpn_rshift */
/* #undef HAVE_NATIVE_mpn_sqr_basecase */
/* #undef HAVE_NATIVE_mpn_sqr_diagonal */
/* #undef HAVE_NATIVE_mpn_sqr_diag_addlsh1 */
/* #undef HAVE_NATIVE_mpn_sub_n */
/* #undef HAVE_NATIVE_mpn_sub_nc */
/* #undef HAVE_NATIVE_mpn_subcnd_n */
/* #undef HAVE_NATIVE_mpn_sublsh1_n */
/* #undef HAVE_NATIVE_mpn_sublsh2_n */
/* #undef HAVE_NATIVE_mpn_sublsh_n */
/* #undef HAVE_NATIVE_mpn_sublsh1_nc */
/* #undef HAVE_NATIVE_mpn_sublsh2_nc */
/* #undef HAVE_NATIVE_mpn_sublsh_nc */
/* #undef HAVE_NATIVE_mpn_sublsh1_n_ip1 */
/* #undef HAVE_NATIVE_mpn_sublsh2_n_ip1 */
/* #undef HAVE_NATIVE_mpn_sublsh_n_ip1 */
/* #undef HAVE_NATIVE_mpn_sublsh1_nc_ip1 */
/* #undef HAVE_NATIVE_mpn_sublsh2_nc_ip1 */
/* #undef HAVE_NATIVE_mpn_sublsh_nc_ip1 */
/* #undef HAVE_NATIVE_mpn_submul_1c */
/* #undef HAVE_NATIVE_mpn_tabselect */
/* #undef HAVE_NATIVE_mpn_udiv_qrnnd */
/* #undef HAVE_NATIVE_mpn_udiv_qrnnd_r */
/* #undef HAVE_NATIVE_mpn_umul_ppmm */
/* #undef HAVE_NATIVE_mpn_umul_ppmm_r */
/* #undef HAVE_NATIVE_mpn_xor_n */
/* #undef HAVE_NATIVE_mpn_xnor_n */

/* Define to 1 if you have the `nl_langinfo' function. */
#define HAVE_NL_LANGINFO 1

/* Define to 1 if you have the <nl_types.h> header file. */
#define HAVE_NL_TYPES_H 1

/* Define to 1 if you have the `obstack_vprintf' function. */
/* #undef HAVE_OBSTACK_VPRINTF */

/* Define to 1 if you have the `popen' function. */
#define HAVE_POPEN 1

/* Define to 1 if you have the `processor_info' function. */
/* #undef HAVE_PROCESSOR_INFO */

/* Define to 1 if <sys/pstat.h> `struct pst_processor' exists and contains
   `psp_iticksperclktick'. */
/* #undef HAVE_PSP_ITICKSPERCLKTICK */

/* Define to 1 if you have the `pstat_getprocessor' function. */
/* #undef HAVE_PSTAT_GETPROCESSOR */

/* Define to 1 if the system has the type `ptrdiff_t'. */
#define HAVE_PTRDIFF_T 1

/* Define to 1 if the system has the type `quad_t'. */
#define HAVE_QUAD_T 1

/* Define to 1 if you have the `raise' function. */
#define HAVE_RAISE 1

/* Define to 1 if you have the `read_real_time' function. */
/* #undef HAVE_READ_REAL_TIME */

/* Define to 1 if you have the `sigaction' function. */
#define HAVE_SIGACTION 1

/* Define to 1 if you have the `sigaltstack' function. */
#define HAVE_SIGALTSTACK 1

/* Define to 1 if you have the `sigstack' function. */
/* #undef HAVE_SIGSTACK */

/* Tune directory speed_cyclecounter, undef=none, 1=32bits, 2=64bits) */
/* #undef HAVE_SPEED_CYCLECOUNTER */

/* Define to 1 if you have the <sstream> header file. */
/* #undef HAVE_SSTREAM */

/* Define to 1 if the system has the type `stack_t'. */
#define HAVE_STACK_T 1

/* Define to 1 if <stdarg.h> exists and works */
#define HAVE_STDARG 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if the system has the type `std::locale'. */
/* #undef HAVE_STD__LOCALE */

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if cpp supports the ANSI # stringizing operator. */
#define HAVE_STRINGIZE 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strnlen' function. */
#define HAVE_STRNLEN 1

/* Define to 1 if you have the `strtol' function. */
#define HAVE_STRTOL 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define to 1 if you have the `sysconf' function. */
#define HAVE_SYSCONF 1

/* Define to 1 if you have the `sysctl' function. */
#define HAVE_SYSCTL 1

/* Define to 1 if you have the `sysctlbyname' function. */
#define HAVE_SYSCTLBYNAME 1

/* Define to 1 if you have the `syssgi' function. */
/* #undef HAVE_SYSSGI */

/* Define to 1 if you have the <sys/attributes.h> header file. */
/* #undef HAVE_SYS_ATTRIBUTES_H */

/* Define to 1 if you have the <sys/iograph.h> header file. */
/* #undef HAVE_SYS_IOGRAPH_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/processor.h> header file. */
/* #undef HAVE_SYS_PROCESSOR_H */

/* Define to 1 if you have the <sys/pstat.h> header file. */
/* #undef HAVE_SYS_PSTAT_H */

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/sysctl.h> header file. */
#define HAVE_SYS_SYSCTL_H 1

/* Define to 1 if you have the <sys/sysinfo.h> header file. */
/* #undef HAVE_SYS_SYSINFO_H */

/* Define to 1 if you have the <sys/syssgi.h> header file. */
/* #undef HAVE_SYS_SYSSGI_H */

/* Define to 1 if you have the <sys/systemcfg.h> header file. */
/* #undef HAVE_SYS_SYSTEMCFG_H */

/* Define to 1 if you have the <sys/times.h> header file. */
#define HAVE_SYS_TIMES_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the `times' function. */
#define HAVE_TIMES 1

/* Define to 1 if the system has the type `uint_least32_t'. */
#define HAVE_UINT_LEAST32_T 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `vsnprintf' function and it works properly. */
#define HAVE_VSNPRINTF 1

/* Define to 1 for Windos/64 */
/* #undef HOST_DOS64 */

/* Assembler local label prefix */
/* #undef LSYM_PREFIX */

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "gmp"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "gmp-bugs@gmplib.org, see http://gmplib.org/manual/Reporting-Bugs.html"

/* Define to the full name of this package. */
#define PACKAGE_NAME "GNU MP"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "GNU MP 5.1.3"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "gmp"

/* Define to the home page for this package. */
#define PACKAGE_URL "http://www.gnu.org/software/gmp/"

/* Define to the version of this package. */
#define PACKAGE_VERSION "5.1.3"

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* The size of `mp_limb_t', as computed by sizeof. */
#define SIZEOF_MP_LIMB_T 4

/* The size of `unsigned', as computed by sizeof. */
#define SIZEOF_UNSIGNED 4

/* The size of `unsigned long', as computed by sizeof. */
#define SIZEOF_UNSIGNED_LONG 4

/* The size of `unsigned short', as computed by sizeof. */
#define SIZEOF_UNSIGNED_SHORT 2

/* The size of `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 4

/* Define to 1 if sscanf requires writable inputs */
/* #undef SSCANF_WRITABLE_INPUT */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Maximum size the tune program can test for SQR_TOOM2_THRESHOLD */
#define TUNE_SQR_TOOM2_MAX SQR_TOOM2_MAX_GENERIC

/* Version number of package */
#define VERSION "5.1.3"

/* Defined to 1 as per --enable-assembly */
#define WANT_ASSEMBLY 1

/* Define to 1 to enable ASSERT checking, per --enable-assert */
/* #undef WANT_ASSERT */

/* Define to 1 when building a fat binary. */
/* #undef WANT_FAT_BINARY */

/* Define to 1 to enable FFTs for multiplication, per --enable-fft */
#define WANT_FFT 1

/* Define to 1 to enable old mpn_mul_fft_full for multiplication, per
   --enable-old-fft-full */
/* #undef WANT_OLD_FFT_FULL */

/* Define to 1 if --enable-profiling=gprof */
/* #undef WANT_PROFILING_GPROF */

/* Define to 1 if --enable-profiling=instrument */
/* #undef WANT_PROFILING_INSTRUMENT */

/* Define to 1 if --enable-profiling=prof */
/* #undef WANT_PROFILING_PROF */

/* Define one of these to 1 for the desired temporary memory allocation
   method, per --enable-alloca. */
#define WANT_TMP_ALLOCA 1
/* #undef WANT_TMP_REENTRANT */
/* #undef WANT_TMP_NOTREENTRANT */
/* #undef WANT_TMP_DEBUG */

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a
   `char[]'. */
#define YYTEXT_POINTER 1

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to the equivalent of the C99 'restrict' keyword, or to
   nothing if this is not supported.  Do not define if restrict is
   supported directly.  */
#define restrict __restrict
/* Work around a bug in Sun C++: it does not support _Restrict or
   __restrict__, even though the corresponding Sun C compiler ends up with
   "#define restrict _Restrict" or "#define restrict __restrict__" in the
   previous line.  Perhaps some future version of Sun C++ will work with
   restrict; if so, hopefully it defines __RESTRICT like Sun C does.  */
#if defined __SUNPRO_CC && !defined __RESTRICT
# define _Restrict
# define __restrict__
#endif

/* Define to empty if the keyword `volatile' does not work. Warning: valid
   code using `volatile' can become incorrect without. Disable with care. */
/* #undef volatile */
