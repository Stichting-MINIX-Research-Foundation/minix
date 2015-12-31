//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Define a bunch of macros that can be used in the tests instead of
//  implementation defined assumptions:
//   - locale names
//   - floating point number string output

#ifndef PLATFORM_SUPPORT_H
#define PLATFORM_SUPPORT_H

#include <__config>

// locale names
#ifdef _WIN32
// WARNING: Windows does not support UTF-8 codepages.
// Locales are "converted" using http://docs.moodle.org/dev/Table_of_locales
#define LOCALE_en_US_UTF_8     "English_United States.1252"
#define LOCALE_cs_CZ_ISO8859_2 "Czech_Czech Republic.1250"
#define LOCALE_fr_FR_UTF_8     "French_France.1252"
#define LOCALE_fr_CA_ISO8859_1 "French_Canada.1252"
#define LOCALE_ru_RU_UTF_8     "Russian_Russia.1251"
#define LOCALE_zh_CN_UTF_8     "Chinese_China.936"
#elif defined(__CloudABI__)
// Timezones are integrated into locales through LC_TIMEZONE_MASK on
// CloudABI. LC_ALL_MASK can only be used if a timezone has also been
// provided. UTC should be all right.
#define LOCALE_en_US_UTF_8     "en_US.UTF-8@UTC"
#define LOCALE_fr_FR_UTF_8     "fr_FR.UTF-8@UTC"
#define LOCALE_fr_CA_ISO8859_1 "fr_CA.ISO-8859-1@UTC"
#define LOCALE_cs_CZ_ISO8859_2 "cs_CZ.ISO-8859-2@UTC"
#define LOCALE_ru_RU_UTF_8     "ru_RU.UTF-8@UTC"
#define LOCALE_zh_CN_UTF_8     "zh_CN.UTF-8@UTC"
#else
#define LOCALE_en_US_UTF_8     "en_US.UTF-8"
#define LOCALE_fr_FR_UTF_8     "fr_FR.UTF-8"
#ifdef __linux__
#define LOCALE_fr_CA_ISO8859_1 "fr_CA.ISO-8859-1"
#define LOCALE_cs_CZ_ISO8859_2 "cs_CZ.ISO-8859-2"
#else
#define LOCALE_fr_CA_ISO8859_1 "fr_CA.ISO8859-1"
#define LOCALE_cs_CZ_ISO8859_2 "cs_CZ.ISO8859-2"
#endif
#define LOCALE_ru_RU_UTF_8     "ru_RU.UTF-8"
#define LOCALE_zh_CN_UTF_8     "zh_CN.UTF-8"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string>
#if defined(_LIBCPP_MSVCRT) || defined(__MINGW32__)
#include <io.h> // _mktemp
#else
#include <unistd.h> // close
#endif

#if defined(_NEWLIB_VERSION) && defined(__STRICT_ANSI__)
// Newlib provies this, but in the header it's under __STRICT_ANSI__
extern "C" {
  int mkstemp(char*);
}
#endif

#ifndef _LIBCPP_HAS_NO_GLOBAL_FILESYSTEM_NAMESPACE
inline
std::string
get_temp_file_name()
{
#if defined(_LIBCPP_MSVCRT) || defined(__MINGW32__)
    char Path[MAX_PATH+1];
    char FN[MAX_PATH+1];
    do { } while (0 == GetTempPath(MAX_PATH+1, Path));
    do { } while (0 == GetTempFileName(Path, "libcxx", 0, FN));
    return FN;
#else
    std::string Name;
    int FD = -1;
    do {
        Name = "libcxx.XXXXXX";
        FD = mkstemp(&Name[0]);
        if (FD == -1 && errno == EINVAL) {
            perror("mkstemp");
            abort();
        }
    } while (FD == -1);
    close(FD);
    return Name;
#endif
}
#endif // _LIBCPP_HAS_NO_GLOBAL_FILESYSTEM_NAMESPACE

#endif // PLATFORM_SUPPORT_H
