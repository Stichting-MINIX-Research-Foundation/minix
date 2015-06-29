#ifndef BIT_FIELD_AGGREGATION_H
#define BIT_FIELD_AGGREGATION_H

#include <magic/support/EDIType.h>
#include <magic/support/TypeUtil.h>

using namespace llvm;

namespace llvm {

#define BitFieldAggregationErr(M) errs() << "BitFieldAggregation: " << M << "\n"

#define BFA_NAME_PREFIX "__BFA__"

class BitFieldAggregation {
  public:
      BitFieldAggregation(TYPECONST Type* type, std::vector<EDIType> EDITypes, unsigned typeIndex, unsigned EDITypeIndex, std::vector<DIDerivedType> members, unsigned counter);
      BitFieldAggregation();
      void init(TYPECONST Type* type, std::vector<EDIType> EDITypes, unsigned typeIndex, unsigned EDITypeIndex, std::vector<DIDerivedType> members, unsigned counter);

      const std::string getDescription() const;

      unsigned getTypeIndex() const;
      unsigned getEDITypeIndex() const;
      std::string getName() const;
      std::vector<DIDerivedType> getMembers() const;

      unsigned getSize() const;
      TYPECONST Type *getType() const;
      std::vector<EDIType> getEDITypes() const;
      unsigned getRepresentativeEDITypeIndex() const;

      void print(raw_ostream &OS) const;

      static bool getBitFieldAggregations(TYPECONST Type *type, const EDIType *aEDIType, std::vector<BitFieldAggregation> &bfas, bool returnOnError=false);
      static bool hasBitFields(TYPECONST Type *type, const EDIType *aEDIType);
      static bool isBitField(TYPECONST Type *type, const EDIType *aEDIType, unsigned memberIdx);

  private:
      TYPECONST Type *type;
      std::vector<EDIType> EDITypes;
      unsigned typeIndex;
      unsigned EDITypeIndex;
      std::string name;
      std::vector<DIDerivedType> members;
      unsigned size;

      static std::string bfaNamePrefix;

      static BitFieldAggregation* getBitFieldAggregation(TYPECONST Type *type, const EDIType *aEDIType, bool returnOnError, unsigned typeIndex, unsigned EDITypeIndex, unsigned lastTypeIndex, unsigned lastEDITypeIndex, unsigned counter);
};

inline raw_ostream &operator<<(raw_ostream &OS, const BitFieldAggregation &bfa) {
    bfa.print(OS);
    return OS;
}

inline unsigned BitFieldAggregation::getTypeIndex() const {
    return typeIndex;
}

inline unsigned BitFieldAggregation::getEDITypeIndex() const {
    return EDITypeIndex;
}

inline std::string BitFieldAggregation::getName() const {
    return name;
}

inline std::vector<DIDerivedType> BitFieldAggregation::getMembers() const {
    return members;
}

inline unsigned BitFieldAggregation::getSize() const {
    return size;
}

inline TYPECONST Type *BitFieldAggregation::getType() const {
    return type;
}

inline std::vector<EDIType> BitFieldAggregation::getEDITypes() const {
    return EDITypes;
}

inline unsigned BitFieldAggregation::getRepresentativeEDITypeIndex() const {
    return EDITypeIndex;
}

inline void BitFieldAggregation::print(raw_ostream &OS) const {
     OS << getDescription();
}

inline bool BitFieldAggregation::hasBitFields(TYPECONST Type *type, const EDIType *aEDIType) {
    if(!aEDIType->isStructTy()) {
        return false;
    }
    unsigned numContainedTypes = aEDIType->getNumContainedTypes();
    for(unsigned i=0;i<numContainedTypes;i++) {
        if (isBitField(type, aEDIType, i)) {
            return true;
        }
    }
    return false;
}

inline bool BitFieldAggregation::isBitField(TYPECONST Type *type, const EDIType *aEDIType, unsigned memberIdx) {
    const DIDerivedType subDIType = aEDIType->getMember(memberIdx);
    unsigned EDITypeBits = subDIType.getSizeInBits();
    const DIType aDIType = PassUtil::getDITypeDerivedFrom(subDIType);
    unsigned EDITypeOriginalBits = aDIType.getSizeInBits();
    return (EDITypeBits>0 && EDITypeOriginalBits>0 && EDITypeBits != EDITypeOriginalBits);
}

}

#endif
