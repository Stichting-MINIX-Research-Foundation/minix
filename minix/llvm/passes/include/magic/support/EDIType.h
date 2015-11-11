#ifndef EDITYPE_H
#define EDITYPE_H

#include <pass.h>

using namespace llvm;

namespace llvm {

#define EDITypeLog(M) DEBUG(dbgs() << "EDIType: " << M << "\n")
#define EDITypeErr(M) errs() << "EDIType: " << M << "\n"

#define EDIType_assert(X) do {            \
        if(!(X)) {                        \
            errs() << "Assertion failed, dumping object...\n"; \
            errs() << *this;              \
        }                                 \
        assert(X);                        \
    } while(0)

class EDIType {
  public:
      EDIType(const MDNode *N, bool norm=true, bool checkOpaqueTypes=true);
      EDIType(bool norm=false, bool checkOpaqueTypes=false);
      EDIType(const DIType aDIType, bool norm=true, bool checkOpaqueTypes=true);

      bool operator == (const EDIType& aEDIType) const;

      static const bool NORMALIZE = true;
      static const bool DO_NOT_NORMALIZE = false;
      static const bool CHECK_OPAQUE_TYPES = true;
      static const bool DO_NOT_CHECK_OPAQUE_TYPES = false;

      const std::string getDescription(int skipUnions=0, int skipStructs=0, int allowMultiNames=0) const;
      const EDIType& getContainedType(unsigned i, bool norm=true) const;
      unsigned getNumContainedTypes() const;
      const DIDerivedType& getMember(unsigned i) const;
      bool isUnionOrStructTy(bool isStruct=true, bool isUnion=true) const;
      bool hasInnerPointers() const;
      unsigned int getTypeArrayNum() const;
      DIArray getTypeArray() const;
      const EDIType* getTopStructType(unsigned index) const;

      unsigned getNumElements() const;
      unsigned getNumDimensions() const;
      void setCurrentDimension(unsigned dimension);
      unsigned getCurrentDimension() const;
      const DIType *getDIType() const;
      StringRef getName() const;
      std::vector<StringRef> getNames() const;
      StringRef getNamesString() const;
      std::vector<unsigned> getEnumValues() const;
      unsigned getTag()const;
      EDIType getTypeDerivedFrom() const;
      bool isType() const;
      bool isBasicType() const;
      bool isDerivedType() const;
      bool isCompositeType() const;
      bool isPrimitiveType() const;
      bool isAggregateType() const;
      bool isVoidTy() const;
      bool isComplexFloatingPointTy() const;
      bool isFloatingPointTy() const;
      bool isCharTy() const;
      bool isIntTy() const;
      bool isBoolTy() const;
      bool isIntegerTy() const;
      bool isFunctionTy() const;
      bool isArrayTy() const;
      bool isEnumTy() const;
      bool isVectorTy() const;
      bool isUnionTy() const;
      bool isStructTy() const;
      bool isPointerTy() const;
      bool isOpaqueTy() const;

      void print(raw_ostream &OS) const;
      void printDescription(raw_ostream &OS, int skipUnions=0, int skipStructs=0, int allowMultiNames=0) const;
      bool equals(const EDIType *other) const;

      static std::string lookupTypedefName(std::string &name);
      static std::string lookupUnionMemberName(TYPECONST Type* type);
      static const EDIType* getStructEDITypeByName(std::string &name);
      static void setModule(Module *M);
      static void writeTypeSymbolic(raw_string_ostream &OS, TYPECONST Type *type, const Module *M);

  private:
      DIType aDIType;
      unsigned currentDimension;
      bool checkOpaqueTypes;
      StringRef myName;
      std::vector<StringRef> myNames;
      static StringRef voidName;
      static Module *module;
      static DebugInfoFinder DIFinder;

      void init(bool norm, bool checkOpaqueTypes);
      void normalize();
      void normalizeTypedef();
};

inline raw_ostream &operator<<(raw_ostream &OS, const EDIType &aEDIType) {
    aEDIType.print(OS);
    return OS;
}

inline unsigned EDIType::getNumElements() const {
    if(!isArrayTy() && !isVectorTy()) {
        return 1;
    }
    const DIArray aDIArray = getTypeArray();
    const DIDescriptor aDIDescriptor = aDIArray.getElement(currentDimension);
    assert(aDIDescriptor.getTag() == dwarf::DW_TAG_subrange_type);

    return PassUtil::getDbgSubrangeNumElements((DISubrange)aDIDescriptor);
}

inline unsigned EDIType::getNumDimensions() const {
    return isArrayTy() || isVectorTy() ? getTypeArray().getNumElements() : 1;
}

inline void EDIType::setCurrentDimension(unsigned dimension) {
    assert(dimension < getNumDimensions());
    this->currentDimension = dimension;
}

inline unsigned EDIType::getCurrentDimension() const {
    return currentDimension;
}

inline const DIType *EDIType::getDIType() const {
    return &aDIType;
}

inline StringRef EDIType::getName() const {
    return myName;
}

inline std::vector<StringRef> EDIType::getNames() const {
    return myNames;
}

inline StringRef EDIType::getNamesString() const {
    std::string string;
    raw_string_ostream ostream(string);
    for(unsigned i=0;i<myNames.size();i++) {
        if(i>0) ostream << "|";
        ostream << myNames[i];
    }
    ostream.flush();
    return string;
}

inline std::vector<unsigned> EDIType::getEnumValues() const {
    assert(isEnumTy());
    std::vector<unsigned> enumValues;
    DIArray aDIArray = getTypeArray();
    unsigned numValues = aDIArray.getNumElements();
    for(unsigned i=0;i<numValues;i++) {
        DIDescriptor aDIDescriptor = aDIArray.getElement(i);
        assert(aDIDescriptor.getTag() == dwarf::DW_TAG_enumerator);
        const unsigned value = (unsigned) ((DIEnumerator)aDIDescriptor).getEnumValue();
        enumValues.push_back(value);
    }
    return enumValues;
}

inline unsigned EDIType::getTag()const {
    return aDIType.getTag();
}

inline EDIType EDIType::getTypeDerivedFrom() const {
    EDIType_assert(isDerivedType() || isCompositeType());
    EDIType subType(PassUtil::getDITypeDerivedFrom((const DIDerivedType)aDIType));
    return subType;
}

inline bool EDIType::isType() const {
    return aDIType.isType() || isVoidTy();
}

inline bool EDIType::isBasicType() const {
    return aDIType.isBasicType();
}

inline bool EDIType::isDerivedType() const {
    return aDIType.isDerivedType() && !aDIType.isCompositeType();
}

inline bool EDIType::isCompositeType() const {
    return aDIType.isCompositeType();
}

inline bool EDIType::isPrimitiveType() const {
    return (isVoidTy() || isFloatingPointTy());
}

inline bool EDIType::isAggregateType() const {
    return isUnionOrStructTy() || isArrayTy() || isVectorTy();
}

inline bool EDIType::isVoidTy() const {
    return !aDIType.isValid(); //xxx we should keep track of this to spot all the i8* = void*
}

inline bool EDIType::isComplexFloatingPointTy() const {
    if(!isBasicType()) return false;
    return (((DIBasicType)aDIType).getEncoding() == dwarf::DW_ATE_complex_float);
}

inline bool EDIType::isFloatingPointTy() const {
    if(!isBasicType()) return false;
    return EDIType::isComplexFloatingPointTy() || (((DIBasicType)aDIType).getEncoding() == dwarf::DW_ATE_float);
}

inline bool EDIType::isCharTy() const {
    if(!isBasicType()) return false;
    return (((DIBasicType)aDIType).getEncoding() == dwarf::DW_ATE_signed_char ||
        ((DIBasicType)aDIType).getEncoding() == dwarf::DW_ATE_unsigned_char);
}

inline bool EDIType::isIntTy() const {
    if(!isBasicType()) return false;
    return (((DIBasicType)aDIType).getEncoding() == dwarf::DW_ATE_signed ||
        ((DIBasicType)aDIType).getEncoding() == dwarf::DW_ATE_unsigned);
}

inline bool EDIType::isBoolTy() const {
    if(!isBasicType()) return false;
    return (((DIBasicType)aDIType).getEncoding() == dwarf::DW_ATE_boolean);
}

inline bool EDIType::isIntegerTy() const {
    return (isCharTy() || isIntTy() || isBoolTy());
}

inline bool EDIType::isFunctionTy() const {
    return (getTag() == dwarf::DW_TAG_subroutine_type && !isOpaqueTy());
}

inline bool EDIType::isArrayTy() const {
    return (getTag() == dwarf::DW_TAG_array_type && !isOpaqueTy());
}

inline bool EDIType::isEnumTy() const {
    return (getTag() == dwarf::DW_TAG_enumeration_type && !isOpaqueTy());
}

inline bool EDIType::isVectorTy() const {
    return (PassUtil::isDbgVectorTy(aDIType) && !isOpaqueTy());
}

inline bool EDIType::isUnionTy() const {
    return isUnionOrStructTy(false, true);
}

inline bool EDIType::isStructTy() const {
    return isUnionOrStructTy(true, false);
}

inline bool EDIType::isPointerTy() const {
    return (getTag() == dwarf::DW_TAG_pointer_type);
}

inline bool EDIType::isOpaqueTy() const {
    return (isCompositeType() && getTypeArrayNum() == 0);
}

inline void EDIType::writeTypeSymbolic(raw_string_ostream &OS, TYPECONST Type *type, const Module *M) {
    return PassUtil::writeTypeSymbolic(OS, type, M);
}

}

#endif
