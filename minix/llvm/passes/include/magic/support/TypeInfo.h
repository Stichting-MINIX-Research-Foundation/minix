#ifndef TYPE_INFO_H
#define TYPE_INFO_H

#include <magic/magic.h>
#include <magic_common.h>
#include <magic/support/SmartType.h>
#include <magic/support/TypeUtil.h>

using namespace llvm;

#define TypeInfoErr(M) errs() << "TypeInfo: " << M << "\n"

#define FUNCTIONS_USE_HASH_TYPE_STRINGS 1
#define ROOT_TYPES_HAVE_TYPE_STRINGS    0
#define DEBUG_CAST_LOOKUPS              0

#define TYPEINFO_PERSISTENT             0x01

namespace llvm {

class TypeInfo {
  public:
      TypeInfo(const SmartType *aSmartType, int persistent=0);
      TypeInfo(TYPECONST FunctionType *FT, int persistent=0);
      TypeInfo(TYPECONST PointerType *PT, int persistent=0);
      TypeInfo(TYPECONST ArrayType *AT, int persistent=0);
      TypeInfo(TYPECONST IntegerType *IT, int persistent=0);
      TypeInfo(TYPECONST StructType *OpaqueST, int persistent=0);
      void init(int persistent);

      const SmartType *getSmartType() const;
      TYPECONST Type *getType() const;
      unsigned getNumContainedTypes() const;
      unsigned getNumChildTypes() const;
      TypeInfo *getContainedType(unsigned i) const;
      std::vector<GlobalValue*> getParents() const;
      std::string getTypeString() const;
      std::string getDescription() const;
      std::string getVerboseDescription() const;
      std::string getName() const;
      std::vector<std::string> getNames() const;
      std::string getNamesString() const;
      std::vector<std::string> getMemberNames() const;
      std::vector<int> getValueSet() const;
      std::vector<TypeInfo*> getCastTypes() const;
      unsigned getTypeID() const;
      unsigned getFlags() const;
      unsigned getBitWidth() const;
      bool equals(TYPECONST TypeInfo *other) const;
      bool hasRawTypeRepresentation() const;

      std::string formatMemberName(const std::string &memberName, unsigned &numAnonMembers);
      void setValueSet(const std::vector<int> &valueSet);
      void setContainedTypes(const std::vector<TypeInfo*> &containedTypes);
      void addParent(GlobalValue* parent);
      void addParents(const std::vector<GlobalValue*> &parents);
      bool removeParent(GlobalValue* parent);
      bool removeAllParents();
      void setPersistent();
      bool splitByParentValueSet(std::vector<TypeInfo*> &splitTypeInfos, std::set<GlobalVariable*> &globalVariablesWithAddressTaken);

      static unsigned getMaxNameLength();
      static unsigned getMaxTypeStringLength();
      static void setIntCastTypes(std::map<TYPECONST Type*, std::set<int> > &intCastTypes);
      static void setBitCastTypes(std::map<TYPECONST Type*, std::set<TYPECONST Type*> > &bitCastTypes);

  private:
      const SmartType *aSmartType;
      TYPECONST Type *aType;
      bool forceTypeDescription;
      mutable std::string typeDescription;
      std::string name;
      std::vector<std::string> names;
      std::vector<std::string> memberNames;
      std::vector<int> valueSet;
      std::vector<TypeInfo*> containedTypes;
      std::vector<GlobalValue*> parents;
      unsigned bitWidth;
      unsigned typeID;
      unsigned numElements;
      unsigned flags;
      static unsigned maxNameLength;
      static unsigned maxTypeStringLength;
      static std::map<TYPECONST Type*, std::set<int> > intCastTypes;
      static std::map<TYPECONST Type*, std::set<TYPECONST Type*> > bitCastTypes;
      static std::map<TYPECONST Type*, std::set<TypeInfo*> > typeMap;

      TypeInfo() {}
};

inline TypeInfo::TypeInfo(const SmartType *aSmartType, int persistent) {
    unsigned i;
    assert(aSmartType);
    this->aSmartType = aSmartType;
    this->aType = aSmartType->getType();
    bool rawTypeRepresentation = aSmartType->hasRawTypeRepresentation();
    forceTypeDescription = aSmartType->isFunctionTy();
    name = aSmartType->getEDIType()->getName();
    std::vector<StringRef> nameRefs = aSmartType->getEDIType()->getNames();
    for(i=0;i<nameRefs.size();i++) {
        if(nameRefs[i].size() > maxNameLength) {
            maxNameLength = nameRefs[i].size();
        }
        names.push_back(nameRefs[i]);
    }
    if(aSmartType->isStructTy()) {
        if(!rawTypeRepresentation) {
            const EDIType* aEDIType = aSmartType->getEDIType();
            unsigned numContainedTypes = aSmartType->getNumContainedTypes();
            unsigned numAnonMembers = 0;
            if(aEDIType->isUnionTy()) {
                assert(numContainedTypes == 1);
                i=aSmartType->getUnionMemberIdx();
                const DIDerivedType subDIType = aSmartType->getMember(i);
                memberNames.push_back(formatMemberName(subDIType.getName(), numAnonMembers));
            }
            else {
                assert(aEDIType->isStructTy());
                i=0;
                for(;i<numContainedTypes;i++) {
                    const DIDerivedType subDIType = aSmartType->getMember(i);
                    memberNames.push_back(formatMemberName(subDIType.getName(), numAnonMembers));
                }
            }
        }
        else {
            memberNames.push_back("raw");
        }
    }
    if(aSmartType->getEDIType()->isEnumTy()) {
        std::vector<unsigned> enumValues = aSmartType->getEDIType()->getEnumValues();
        valueSet.push_back(enumValues.size()); //push length as the first value
        for(unsigned i=0;i<enumValues.size();i++) {
            valueSet.push_back((int)enumValues[i]);
        }
    }
    bitWidth = TypeUtil::typeToBits(aSmartType->getType());
    const EDIType *aEDIType = aSmartType->getEDIType();
    typeID = 0;
    flags = 0;
    if(aSmartType->isOpaqueTy()) {
        typeID = MAGIC_TYPE_OPAQUE;
    }
    else if(aEDIType->isVoidTy()) {
        typeID = MAGIC_TYPE_VOID;
    }
    else if(aSmartType->isFunctionTy()) {
        typeID = MAGIC_TYPE_FUNCTION;
    }
    else if(aSmartType->isStructTy()) {
        if(aEDIType->isUnionTy()) {
            typeID = MAGIC_TYPE_UNION;
        }
        else if(aEDIType->isStructTy()) {
            typeID = MAGIC_TYPE_STRUCT;
#if MAGIC_VARSIZED_STRUCTS_SUPPORT
            if(!rawTypeRepresentation) {
                assert(this->aType->getNumContainedTypes() > 0);
                TYPECONST Type *lastSubType = this->aType->getContainedType(this->aType->getNumContainedTypes()-1);
                if(lastSubType->isArrayTy() && ((ArrayType*)lastSubType)->getNumElements() <= 1) {
                    flags |= MAGIC_TYPE_VARSIZE;
                }
            }
#endif
        }
    }
    else if(aSmartType->isPointerTy()) {
        typeID = MAGIC_TYPE_POINTER;
    }
    else if(aSmartType->isArrayTy()) {
        typeID = MAGIC_TYPE_ARRAY;
    }
    else if(aEDIType->isFloatingPointTy()) {
        typeID = MAGIC_TYPE_FLOAT;
    }
    else if(aEDIType->isIntegerTy()) {
        typeID = MAGIC_TYPE_INTEGER;
    }
    else if(aEDIType->isEnumTy()) {
        typeID = MAGIC_TYPE_ENUM;
    }
    else if(aEDIType->isVectorTy()) {
        typeID = MAGIC_TYPE_VECTOR;
    }
    assert(typeID);
    if(typeID == MAGIC_TYPE_INTEGER || typeID == MAGIC_TYPE_ENUM) {
        if(getNamesString().find("unsigned") != std::string::npos) {
            flags |= MAGIC_TYPE_UNSIGNED;
        }
    }
    numElements = aSmartType->getNumElements();
    init(persistent);
}

inline TypeInfo::TypeInfo(TYPECONST FunctionType *FT, int persistent) {
    assert(FT);
    aSmartType = NULL;
    aType = FT;
    forceTypeDescription = true;
    bitWidth = 0;
    typeID = MAGIC_TYPE_FUNCTION;
    numElements = 0;
    flags = MAGIC_TYPE_EXTERNAL;
    init(persistent);
}

inline TypeInfo::TypeInfo(TYPECONST PointerType *PT, int persistent) {
    assert(PT);
    aSmartType = NULL;
    aType = PT;
    forceTypeDescription = false;
    bitWidth = 0;
    typeID = MAGIC_TYPE_POINTER;
    numElements = 0;
    flags = MAGIC_TYPE_EXTERNAL;
    init(persistent);
}

inline TypeInfo::TypeInfo(TYPECONST ArrayType *AT, int persistent) {
    assert(AT);
    aSmartType = NULL;
    aType = AT;
    forceTypeDescription = false;
    bitWidth = 0;
    typeID = MAGIC_TYPE_ARRAY;
    numElements = AT->getNumElements();
    flags = MAGIC_TYPE_EXTERNAL;
    init(persistent);
}

inline TypeInfo::TypeInfo(TYPECONST IntegerType *IT, int persistent) {
    assert(IT);
    aSmartType = NULL;
    aType = IT;
    forceTypeDescription = true;
    bitWidth = IT->getBitWidth();
    typeID = MAGIC_TYPE_INTEGER;
    numElements = 0;
    flags = MAGIC_TYPE_EXTERNAL;
    init(persistent);
}

inline TypeInfo::TypeInfo(TYPECONST StructType *OpaqueST, int persistent) {
    assert(OpaqueST);
    assert(TypeUtil::isOpaqueTy(OpaqueST));
    aSmartType = NULL;
    aType = OpaqueST;
    forceTypeDescription = true;
    bitWidth = 0;
    typeID = MAGIC_TYPE_OPAQUE;
    numElements = 0;
    flags = MAGIC_TYPE_EXTERNAL;
    init(persistent);
}

inline void TypeInfo::init(int persistent) {
    std::map<TYPECONST Type*, std::set<int> >::iterator intCastTypesIt;
    //set persistent if necessary
    if(persistent) {
        setPersistent();
    }
    //initialize value set for pointers casted to int
    if(aType->isPointerTy()) {
        intCastTypesIt = intCastTypes.find(aType);
        if(intCastTypesIt != intCastTypes.end()) {
            std::set<int> &pointerValues = intCastTypesIt->second;
            assert(pointerValues.size() > 0);
            flags |= MAGIC_TYPE_INT_CAST;
            if(pointerValues.size() > 1 || *(pointerValues.begin()) != 0) {
                valueSet.push_back(pointerValues.size()); //push length as the first value
                for(std::set<int>::iterator it=pointerValues.begin() ; it != pointerValues.end(); it++) {
                    assert(*it != 0);
                    valueSet.push_back(*it);
                }
            }
        }
    }
    //adjust flags
    bool hasInnerPointers = aSmartType ? aSmartType->hasInnerPointers() : TypeUtil::hasInnerPointers(aType);
    if(!hasInnerPointers) {
        flags |= MAGIC_TYPE_NO_INNER_PTRS;
    }
}

inline const SmartType *TypeInfo::getSmartType() const {
    return aSmartType;
}

inline TYPECONST Type *TypeInfo::getType() const {
    return aType;
}

inline unsigned TypeInfo::getNumContainedTypes() const {
    return containedTypes.size();
}

inline unsigned TypeInfo::getNumChildTypes() const {
    return typeID == MAGIC_TYPE_ARRAY ? numElements : containedTypes.size();
}

inline TypeInfo *TypeInfo::getContainedType(unsigned i) const {
    assert(i<containedTypes.size());
    return containedTypes[i];
}

inline std::vector<GlobalValue*> TypeInfo::getParents() const {
    return parents;
}

inline std::string TypeInfo::getTypeString() const {
    std::string typeString = forceTypeDescription || aType->getNumContainedTypes() == 0 ? getDescription() : "";
    if(MAGIC_SHRINK_TYPE_STR && typeString.size() > MAGIC_MAX_TYPE_STR_LEN) {
        typeString = typeString.substr(0, MAGIC_MAX_TYPE_STR_LEN-3) + "...";
    }
    if(typeString.size() > maxTypeStringLength) {
        maxTypeStringLength = typeString.size();
    }
    return typeString;
}

inline std::string TypeInfo::getDescription() const {
    if(typeDescription.size() == 0) {
        if(aType->isFunctionTy() && FUNCTIONS_USE_HASH_TYPE_STRINGS) {
            unsigned hash = TypeUtil::getHash(aType);
            raw_string_ostream ostream(typeDescription);
            ostream << "hash_" << hash;
            ostream.flush();
        }
        else {
            typeDescription = TypeUtil::getDescription(aType);
        }
    }
    return typeDescription;
}

inline std::string TypeInfo::getVerboseDescription() const {
    return aSmartType ? aSmartType->getDescription() : getDescription();
}

inline std::string TypeInfo::getName() const {
    return name;
}

inline std::vector<std::string> TypeInfo::getNames() const {
    return names;
}

inline std::string TypeInfo::getNamesString() const {
    std::string string;
    raw_string_ostream ostream(string);
    for(unsigned i=0;i<names.size();i++) {
        if(i>0) ostream << "|";
        ostream << names[i];
    }
    ostream.flush();
    return string;
}

inline std::vector<std::string> TypeInfo::getMemberNames() const {
    for(unsigned i=0;i<memberNames.size();i++) {
        if(memberNames[i].size() > maxNameLength) {
            maxNameLength = memberNames[i].size();
        }
    }
    return memberNames;
}

inline std::vector<int> TypeInfo::getValueSet() const {
    return valueSet;
}

inline std::vector<TypeInfo*> TypeInfo::getCastTypes() const {
    std::vector<TypeInfo*> castTypes;
    std::map<TYPECONST Type*, std::set<TYPECONST Type*> >::iterator bitCastTypesIt;
    std::map<TYPECONST Type*, std::set<TypeInfo*> >::iterator typeMapIt;
    if(!aType->isPointerTy()) {
        return castTypes;
    }
    //XXX to-do: match only original struct name during lookup by looking at the original bitcast instruction
    //the following lookups do not distinguish between struct x = {18} and struct y = {i8}
    //the number of false positives generated seems to be fairly small, anyway
    bitCastTypesIt = bitCastTypes.find(aType);
    if(bitCastTypesIt == bitCastTypes.end()) {
        return castTypes;
    }
    std::set<TYPECONST Type*> bitCastSet = bitCastTypesIt->second;
#if MAGIC_INDEX_TRANSITIVE_BIT_CASTS
    std::vector<TYPECONST Type*> bitCasts;
    for(std::set<TYPECONST Type*>::iterator it=bitCastSet.begin();it!=bitCastSet.end();it++) {
        bitCasts.push_back(*it);
    }
    while(!bitCasts.empty()) {
        TYPECONST Type* bcType = bitCasts.front();
        bitCasts.erase(bitCasts.begin());
        bitCastTypesIt = bitCastTypes.find(bcType);
        if(bitCastTypesIt != bitCastTypes.end()) {
            std::set<TYPECONST Type*> set = bitCastTypesIt->second;
            for(std::set<TYPECONST Type*>::iterator it=set.begin();it!=set.end();it++) {
                unsigned bitCastSetSize = bitCastSet.size();
                TYPECONST Type *newBcType = *it;
                if(newBcType == aType) {
                    continue;
                }
                bitCastSet.insert(newBcType);
                if(bitCastSet.size() != bitCastSetSize) {
                    bitCasts.push_back(newBcType);
                }
            }
        }
    }
#endif

#if DEBUG_CAST_LOOKUPS
    if(aType->getContainedType(0)->isStructTy()) {
        TypeInfoErr("--- type is struct* " << getContainedType(0)->getName());
    }
    else if(aType->getContainedType(0)->isPointerTy() && aType->getContainedType(0)->getContainedType(0)->isStructTy()) {
        TypeInfoErr("--- type is struct** " << getContainedType(0)->getContainedType(0)->getName());
    }
    else {
        TypeInfoErr("--- type is " << getDescription());
    }
#endif

    for(std::set<TYPECONST Type*>::iterator it=bitCastSet.begin();it!=bitCastSet.end();it++) {
        TYPECONST Type* type = *it;
        assert(type->isPointerTy());
        typeMapIt = typeMap.find(type->getContainedType(0));
        if(typeMapIt == typeMap.end()) {

#if DEBUG_CAST_LOOKUPS
            TypeInfoErr("*** cast target type not found: " << TypeUtil::getDescription(type->getContainedType(0)));
#endif

            continue;
        }
        std::set<TypeInfo*> *typeInfoSet = &(typeMapIt->second);
        for(std::set<TypeInfo*>::iterator it2=typeInfoSet->begin();it2!=typeInfoSet->end();it2++) {
            TypeInfo* typeInfo = *it2;
            assert(typeInfo->getType() != getType()->getContainedType(0));

#if DEBUG_CAST_LOOKUPS
            if(typeInfo->getType()->isStructTy()) {
                TypeInfoErr(">>> cast target type info is struct " << typeInfo->getName());
            }
            else if(typeInfo->getType()->isPointerTy() && typeInfo->getType()->getContainedType(0)->isStructTy()) {
                TypeInfoErr(">>> cast target type info is struct* " << typeInfo->getContainedType(0)->getName());
            }
            else {
                TypeInfoErr(">>> cast target type info is " << typeInfo->getDescription());
            }
#endif
            castTypes.push_back(typeInfo);

#if MAGIC_COMPACT_COMP_TYPES
            /* This is safe as long as we check for compatible (LLVM) types at runtime. */
            break;
#endif
        }
    }
    if(castTypes.size() > 0) { //push delimiter
        castTypes.push_back(NULL);
    }
    return castTypes;
}

inline unsigned TypeInfo::getTypeID() const {
    return typeID;
}

inline unsigned TypeInfo::getFlags() const {
    return flags;
}

inline unsigned TypeInfo::getBitWidth() const {
    return bitWidth;
}

inline bool TypeInfo::equals(TYPECONST TypeInfo *other) const {
    if(aSmartType && other->getSmartType()) {
        return aSmartType->equals(other->getSmartType());
    }
    return (flags & (~MAGIC_TYPE_IS_ROOT)) == (other->getFlags() & (~MAGIC_TYPE_IS_ROOT))
        && !getDescription().compare(other->getDescription());
}

inline bool TypeInfo::hasRawTypeRepresentation() const {
    return aSmartType && aSmartType->hasRawTypeRepresentation();
}

inline std::string TypeInfo::formatMemberName(const std::string &memberName, unsigned &numAnonMembers) {
    if (memberName.compare("")) {
        return memberName;
    }
    std::string name(memberName);
    raw_string_ostream ostream(name);
    ostream << MAGIC_ANON_MEMBER_PREFIX << "." << (numAnonMembers+1);
    ostream.flush();
    numAnonMembers++;

    return name;
}

inline void TypeInfo::setValueSet(const std::vector<int> &valueSet) {
    this->valueSet = valueSet;
}

inline void TypeInfo::setContainedTypes(const std::vector<TypeInfo*> &containedTypes) {
    this->containedTypes = containedTypes;
}

inline void TypeInfo::addParent(GlobalValue* parent) {
    assert((typeID == MAGIC_TYPE_FUNCTION && dyn_cast<Function>(parent))
        || (typeID != MAGIC_TYPE_FUNCTION && dyn_cast<GlobalVariable>(parent)));
    this->parents.push_back(parent);
    flags |= MAGIC_TYPE_IS_ROOT;

#if ROOT_TYPES_HAVE_TYPE_STRINGS
    forceTypeDescription = true;
#endif
}

inline void TypeInfo::addParents(const std::vector<GlobalValue*> &parents) {
    for(unsigned i=0;i<parents.size();i++) {
        addParent(parents[i]);
    }
}

inline bool TypeInfo::removeParent(GlobalValue* parent)
{
    std::vector<GlobalValue*> originalParents = this->parents;
    this->parents.clear();
    for(unsigned i=0;i<originalParents.size();i++) {
        if(originalParents[i] != parent) {
            this->parents.push_back(originalParents[i]);
        }
    }
    int sizeDiff = originalParents.size() - this->parents.size();
    assert(sizeDiff == 0 || sizeDiff == 1);
    return (sizeDiff == 1);
}

inline bool TypeInfo::removeAllParents()
{
    if(this->parents.size() > 0) {
        this->parents.clear();
        return true;
    }
    return false;
}

inline void TypeInfo::setPersistent() {
    std::map<TYPECONST Type*, std::set<TypeInfo*> >::iterator typeMapIt;
    typeMapIt = typeMap.find(aType);
    if(typeMapIt == typeMap.end()) {
        std::set<TypeInfo*> set;
        set.insert(this);
        typeMap.insert(std::pair<TYPECONST Type*, std::set<TypeInfo*> >(aType, set));
    }
    else {
        std::set<TypeInfo*> *set;
        set = &(typeMapIt->second);
        set->insert(this);
    }
}

inline bool TypeInfo::splitByParentValueSet(std::vector<TypeInfo*> &splitTypeInfos, std::set<GlobalVariable*> &globalVariablesWithAddressTaken) {
    std::map<std::vector<int>, std::vector<GlobalVariable*> > valueSetMap;
    std::map<std::vector<int>, std::vector<GlobalVariable*> >::iterator valueSetMapIt;
    std::vector<int> valueSet;
    splitTypeInfos.push_back(this);
    if(!isa<IntegerType>(aType)) {
        return false;
    }
    assert(valueSet.size() == 0);
    for(unsigned i=0;i<parents.size();i++) {
        if(GlobalVariable *GV = dyn_cast<GlobalVariable>(parents[i])) {
            bool hasAddressTaken = globalVariablesWithAddressTaken.find(GV) != globalVariablesWithAddressTaken.end();
            if(hasAddressTaken) {
                continue;
            }
            valueSet.clear();
            bool valueSetFound = MagicUtil::lookupValueSet(GV, valueSet);
            if(!valueSetFound) {
                continue;
            }
            valueSetMapIt = valueSetMap.find(valueSet);
            if(valueSetMapIt == valueSetMap.end()) {
                std::vector<GlobalVariable*> vector;
                valueSetMap.insert(std::pair<std::vector<int>, std::vector<GlobalVariable*> >(valueSet, vector));
                valueSetMapIt = valueSetMap.find(valueSet);
                assert(valueSetMapIt != valueSetMap.end());
            }
            std::vector<GlobalVariable*> *globalsVector = &valueSetMapIt->second;
            globalsVector->push_back(GV);
        }
    }
    if(valueSetMap.size() == 0) {
        return false;
    }
    for(valueSetMapIt = valueSetMap.begin(); valueSetMapIt!=valueSetMap.end(); valueSetMapIt++) {
        const std::vector<int> &values = valueSetMapIt->first;
        const std::vector<GlobalVariable*> &globalVariables = valueSetMapIt->second;
        TypeInfo *aTypeInfo = new TypeInfo(*this);
        aTypeInfo->setValueSet(values);
        aTypeInfo->removeAllParents();
        for(unsigned i=0;i<globalVariables.size();i++) {
            GlobalVariable* GV = globalVariables[i];
            bool parentRemoved = this->removeParent(GV);
            assert(parentRemoved);
            aTypeInfo->addParent(GV);
        }
        splitTypeInfos.push_back(aTypeInfo);
    }
    return true;
}

inline unsigned TypeInfo::getMaxNameLength() {
    return TypeInfo::maxNameLength;
}

inline unsigned TypeInfo::getMaxTypeStringLength() {
    return TypeInfo::maxTypeStringLength;
}

inline void TypeInfo::setIntCastTypes(std::map<TYPECONST Type*, std::set<int> > &intCastTypes) {
    TypeInfo::intCastTypes = intCastTypes;
}

inline void TypeInfo::setBitCastTypes(std::map<TYPECONST Type*, std::set<TYPECONST Type*> > &bitCastTypes) {
    TypeInfo::bitCastTypes = bitCastTypes;
}

}

#endif
