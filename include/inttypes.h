/*	inttypes.h - Format conversions of integer types.
 *							Author: Kees J. Bot
 *								4 Oct 2003
 * Assumptions and bugs the same as for <stdint.h>
 * Bug: Wide character integer conversion functions missing.
 */

#ifndef _INTTYPES_H
#define _INTTYPES_H

#ifndef _STDINT_H
#include <stdint.h>
#endif

#if !__cplusplus || defined(__STDC_FORMAT_MACROS)

/* Macros to print integers defined in <stdint.h>.  The first group should
 * not be used in code, they're merely here to build the second group.
 * (The standard really went overboard here, only the first group is needed.)
 */
#define PRI8		""
#define PRILEAST8	""
#define PRIFAST8	""
#define PRI16		""
#define PRILEAST16	""
#define PRIFAST16	""
#if _WORD_SIZE == 2
#define PRI32		"l"
#define PRILEAST32	"l"
#define PRIFAST32	"l"
#else
#define PRI32		""
#define PRILEAST32	""
#define PRIFAST32	""
#endif
#if _WORD_SIZE > 2 && __L64
#define PRI64		"l"
#define PRILEAST64	"l"
#define PRIFAST64	"l"
#endif

/* Macros for fprintf, the ones defined by the standard. */
#define PRId8		PRI8"d"
#define PRIdLEAST8	PRILEAST8"d"
#define PRIdFAST8	PRIFAST8"d"
#define PRId16		PRI16"d"
#define PRIdLEAST16	PRILEAST16"d"
#define PRIdFAST16	PRIFAST16"d"
#define PRId32		PRI32"d"
#define PRIdLEAST32	PRILEAST32"d"
#define PRIdFAST32	PRIFAST32"d"
#if _WORD_SIZE > 2 && __L64
#define PRId64		PRI64"d"
#define PRIdLEAST64	PRILEAST64"d"
#define PRIdFAST64	PRIFAST64"d"
#endif

#define PRIi8		PRI8"i"
#define PRIiLEAST8	PRILEAST8"i"
#define PRIiFAST8	PRIFAST8"i"
#define PRIi16		PRI16"i"
#define PRIiLEAST16	PRILEAST16"i"
#define PRIiFAST16	PRIFAST16"i"
#define PRIi32		PRI32"i"
#define PRIiLEAST32	PRILEAST32"i"
#define PRIiFAST32	PRIFAST32"i"
#if _WORD_SIZE > 2 && __L64
#define PRIi64		PRI64"i"
#define PRIiLEAST64	PRILEAST64"i"
#define PRIiFAST64	PRIFAST64"i"
#endif

#define PRIo8		PRI8"o"
#define PRIoLEAST8	PRILEAST8"o"
#define PRIoFAST8	PRIFAST8"o"
#define PRIo16		PRI16"o"
#define PRIoLEAST16	PRILEAST16"o"
#define PRIoFAST16	PRIFAST16"o"
#define PRIo32		PRI32"o"
#define PRIoLEAST32	PRILEAST32"o"
#define PRIoFAST32	PRIFAST32"o"
#if _WORD_SIZE > 2 && __L64
#define PRIo64		PRI64"o"
#define PRIoLEAST64	PRILEAST64"o"
#define PRIoFAST64	PRIFAST64"o"
#endif

#define PRIu8		PRI8"u"
#define PRIuLEAST8	PRILEAST8"u"
#define PRIuFAST8	PRIFAST8"u"
#define PRIu16		PRI16"u"
#define PRIuLEAST16	PRILEAST16"u"
#define PRIuFAST16	PRIFAST16"u"
#define PRIu32		PRI32"u"
#define PRIuLEAST32	PRILEAST32"u"
#define PRIuFAST32	PRIFAST32"u"
#if _WORD_SIZE > 2 && __L64
#define PRIu64		PRI64"u"
#define PRIuLEAST64	PRILEAST64"u"
#define PRIuFAST64	PRIFAST64"u"
#endif

#define PRIx8		PRI8"x"
#define PRIxLEAST8	PRILEAST8"x"
#define PRIxFAST8	PRIFAST8"x"
#define PRIx16		PRI16"x"
#define PRIxLEAST16	PRILEAST16"x"
#define PRIxFAST16	PRIFAST16"x"
#define PRIx32		PRI32"x"
#define PRIxLEAST32	PRILEAST32"x"
#define PRIxFAST32	PRIFAST32"x"
#if _WORD_SIZE > 2 && __L64
#define PRIx64		PRI64"x"
#define PRIxLEAST64	PRILEAST64"x"
#define PRIxFAST64	PRIFAST64"x"
#endif

#define PRIX8		PRI8"X"
#define PRIXLEAST8	PRILEAST8"X"
#define PRIXFAST8	PRIFAST8"X"
#define PRIX16		PRI16"X"
#define PRIXLEAST16	PRILEAST16"X"
#define PRIXFAST16	PRIFAST16"X"
#define PRIX32		PRI32"X"
#define PRIXLEAST32	PRILEAST32"X"
#define PRIXFAST32	PRIFAST32"X"
#if _WORD_SIZE > 2 && __L64
#define PRIX64		PRI64"X"
#define PRIXLEAST64	PRILEAST64"X"
#define PRIXFAST64	PRIFAST64"X"
#endif

/* Macros to scan integers with fscanf(), nonstandard first group. */
#define SCN8		"hh"
#define SCNLEAST8	"hh"
#define SCNFAST8	""
#define SCN16		"h"
#define SCNLEAST16	"h"
#define SCNFAST16	""
#if _WORD_SIZE == 2
#define SCN32		"l"
#define SCNLEAST32	"l"
#define SCNFAST32	"l"
#else
#define SCN32		""
#define SCNLEAST32	""
#define SCNFAST32	""
#endif
#if _WORD_SIZE > 2 && __L64
#define SCN64		"l"
#define SCNLEAST64	"l"
#define SCNFAST64	"l"
#endif

/* Macros for fscanf, the ones defined by the standard. */
#define SCNd8		SCN8"d"
#define SCNdLEAST8	SCNLEAST8"d"
#define SCNdFAST8	SCNFAST8"d"
#define SCNd16		SCN16"d"
#define SCNdLEAST16	SCNLEAST16"d"
#define SCNdFAST16	SCNFAST16"d"
#define SCNd32		SCN32"d"
#define SCNdLEAST32	SCNLEAST32"d"
#define SCNdFAST32	SCNFAST32"d"
#if _WORD_SIZE > 2 && __L64
#define SCNd64		SCN64"d"
#define SCNdLEAST64	SCNLEAST64"d"
#define SCNdFAST64	SCNFAST64"d"
#endif

#define SCNi8		SCN8"i"
#define SCNiLEAST8	SCNLEAST8"i"
#define SCNiFAST8	SCNFAST8"i"
#define SCNi16		SCN16"i"
#define SCNiLEAST16	SCNLEAST16"i"
#define SCNiFAST16	SCNFAST16"i"
#define SCNi32		SCN32"i"
#define SCNiLEAST32	SCNLEAST32"i"
#define SCNiFAST32	SCNFAST32"i"
#if _WORD_SIZE > 2 && __L64
#define SCNi64		SCN64"i"
#define SCNiLEAST64	SCNLEAST64"i"
#define SCNiFAST64	SCNFAST64"i"
#endif

#define SCNo8		SCN8"o"
#define SCNoLEAST8	SCNLEAST8"o"
#define SCNoFAST8	SCNFAST8"o"
#define SCNo16		SCN16"o"
#define SCNoLEAST16	SCNLEAST16"o"
#define SCNoFAST16	SCNFAST16"o"
#define SCNo32		SCN32"o"
#define SCNoLEAST32	SCNLEAST32"o"
#define SCNoFAST32	SCNFAST32"o"
#if _WORD_SIZE > 2 && __L64
#define SCNo64		SCN64"o"
#define SCNoLEAST64	SCNLEAST64"o"
#define SCNoFAST64	SCNFAST64"o"
#endif

#define SCNu8		SCN8"u"
#define SCNuLEAST8	SCNLEAST8"u"
#define SCNuFAST8	SCNFAST8"u"
#define SCNu16		SCN16"u"
#define SCNuLEAST16	SCNLEAST16"u"
#define SCNuFAST16	SCNFAST16"u"
#define SCNu32		SCN32"u"
#define SCNuLEAST32	SCNLEAST32"u"
#define SCNuFAST32	SCNFAST32"u"
#if _WORD_SIZE > 2 && __L64
#define SCNu64		SCN64"u"
#define SCNuLEAST64	SCNLEAST64"u"
#define SCNuFAST64	SCNFAST64"u"
#endif

#define SCNx8		SCN8"x"
#define SCNxLEAST8	SCNLEAST8"x"
#define SCNxFAST8	SCNFAST8"x"
#define SCNx16		SCN16"x"
#define SCNxLEAST16	SCNLEAST16"x"
#define SCNxFAST16	SCNFAST16"x"
#define SCNx32		SCN32"x"
#define SCNxLEAST32	SCNLEAST32"x"
#define SCNxFAST32	SCNFAST32"x"
#if _WORD_SIZE > 2 && __L64
#define SCNx64		SCN64"x"
#define SCNxLEAST64	SCNLEAST64"x"
#define SCNxFAST64	SCNFAST64"x"
#endif
#endif /* !__cplusplus || __STDC_FORMAT_MACROS */

/* Integer conversion functions for [u]intmax_t. */
#define stroimax(nptr, endptr, base)	strtol(nptr, endptr, base)
#define stroumax(nptr, endptr, base)	strtoul(nptr, endptr, base)

#endif /* _INTTYPES_H */
