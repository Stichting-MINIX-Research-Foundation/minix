
#include <magic/support/EDIType.h>

using namespace llvm;

namespace llvm {

#define DEBUG_EDI_EQUALS        0
int debugEDIEquals = 0;

//===----------------------------------------------------------------------===//
// Constructors, destructor, and operators
//===----------------------------------------------------------------------===//

EDIType::EDIType(const MDNode *N, bool norm, bool checkOpaqueTypes) : aDIType(N) {
    init(norm, checkOpaqueTypes);
}

EDIType::EDIType(bool norm, bool checkOpaqueTypes) : aDIType() {
    init(norm, checkOpaqueTypes);
}

EDIType::EDIType(const DIType aDIType, bool norm, bool checkOpaqueTypes) : aDIType(aDIType) {
    init(norm, checkOpaqueTypes);
}

bool EDIType::operator == (const EDIType& aEDIType) const {
    const DIType oDIType = *(aEDIType.getDIType());
    return (aDIType == oDIType);
}

//===----------------------------------------------------------------------===//
// Getters
//===----------------------------------------------------------------------===//

const std::string EDIType::getDescription(int skipUnions, int skipStructs, int allowMultiNames) const {
    std::string string;
    raw_string_ostream ostream(string);
    printDescription(ostream, skipUnions, skipStructs, allowMultiNames);
    ostream.flush();
    return string;
}

const EDIType& EDIType::getContainedType(unsigned i, bool norm) const {
    static EDIType subType;
    EDIType_assert(!isBasicType() && !isVoidTy() && !isEnumTy());
    bool isArrayOrVectorTy = isArrayTy() || isVectorTy();
    if(isDerivedType() || isArrayOrVectorTy) {
        EDIType_assert(i == 0);
        if(isArrayOrVectorTy && getCurrentDimension() < getNumDimensions()-1) {
            subType = *this;
            subType.setCurrentDimension(getCurrentDimension() + 1);
        }
        else {
            subType = getTypeDerivedFrom();
        }
        return subType;
    }
    if (isFunctionTy()) {
        DITypeArray DTA = ((const DISubroutineType)aDIType).getTypeArray();
        subType = PassUtil::getDITypeFromRef(DTA.getElement(i));
    } else {
        DIArray aDIArray = getTypeArray();
        unsigned numContainedTypes = aDIArray.getNumElements();
        assert(i < numContainedTypes);
        EDIType tmpType((const DIType) aDIArray.getElement(i), norm);
        subType = tmpType;
    }
    return subType;
}

unsigned EDIType::getNumContainedTypes() const {
    if(isBasicType() || isVoidTy() || isEnumTy()) {
        return 0;
    }
    bool isArrayOrVectorTy = isArrayTy() || isVectorTy();
    if(isDerivedType() || isArrayOrVectorTy) {
        return 1;
    }
    return getTypeArrayNum();
}

const DIDerivedType& EDIType::getMember(unsigned i) const {
    static DIDerivedType aDIDerivedType;
    EDIType_assert(isUnionOrStructTy());
    DIArray aDIArray = getTypeArray();
    DIDescriptor aDIDescriptor = aDIArray.getElement(i);
    assert(aDIDescriptor.getTag() == dwarf::DW_TAG_member);
    aDIDerivedType = (DIDerivedType) aDIDescriptor;
    return aDIDerivedType;
}

bool EDIType::isUnionOrStructTy(bool isStruct, bool isUnion) const {
    if(isOpaqueTy()) {
        return false;
    }
    if((isStruct && getTag() == dwarf::DW_TAG_structure_type) ||
        (isUnion && getTag() == dwarf::DW_TAG_union_type)) {
        EDIType_assert(isCompositeType());
        return true;
    }
    return false;
}

bool EDIType::hasInnerPointers() const {
    if(isOpaqueTy() || isFunctionTy()) {
        return false;
    }
    if(isPointerTy()) {
        return true;
    }

    unsigned numContainedTypes = getNumContainedTypes();
    if(numContainedTypes == 0) {
        return false;
    }
    else if(isArrayTy() || isVectorTy()) {
        const EDIType subType = getContainedType(0);
        return subType.hasInnerPointers();
    }
    else {
        assert(isUnionOrStructTy());
        for(unsigned i=0;i<numContainedTypes;i++) {
            const EDIType subType = getContainedType(i);
            if(subType.hasInnerPointers()) {
                return true;
            }
        }
   }

   return false;
}

unsigned int EDIType::getTypeArrayNum() const {
    EDIType_assert(isCompositeType());
    /* This function is used from isOpaqueTy(), so do not use isFunctionTy() here. */
    if (getTag() == dwarf::DW_TAG_subroutine_type) {
        DITypeArray DTA = ((const DISubroutineType)aDIType).getTypeArray();
        return DTA.getNumElements();
    } else {
        return getTypeArray().getNumElements();
    }
}

DIArray EDIType::getTypeArray() const {
    static std::set<std::string> nonOpaqueEmptyTypes;
    static std::set<std::string>::iterator nonOpaqueEmptyTypesIt;
    EDIType_assert(isCompositeType());
    EDIType_assert(getTag() != dwarf::DW_TAG_subroutine_type); /* as above, no isFunctionTy() */
    DIArray aDIArray = ((const DICompositeType)aDIType).getElements();
    if(aDIArray.getNumElements() == 0 && checkOpaqueTypes && myNames.size() > 0) {
        const EDIType *aType = NULL;
        std::string name;
        for(int i=myNames.size()-1;i>=0;i--) {
            name = myNames[i];
            aType = getStructEDITypeByName(name);
            if(aType) {
                break;
            }
        }
        if(aType) {
            aDIArray = ((const DICompositeType *)aType->getDIType())->getElements();
            nonOpaqueEmptyTypesIt = nonOpaqueEmptyTypes.find(name);
            if(nonOpaqueEmptyTypesIt == nonOpaqueEmptyTypes.end()) {
                EDITypeLog("Found a non-opaque composite type with 0 members! Name is: " << name);
                nonOpaqueEmptyTypes.insert(name);
            }
        }
    }
    return aDIArray;
}

const EDIType* EDIType::getTopStructType(unsigned index) const {
    static unsigned level = 0;
    static unsigned structsLeft;
    static EDIType targetType;

    if(level == 0) {
        structsLeft = index;
    }

    if(isUnionOrStructTy() || isOpaqueTy()) {
        if(structsLeft == 0) {
            targetType = *this;
            return &targetType;
        }
        else {
            structsLeft--;
            return NULL;
        }
    }
    unsigned numContainedTypes = getNumContainedTypes();
    for(unsigned i=0;i<numContainedTypes;i++) {
        const EDIType containedType(getContainedType(i));
        level++;
        const EDIType *topStructType = containedType.getTopStructType(index);
        level--;
        if(topStructType != NULL) {
            return topStructType;
        }
    }
    return NULL;
}

//===----------------------------------------------------------------------===//
// Other public methods
//===----------------------------------------------------------------------===//

void EDIType::print(raw_ostream &OS) const {
     OS << getDescription();
}

void EDIType::printDescription(raw_ostream &OS, int skipUnions, int skipStructs, int allowMultiNames) const {
    static std::vector<const EDIType*> nestedTypes;
    int printMultiNames = allowMultiNames && myNames.size() > 1;
    if(allowMultiNames && !printMultiNames && isPointerTy() && myName.compare("")) {
        printMultiNames = 1;
    }

    if(isOpaqueTy()) {
        OS << "opaque";
        return;
    }

    unsigned numContainedTypes = getNumContainedTypes();
    if(numContainedTypes == 0) {
        OS << (printMultiNames ? getNamesString() : getName());
        return;
    }

    if(isPointerTy() && getContainedType(0).isUnionOrStructTy()) {
        bool isNestedType = false;
        unsigned j;
        for(j=0;j<nestedTypes.size();j++) {
            if(nestedTypes[j]->equals(this)) {
                isNestedType = true;
                break;
            }
        }
        if(isNestedType) {
            OS << "\\" << nestedTypes.size() - j;
            return;
        }
    }

    nestedTypes.push_back(this);
    if(isPointerTy()) {
        const EDIType subType = getContainedType(0);
        subType.printDescription(OS, skipUnions, skipStructs, allowMultiNames);
        OS << "*";
        if(printMultiNames) {
            OS << "|" << getNamesString();
        }
    }
    else if(isArrayTy() || isVectorTy()) {
        const EDIType subType = getContainedType(0);
        unsigned numElements = getNumElements();
        char startSep = isArrayTy() ? '[' : '<';
        char endSep = isArrayTy() ? ']' : '>';
        OS << startSep;
        if(numElements) {
            OS << numElements << " x ";
        }
        subType.printDescription(OS, skipUnions, skipStructs, allowMultiNames);
        OS << endSep;
    }
    else if(isUnionOrStructTy()) {
        if(skipUnions && isUnionTy()) {
            OS << "(U) $" << (printMultiNames ? getNamesString() : (myName.compare("") ? myName : "ANONYMOUS"));
            nestedTypes.pop_back();
            return;
        }
        if(skipStructs && isStructTy()) {
            OS << "$" << (printMultiNames ? getNamesString() : (myName.compare("") ? myName : "ANONYMOUS"));
            nestedTypes.pop_back();
            return;
        }
        unsigned numContainedTypes = getNumContainedTypes();
        OS << "{ ";
        if(isUnionTy()) {
            OS << "(U) ";
        }
        OS << "$" << (printMultiNames ? getNamesString() : (myName.compare("") ? myName : "ANONYMOUS")) << " ";
        for(unsigned i=0;i<numContainedTypes;i++) {
            if(i > 0) {
                OS << ", ";
            }
            EDIType subType = getContainedType(i);
            subType.printDescription(OS, skipUnions, skipStructs, allowMultiNames);
        }
        OS << " }";
   }
   else if(isFunctionTy()) {
       unsigned numContainedTypes = getNumContainedTypes();
       assert(numContainedTypes > 0);
       EDIType subType = getContainedType(0);
       subType.printDescription(OS, skipUnions, skipStructs, allowMultiNames);
       numContainedTypes--;
       OS << " (";
       for(unsigned i=0;i<numContainedTypes;i++) {
           if(i > 0) {
               OS << ", ";
           }
           subType = getContainedType(i+1);
           subType.printDescription(OS, skipUnions, skipStructs, allowMultiNames);
       }
       OS << ")";
   }
   else {
       OS << "???";
   }
   nestedTypes.pop_back();
}

bool EDIType::equals(const EDIType *other) const {
    static std::set<std::pair<MDNode*, MDNode*> > compatibleMDNodes;
    static std::set<std::pair<MDNode*, MDNode*> >::iterator compatibleMDNodesIt;
    static int max_recursive_steps = -1;
#if DEBUG_EDI_EQUALS
    if(debugEDIEquals>1) EDITypeErr("COMPARING :" << getTag() << ":" << getName() << " VS " << other->getTag() << ":" << other->getName());
#endif
    if(isOpaqueTy() || other->isOpaqueTy()) {
#if DEBUG_EDI_EQUALS
        if(debugEDIEquals) EDITypeErr("----> ???1");
#endif
        return isOpaqueTy() && other->isOpaqueTy();
    }
    if(getTag() != other->getTag()) {
#if DEBUG_EDI_EQUALS
        if(debugEDIEquals) EDITypeErr("----> false1");
#endif
        return false;
    }
    unsigned numContainedTypes = getNumContainedTypes();
    unsigned numOtherContainedTypes = other->getNumContainedTypes();
    if(numContainedTypes != numOtherContainedTypes) {
#if DEBUG_EDI_EQUALS
        if(debugEDIEquals) EDITypeErr("----> false2");
#endif
        return false;
    }
    if(getNumElements() != other->getNumElements()) {
#if DEBUG_EDI_EQUALS
        if(debugEDIEquals) EDITypeErr("----> false3");
#endif
        return false;
    }
    if(myName.compare(other->getName())) {
#if DEBUG_EDI_EQUALS
        if(debugEDIEquals) EDITypeErr("----> false4");
#endif
        return false;
    }
    if((myNames.size() > 0 || other->getNames().size() > 0) && getNamesString().compare(other->getNamesString())) {
#if DEBUG_EDI_EQUALS
        if(debugEDIEquals) EDITypeErr("----> false5");
#endif
        return false;
    }
    if(numContainedTypes == 0) {
#if DEBUG_EDI_EQUALS
        if(debugEDIEquals) EDITypeErr("----> true1");
#endif
        return true;
    }
    MDNode *aNode = *(&aDIType);
    MDNode *otherNode = *(other->getDIType());
    if(aNode == otherNode) {
#if DEBUG_EDI_EQUALS
        if(debugEDIEquals) EDITypeErr("----> true2");
#endif
        return true;
    }
    int isUnionOrStruct = isUnionOrStructTy();
    int isNonAnonUnionOrStruct = isUnionOrStruct && myName.size() > 0;
    int saved_max_recursive_steps = max_recursive_steps;
    if(max_recursive_steps == -1 && isNonAnonUnionOrStruct) {
        //A simple way to break recursion for recursive non-anonymous structs/unions.
        max_recursive_steps = 10;
    }
    else if(max_recursive_steps == 0) {
#if DEBUG_EDI_EQUALS
        if(debugEDIEquals) EDITypeErr("----> true4");
#endif
        return true;
    }
    else {
        max_recursive_steps--;
    }
    for(unsigned i=0;i<numContainedTypes;i++) {
        const EDIType &subEDIType = getContainedType(i);
        const EDIType &subOtherEDIType = other->getContainedType(i);
        if(!subEDIType.equals(&subOtherEDIType)) {
            max_recursive_steps = saved_max_recursive_steps;
            return false;
        }
    }
    max_recursive_steps = saved_max_recursive_steps;
    return true;
}

//===----------------------------------------------------------------------===//
// Public static methods
//===----------------------------------------------------------------------===//

std::string EDIType::lookupTypedefName(std::string &typedefName) {
    static std::string noName;
    for (const DIType aDITypeIt : DIFinder.types()) {
        DIType aDIType(aDITypeIt);
        if(aDIType.getTag() == dwarf::DW_TAG_typedef && aDIType.getName().compare(typedefName)) {
            while(aDIType.getTag() == dwarf::DW_TAG_typedef) {
                aDIType = PassUtil::getDITypeDerivedFrom((const DIDerivedType)aDIType);
            }
            if(aDIType.getName().compare("")) {
                return aDIType.getName();
            }
        }
    }
    return noName;
}

std::string EDIType::lookupUnionMemberName(TYPECONST Type* type) {
    std::string string;
    std::string error;
    if(!type->isStructTy() || type->getNumContainedTypes() != 1) {
        return "";
    }
    raw_string_ostream ostream(string);
    writeTypeSymbolic(ostream, type->getContainedType(0), EDIType::module);
    ostream.flush();
    Regex unionRegex("%(union|struct)\\.([^ ]+)", 0);
    assert(unionRegex.isValid(error));
    SmallVector<StringRef, 8> unionMatches;
    if(unionRegex.match(string, &unionMatches)) {
        return unionMatches[2];
    }
    return "";
}

const EDIType* EDIType::getStructEDITypeByName(std::string &typeName) {
    static EDIType aEDIType;
    assert(module);
    for (const DIType aDIType : DIFinder.types()) {
        //skip zero-element stuct types, necessary to avoid infinite recursion during opaque type lookup
        //xxx opaque type lookup should not be necessary but is there a bug in the frontend that leaves certain concrete types unnecessarily opaque?
        const EDIType tmpEDIType(aDIType, NORMALIZE, DO_NOT_CHECK_OPAQUE_TYPES);
        aEDIType = tmpEDIType;
        if(typeName.compare(aEDIType.getName())) {
            continue;
        }
        if(aEDIType.isUnionOrStructTy()) {
            return &aEDIType;
        }
    }
    return NULL;
}

void EDIType::setModule(Module *M) {
    assert(module == NULL);
    module = M;
    DIFinder.processModule(*module);
}

//===----------------------------------------------------------------------===//
// Private methods
//===----------------------------------------------------------------------===//
void EDIType::init(bool norm, bool checkOpaqueTypes) {
    EDIType_assert(isType());
    currentDimension = 0;
    this->checkOpaqueTypes = checkOpaqueTypes;
    myName = "";
    if(norm) {
        normalize();
    }
    if(myNames.size() == 0) {
        //nobody assigned names yet, do it here
        myName = isVoidTy() ? voidName : aDIType.getName();
        if(myName.compare("")) {
            myNames.push_back(myName);
        }
    }
}

void EDIType::normalize() {
    if(getTag() == dwarf::DW_TAG_typedef) {
        normalizeTypedef();
    }
    if(isBasicType() || isVoidTy() || isEnumTy() || isOpaqueTy()) {
        return;
    }
    if(isDerivedType()) {
        if(isPointerTy() || isUnionOrStructTy()) {
           return;
        }
        aDIType = PassUtil::getDITypeDerivedFrom((const DIDerivedType)aDIType);
        normalize();
        return;
    }
    EDIType_assert(isCompositeType());
    if(isAggregateType() || isVectorTy() || isFunctionTy()) {
       return;
    }
    EDIType_assert(getNumContainedTypes() == 1);
    aDIType = *(getContainedType(0, DO_NOT_NORMALIZE).getDIType());
    normalize();
}

void EDIType::normalizeTypedef() {
    myNames.clear();
    while(aDIType.getTag() == dwarf::DW_TAG_typedef) {
        if(aDIType.getName().compare("")) {
            myNames.push_back(aDIType.getName());
        }
        aDIType = PassUtil::getDITypeDerivedFrom((const DIDerivedType)aDIType);
    }
    myName = isVoidTy() ? voidName : aDIType.getName();
    if(!myName.compare("")) {
        //anonymous typedefed type, use the deepest typedef name
        assert(!isBasicType());
        assert(myNames.size() > 0);
        myName = myNames[myNames.size()-1];
    }
    else {
        myNames.push_back(myName);
    }
}

StringRef EDIType::voidName("void");
Module *EDIType::module = NULL;
DebugInfoFinder EDIType::DIFinder;

}
