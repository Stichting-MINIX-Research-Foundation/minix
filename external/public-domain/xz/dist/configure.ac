#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

###############################################################################
#
# Author: Lasse Collin
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
#
###############################################################################

# NOTE: Don't add useless checks. autoscan detects this and that, but don't
# let it confuse you. For example, we don't care about checking for behavior
# of malloc(), stat(), or lstat(), since we don't use those functions in
# a way that would cause the problems the autoconf macros check.

AC_PREREQ([2.52])

AC_INIT([XZ Utils], 5.0.0, [joerg@NetBSD.org], [xz], [http://tukaani.org/xz/])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_AUX_DIR([../../gnu/dist/autoconf/config])

AC_CANONICAL_HOST
AC_USE_SYSTEM_EXTENSIONS

###########
# Filters #
###########

m4_define([SUPPORTED_FILTERS], [lzma1,lzma2,delta,x86,powerpc,ia64,arm,armthumb,sparc])

m4_foreach([NAME], [SUPPORTED_FILTERS], [
	AC_DEFINE(HAVE_ENCODER_[]m4_toupper(NAME), [1],
	    [Define to 1 if] NAME [encoder is enabled.])
	AC_DEFINE(HAVE_DECODER_[]m4_toupper(NAME), [1],
	    [Define to 1 if] NAME [decoder is enabled.])
])

#################
# Match finders #
#################

m4_define([SUPPORTED_MATCH_FINDERS], [hc3,hc4,bt2,bt3,bt4])

m4_foreach([NAME], [SUPPORTED_MATCH_FINDERS], [
	AC_DEFINE(HAVE_MF_[]m4_toupper(NAME), [1],
	    [Define to 1 to enable] NAME [match finder.])
])

####################
# Integrity checks #
####################

m4_define([SUPPORTED_CHECKS], [crc32,crc64,sha256])

m4_foreach([NAME], [SUPPORTED_CHECKS], [
	AC_DEFINE(HAVE_CHECK_[]m4_toupper(NAME), [1],
	    [Define to 1 if] NAME [integrity check is enabled.])
])


###########################
# Assembler optimizations #
###########################

AM_CONDITIONAL(COND_ASM_X86, false)
AM_CONDITIONAL(COND_ASM_X86_64, false)


#####################
# Size optimization #
#####################

if false; then
	AC_DEFINE([HAVE_SMALL], [1], [Define to 1 if optimizing for size.])
fi


AC_DEFINE_UNQUOTED([ASSUME_RAM], [128],
    [How many MiB of RAM to assume if the real amount cannot be determined.])

# There is currently no workarounds in this package if some of
# these headers are missing.
AC_CHECK_HEADERS([fcntl.h limits.h sys/time.h],
	[],
	[AC_MSG_ERROR([Required header file(s) are missing.])])

AC_HEADER_STDBOOL

AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_INT32_T
AC_TYPE_UINT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT64_T
AC_TYPE_UINTPTR_T

AC_CHECK_SIZEOF([size_t])

# The command line tool can copy high resolution timestamps if such
# information is availabe in struct stat. Otherwise one second accuracy
# is used.
AC_CHECK_MEMBERS([
	struct stat.st_atim.tv_nsec,
	struct stat.st_atimespec.tv_nsec,
	struct stat.st_atimensec,
	struct stat.st_uatime,
	struct stat.st_atim.st__tim.tv_nsec])

AC_SYS_LARGEFILE
AC_C_BIGENDIAN

# Find the best function to set timestamps.
AC_CHECK_FUNCS([futimens futimes futimesat utimes utime], [break])

TUKLIB_PROGNAME
TUKLIB_INTEGER
TUKLIB_PHYSMEM
TUKLIB_CPUCORES
TUKLIB_MBSTR

AC_OUTPUT
