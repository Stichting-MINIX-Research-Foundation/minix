#ifndef TYPE_UTIL_H
#define TYPE_UTIL_H

#include <pass.h>
#include <magic/support/EDIType.h>

using namespace llvm;

namespace llvm {

#define TypeUtilLog(M) DEBUG(dbgs() << "TypeUtil: " << M << "\n")

class TypeUtil {
  public:
      static int VERBOSE_LEVEL;
      static int PRINT_SKIP_UNIONS;
      static int PRINT_SKIP_STRUCTS;
      static int PRINT_USE_BUILTIN_PRINTING;
      static int PRINT_MULTI_NAMES;
      static const unsigned TYPE_STRUCT = 0x01;
      static const unsigned TYPE_UNION = 0x02;
      static const unsigned TYPE_ANONYMOUS = 0x04;
      static const unsigned TYPE_UNNAMED = 0x08;

      static bool isPaddedType(TYPECONST Type *type);

      static TYPECONST Type* lookupTopStructType(TYPECONST Type *type, unsigned index);
      static void parseTopStructTypes(Module &M, TYPECONST Type *type, std::vector<std::string> *names, std::vector<unsigned> *flags);
      static int findTopStructTypeIndex(Module &M, TYPECONST Type *type, std::string &name, unsigned flagsToAccept);
      static TYPECONST Type* getRecursiveElementType(TYPECONST Type *type);
      static TYPECONST Type* getArrayFreePointerType(TYPECONST Type *type);
      static bool hasInnerPointers(TYPECONST Type *type);
      static bool isArrayAsStructTy(TYPECONST Type *type);
      static bool isOpaqueTy(TYPECONST Type *type);

      static unsigned getHash(TYPECONST Type* type);
      static const std::string getDescription(TYPECONST Type* type, size_t max_chars = 0, size_t max_levels = 0);
      static const std::string getDescription(const EDIType* aEDIType);
      static const std::string getDescription(TYPECONST Type* type, const EDIType* aEDIType, size_t max_chars = 0, size_t max_levels = 0);
      static const std::string getFormattedDescription(std::string &description);
      static void printFormattedTypeString(raw_ostream &OS, std::string &typeStr, int start, int length);
      static void printTypeString(raw_ostream &OS, TYPECONST Type* type, size_t max_chars = 0, size_t max_levels = 0);
      static unsigned typeToBits(TYPECONST Type *type);
};

inline bool TypeUtil::isOpaqueTy(TYPECONST Type *type) {
      return PassUtil::isOpaqueTy(type);
}

}

#endif
