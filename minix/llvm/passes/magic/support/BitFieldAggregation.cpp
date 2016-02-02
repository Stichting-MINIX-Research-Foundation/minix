#include <magic/support/BitFieldAggregation.h>

using namespace llvm;

namespace llvm {

#define DEBUG_BFA           0

#define BitFieldAggregation_assert_or_return(R,RV,T,ET,X) do {              \
        if(!(X)) {                        \
            if(R) return RV;              \
            errs() << "Assertion failed, dumping types...\n"; \
            errs() << TypeUtil::getDescription(T, ET); \
        }                                 \
        assert(X);                        \
    } while(0)

//===----------------------------------------------------------------------===//
// Constructors, destructor, and operators
//===----------------------------------------------------------------------===//

BitFieldAggregation::BitFieldAggregation(TYPECONST Type* type, std::vector<EDIType> EDITypes, unsigned typeIndex, unsigned EDITypeIndex, std::vector<DIDerivedType> members, unsigned counter) {
    init(type, EDITypes, typeIndex, EDITypeIndex, members, counter);
}

BitFieldAggregation::BitFieldAggregation() {
    type = NULL;
    typeIndex = 0;
    EDITypeIndex = 0;
    name = "";
}

void BitFieldAggregation::init(TYPECONST Type* type, std::vector<EDIType> EDITypes, unsigned typeIndex, unsigned EDITypeIndex, std::vector<DIDerivedType> members, unsigned counter) {
    assert(members.size() == EDITypes.size());
    assert(typeIndex <= EDITypeIndex);
    size = members.size();
    assert(size > 0);

    this->type = type;
    this->EDITypes = EDITypes;
    this->typeIndex = typeIndex;
    this->EDITypeIndex = EDITypeIndex;
    this->members = members;
    raw_string_ostream ostream(name);
    ostream << bfaNamePrefix << counter;
    ostream.flush();
}

//===----------------------------------------------------------------------===//
// Getters
//===----------------------------------------------------------------------===//

const std::string BitFieldAggregation::getDescription() const {
    std::string string;
    raw_string_ostream ostream(string);
    ostream << "[\nname = " << name << "\nmembers = ";
    for(unsigned i=0;i<size;i++) {
        ostream << (i==0 ? "" : ", ") << members[i].getName();
    }
    ostream << "\ntype = \n" << TypeUtil::getDescription(getType());
    std::vector<EDIType> EDITypes = getEDITypes();
    for(unsigned i=0;i<EDITypes.size();i++) {
        ostream << "\nEDIType" << i << " =\n" << TypeUtil::getDescription(&EDITypes[i]);
    }
    ostream << "\n]";
    ostream.flush();
    return string;
}

//===----------------------------------------------------------------------===//
// Public static methods
//===----------------------------------------------------------------------===//

bool BitFieldAggregation::getBitFieldAggregations(TYPECONST Type *type, const EDIType *aEDIType, std::vector<BitFieldAggregation> &bfas, bool returnOnError) {
    std::vector<BitFieldAggregation> emptyBfas;
    if(!hasBitFields(type, aEDIType)) {
        return true;
    }
    unsigned typeIndex = 0;
    unsigned EDITypeIndex = 0;
    unsigned typeContainedTypes = type->getNumContainedTypes();
    unsigned aEDITypeContainedTypes = aEDIType->getNumContainedTypes();
    unsigned counter = 1;
    const EDIType privateEDIType(*aEDIType);
    aEDIType = &privateEDIType;
    while(typeIndex < typeContainedTypes) {
        TYPECONST Type *containedType = type->getContainedType(typeIndex);
        if(EDITypeIndex >= aEDITypeContainedTypes && typeIndex == typeContainedTypes-1 && TypeUtil::isPaddedType(type)) {
            break;
        }
        BitFieldAggregation_assert_or_return(returnOnError, false, type, aEDIType, EDITypeIndex < aEDITypeContainedTypes);
        const EDIType containedEDIType = aEDIType->getContainedType(EDITypeIndex);
        unsigned typeBits = TypeUtil::typeToBits(containedType);
        if(typeBits > 0 && containedEDIType.isIntegerTy()) {
            unsigned EDITypeBits = aEDIType->getMember(EDITypeIndex).getSizeInBits();
            assert(typeBits >= EDITypeBits);
            if(typeBits > EDITypeBits) {
                unsigned lastTypeIndex = typeIndex;
                unsigned lastEDITypeIndex = EDITypeIndex;
                while(lastEDITypeIndex+1 < aEDITypeContainedTypes && isBitField(type, aEDIType, lastEDITypeIndex+1)) { // grab all the bitfields following the first one found
                    lastEDITypeIndex++;
                    EDITypeBits += aEDIType->getMember(lastEDITypeIndex).getSizeInBits();
                }
                while(lastTypeIndex+1 < typeContainedTypes && EDITypeBits > typeBits) { // grab all the necessary fields to cover all the bits found in the bitfields
                    lastTypeIndex++;
                    typeBits += TypeUtil::typeToBits(type->getContainedType(lastTypeIndex));
                }
                BitFieldAggregation *bfa = BitFieldAggregation::getBitFieldAggregation(type, aEDIType, returnOnError, typeIndex, EDITypeIndex, lastTypeIndex, lastEDITypeIndex, counter++);
                BitFieldAggregation_assert_or_return(returnOnError, false, type, aEDIType, bfa != NULL);
                if(bfa->getSize() > 1) {
                    //we don't care about single-element aggregates
                    bfas.push_back(*bfa);
                }
                typeIndex++;
                EDITypeIndex += bfa->getSize();
                continue;
            }
        }
        typeIndex++;
        EDITypeIndex++;
    }
    return true;
}

BitFieldAggregation *BitFieldAggregation::getBitFieldAggregation(TYPECONST Type *type, const EDIType *aEDIType, bool returnOnError, unsigned typeIndex, unsigned EDITypeIndex, unsigned lastTypeIndex, unsigned lastEDITypeIndex, unsigned counter) {
    static BitFieldAggregation bfa;
    TYPECONST Type *containedType = type->getContainedType(typeIndex);
    unsigned typeBits = TypeUtil::typeToBits(containedType);
    assert(typeBits > 0);
    unsigned nextTypeBits = 0;
    if (typeIndex < lastTypeIndex) {
        nextTypeBits = TypeUtil::typeToBits(type->getContainedType(typeIndex+1));
    }
    const int maxNumMembers = (lastEDITypeIndex - EDITypeIndex) - (lastTypeIndex - typeIndex) + 1;
    unsigned index = EDITypeIndex;
    std::vector<DIDerivedType> members;
    std::vector<EDIType> containedEDITypes;

    BitFieldAggregation_assert_or_return(returnOnError, NULL, type, aEDIType, maxNumMembers > 0);
#if DEBUG_BFA
    BitFieldAggregationErr("getBitFieldAggregation(): typeIndex = " << typeIndex << ", EDITypeIndex = " << EDITypeIndex << ", maxNumMembers = " << maxNumMembers);
    BitFieldAggregationErr("getBitFieldAggregation(): lastTypeIndex = " << lastTypeIndex << ", lastEDITypeIndex = " << lastEDITypeIndex);
    BitFieldAggregationErr("getBitFieldAggregation(): " << TypeUtil::getDescription(type) << " VS " << TypeUtil::getDescription(aEDIType));
#endif
    while(index <= lastEDITypeIndex && members.size() < (unsigned)maxNumMembers) {
        const EDIType containedEDIType = aEDIType->getContainedType(index);
#if DEBUG_BFA
        BitFieldAggregationErr("Examining type " << TypeUtil::getDescription(&containedEDIType));
#endif
        BitFieldAggregation_assert_or_return(returnOnError, NULL, type, aEDIType, containedEDIType.isIntegerTy());
        DIDerivedType member = aEDIType->getMember(index);
        unsigned EDITypeBits = member.getSizeInBits();
#if DEBUG_BFA
        BitFieldAggregationErr("Type bits = " << typeBits << ", next type bits = " << nextTypeBits << ", index = " << index);
        BitFieldAggregationErr("This is member " << member.getName() << " with bits " << EDITypeBits);
#endif
        if((index > EDITypeIndex && EDITypeBits == nextTypeBits) || EDITypeBits > typeBits) {
            break;
        }
        typeBits -= EDITypeBits;
        members.push_back(member);
        containedEDITypes.push_back(containedEDIType);
        index++;
    }
    bfa.init(containedType, containedEDITypes, typeIndex, EDITypeIndex, members, counter);
    return &bfa;
}

std::string BitFieldAggregation::bfaNamePrefix = BFA_NAME_PREFIX;

}
