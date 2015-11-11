#include <magic_common.h>
#include <magic/support/SmartType.h>
#include <limits.h>

using namespace llvm;

namespace llvm {

#define DEBUG_EQUALS        0
#define DEBUG_BFAS          0
#define DEBUG_UNIONS        0
int debugEquals = 0;

//===----------------------------------------------------------------------===//
// Constructors, destructor, and operators
//===----------------------------------------------------------------------===//
SmartType::SmartType(const SmartType& et) {
    cloneFrom(et);
}

SmartType::SmartType(TYPECONST Type *type, const DIType *aDIType, bool useExceptions, bool forceRawTypeRepresentation) {
    EDIType tmp(*aDIType);
    init(type, &tmp, useExceptions, forceRawTypeRepresentation);
    normalize();
}

SmartType::SmartType(TYPECONST Type *type, const EDIType *aEDIType, bool useExceptions, bool forceRawTypeRepresentation) {
    init(type, aEDIType, useExceptions, forceRawTypeRepresentation);
    normalize();
}

SmartType::~SmartType() {
    if(hasExplicitContainedEDITypes) {
        for(unsigned i=0;i<explicitContainedEDITypes.size();i++) {
            const EDIType* aEDIType = explicitContainedEDITypes[i];
            delete aEDIType;
        }
    }
}

SmartType& SmartType::operator=(const SmartType& et) {
    if (this != &et) {
        cloneFrom(et);
    }
    return *this;
}

void SmartType::cloneFrom(const SmartType& et) {
    init(et.type, &et.aEDIType, et.useExceptions, false);
    if(et.hasExplicitContainedEDITypes) {
        hasExplicitContainedEDITypes = true;
        for(unsigned i=0;i<et.explicitContainedEDITypes.size();i++) {
            const EDIType* aEDIType = et.explicitContainedEDITypes[i];
            explicitContainedEDITypes.push_back(new EDIType(*aEDIType));
        }
    }
    isInconsistent = et.isInconsistent;
    rawTypeRepresentation = et.rawTypeRepresentation;
    bfas = et.bfas;
}

//===----------------------------------------------------------------------===//
// Getters
//===----------------------------------------------------------------------===//

const std::string SmartType::getDescription() const {
    return TypeUtil::getDescription(type, &aEDIType);
}

const SmartType* SmartType::getContainedType(unsigned i) const {
    assert(!rawTypeRepresentation);
    bool hasBitFields = false;
    TYPECONST Type *subType = type->getContainedType(i);
    const SmartType* retSmartType = NULL;
    if(hasExplicitContainedEDITypes) {
        const EDIType subEDIType(*(explicitContainedEDITypes[i]));
        retSmartType = new SmartType(subType, &subEDIType, useExceptions);
    }
    else {
        if(aEDIType.isUnionTy()) {
            assert(i == 0);
            i = unionMemberIdx;
        }
        else if(bfas.size() > 0) {
            i = getBFAFreeIdx(i, bfas);
            hasBitFields = true;
        }
        const EDIType subEDIType(aEDIType.getContainedType(i));
        retSmartType = new SmartType(subType, &subEDIType, useExceptions, hasBitFields && subEDIType.isIntegerTy() && subType->isArrayTy());
    }
    return retSmartType;
}

unsigned SmartType::getNumContainedTypes() const {
    if(rawTypeRepresentation || aEDIType.isComplexFloatingPointTy() || isOpaqueTy()) {
        return 0;
    }
    unsigned numContainedTypes = type->getNumContainedTypes();
    unsigned numContainedEDITypes;
    if(hasExplicitContainedEDITypes) {
        numContainedEDITypes = explicitContainedEDITypes.size();
    }
    else {
        if(aEDIType.isUnionTy()) {
            numContainedEDITypes = 1;
        }
        else {
            numContainedEDITypes = aEDIType.getNumContainedTypes();
            if(bfas.size() > 0) {
                for(unsigned i=0;i<bfas.size();i++) {
                    numContainedEDITypes -= (bfas[i].getSize() - 1);
                }
            }
        }
    }
    if(numContainedTypes == numContainedEDITypes+1 && isPaddedTy()) {
        numContainedTypes--;
    }
    if(numContainedTypes != numContainedEDITypes) {
        if(isArrayTy() && TypeUtil::isArrayAsStructTy(type)) {
            numContainedTypes = 1;
        }
    }
    SmartType_assert(numContainedTypes == numContainedEDITypes);
    return numContainedTypes;
}

const DIDerivedType& SmartType::getMember(unsigned i) const {
    assert(!rawTypeRepresentation);
    if(bfas.size() > 0) {
        i = getBFAFreeIdx(i, bfas);
    }
    return aEDIType.getMember(i);
}

unsigned SmartType::getUnionMemberIdx() const {
    assert(!rawTypeRepresentation);
    SmartType_assert(isTy(type->isStructTy(), aEDIType.isUnionTy(), "getUnionMemberIdx"));
    SmartType_assert(getNumContainedTypes() == 1);
    TYPECONST Type* uMemberType = type->getContainedType(0);
    unsigned numSubEDITypes = aEDIType.getNumContainedTypes();
    std::vector<unsigned> indexes;
    int maxWeakConsistencyLevel = -1;
    unsigned maxWeakConsistencyIndex = -1;
    int maxWeakConsistencyLevelEntries;
    unsigned index;
    for(unsigned i=0;i<numSubEDITypes;i++) {
        int weakConsistencyLevel;
        EDIType subEDIType = aEDIType.getContainedType(i);
        if(isTypeConsistent(uMemberType, &subEDIType, true, &weakConsistencyLevel)) {
            indexes.push_back(i);
            if(weakConsistencyLevel > maxWeakConsistencyLevel) {
                maxWeakConsistencyLevel = weakConsistencyLevel;
                maxWeakConsistencyIndex = i;
                maxWeakConsistencyLevelEntries = 1;
            }
            else if(weakConsistencyLevel == maxWeakConsistencyLevel) {
                maxWeakConsistencyLevelEntries++;
            }
        }
    }
    if(indexes.size() == 0) {
        //try to match names if we failed before
        std::string name = EDIType::lookupUnionMemberName(type);
        if(name.compare("")) {
            for(unsigned i=0;i<numSubEDITypes;i++) {
                EDIType subEDIType = aEDIType.getContainedType(i);
                if(!subEDIType.getName().compare(name)) {
                    indexes.push_back(i);
                    maxWeakConsistencyIndex = i;
                }
            }
        }
    }
    if(indexes.size() == 0) {
        //No valid union member found
#if DEBUG_UNIONS
        SmartTypeErr("getUnionMemberIdx: resorting to a raw type. No valid union member found for: " << getDescription());
#endif
        return UINT_MAX;
    }
    index = maxWeakConsistencyIndex;
    if(indexes.size() > 1) {
        SmartTypeLog("getUnionMemberIdx: warning: multiple valid union members found, automatically selecting the first most-consistent member:");
        SmartTypeLog(" - target member type is: " << TypeUtil::getDescription(uMemberType));
        SmartTypeLog(" - selected index is: " << index);
        for(unsigned i=0;i<indexes.size();i++) {
            EDIType subEDIType = aEDIType.getContainedType(indexes[i]);
            SmartTypeLog(" - " << indexes[i] << ". " << TypeUtil::getDescription(&subEDIType));
        }
    }

    return index;
}

const SmartType* SmartType::getTopStructType(unsigned index) const {
    TYPECONST Type *topType = TypeUtil::lookupTopStructType(type, index);
    const EDIType *topEDIType = aEDIType.getTopStructType(index);
    assert((topType && topEDIType) || (!topType && !topEDIType));
    if(topType) {
        const SmartType* retSmartType = new SmartType(topType, topEDIType);
        return retSmartType;
    }
    return NULL;
}

//===----------------------------------------------------------------------===//
// Other public methods
//===----------------------------------------------------------------------===//

void SmartType::verify() const {
    SmartType_assert(isTypeConsistent());
}

void SmartType::print(raw_ostream &OS) const {
     OS << getDescription();
}

bool SmartType::equals(const SmartType* other, bool isDebug) const {
    static std::set<std::pair<MDNode*, MDNode*> > compatibleMDNodes;
    static std::set<std::pair<MDNode*, MDNode*> >::iterator compatibleMDNodesIt;
#if DEBUG_EQUALS
    if(debugEquals) SmartTypeErr("COMPARING :" << getDescription() << " VS " << other->getDescription());
#endif
    if(type != other->getType()) {
#if DEBUG_EQUALS
        if(debugEquals) SmartTypeErr("----> false1");
#endif
        return false;
    }
    if(isOpaqueTy() || other->isOpaqueTy()) {
        return isOpaqueTy() && other->isOpaqueTy();
    }
    if(aEDIType.getTag() != other->getEDIType()->getTag()) {
#if DEBUG_EQUALS
        if(debugEquals) SmartTypeErr("----> false1b");
#endif
        return false;
    }
    if(isFunctionTy() && (!isTypeConsistent() || !other->isTypeConsistent())) {
        //we just compare the types for inconsistent functions types
        return true;
    }
    if(hasRawTypeRepresentation() || other->hasRawTypeRepresentation()) {
        return !aEDIType.getNamesString().compare(other->getEDIType()->getNamesString());
    }
    unsigned numContainedTypes = getNumContainedTypes();
    unsigned otherNumContainedTypes = other->getNumContainedTypes();
    if(numContainedTypes != otherNumContainedTypes) {
#if DEBUG_EQUALS
        if(debugEquals) SmartTypeErr("----> false2");
#endif
        return false;
    }
    unsigned numElements = aEDIType.getNumElements();
    unsigned otherNumElements = other->getEDIType()->getNumElements();
    if(numElements != otherNumElements) {
#if DEBUG_EQUALS
        if(debugEquals) SmartTypeErr("----> false2b");
#endif
        return false;
    }
    std::string name = aEDIType.getName();
    std::string otherName = other->getEDIType()->getName();
    if(name.compare(otherName)) {
#if DEBUG_EQUALS
        if(debugEquals) SmartTypeErr("----> false3: " <<  name <<  " vs " <<  otherName);
#endif
        return false;
    }
    if(aEDIType.getNames().size() > 1 || other->getEDIType()->getNames().size() > 1) {
        std::string namesString = aEDIType.getNamesString();
        std::string otherNamesString = other->getEDIType()->getNamesString();
        if(namesString.compare(otherNamesString)) {
#if DEBUG_EQUALS
            if(debugEquals) SmartTypeErr("----> false4: " <<  namesString <<  " vs " <<  otherNamesString);
#endif
            return false;
        }
    }
    if(numContainedTypes == 0) {
#if DEBUG_EQUALS
        if(debugEquals) SmartTypeErr("----> true4");
#endif
        return true;
    }
    MDNode *node = *(aEDIType.getDIType());
    MDNode *otherNode = *(other->getEDIType()->getDIType());
    if(node == otherNode) {
        return true;
    }
    for(unsigned i=0;i<SmartType::equalsNestedTypes.size();i++) {
        if(type == SmartType::equalsNestedTypes[i]) {
#if DEBUG_EQUALS
            if(debugEquals) SmartTypeErr("----> true5");
#endif
            return true;
        }
    }
    //before digging the type tree, see if we have these 2 metadata nodes in cache
    //this gives us an impressive speedup
    MDNode *minNode = node < otherNode ? node : otherNode;
    MDNode *maxNode = node < otherNode ? otherNode : node;
    compatibleMDNodesIt = compatibleMDNodes.find(std::pair<MDNode*, MDNode*>(minNode, maxNode));
    if(compatibleMDNodesIt != compatibleMDNodes.end()) {
        return true;
    }
    SmartType::equalsNestedTypes.push_back(type);
    const SmartType* containedType = NULL;
    const SmartType* otherContainedType = NULL;
    bool sameContainedTypes = true;
    for(unsigned i=0;i<numContainedTypes;i++) {
        containedType = getContainedType(i);
        otherContainedType = other->getContainedType(i);
        sameContainedTypes = containedType->equals(otherContainedType);
        delete containedType;
        delete otherContainedType;
        if(!sameContainedTypes) {
            break;
        }
    }
    SmartType::equalsNestedTypes.pop_back();
    if(!sameContainedTypes) {
#if DEBUG_EQUALS
        if(debugEquals) SmartTypeErr("----> false6");
#endif
        return false;
    }
#if DEBUG_EQUALS
    if(debugEquals) SmartTypeErr("----> true7");
#endif
    compatibleMDNodes.insert(std::pair<MDNode*, MDNode*>(minNode, maxNode));
    return true;
}

//===----------------------------------------------------------------------===//
// Public static methods
//===----------------------------------------------------------------------===//

const SmartType* SmartType::getSmartTypeFromGV(Module &M, GlobalVariable *GV, DIGlobalVariable *DIG) {
    //ignore anonymous strings
    if(GV->getName().startswith(".str")) {
        return NULL;
    }
    MDNode *DIGV = PassUtil::findDbgGlobalDeclare(GV);
    if (!DIGV) {
        return NULL;
    }
    DIGlobalVariable Var(DIGV);
    DIType aDIType = PassUtil::getDITypeFromRef(Var.getType());
    const SmartType* retSmartType = new SmartType(GV->getType()->getElementType(), &aDIType);
    if(DIG) {
        *DIG = Var;
    }
    return retSmartType;
}

const SmartType* SmartType::getSmartTypeFromLV(Module &M, AllocaInst *AI, DIVariable *DIV) {
    const DbgDeclareInst *DDI = FindAllocaDbgDeclare(AI);
    if (!DDI) {
        return NULL;
    }
    if(DDI == (const DbgDeclareInst *) -1) {
        return (const SmartType*)-1;
    }
    DIVariable Var(cast<MDNode>(DDI->getVariable()));
    DIType aDIType = PassUtil::getDITypeFromRef(Var.getType());
    if(DIV) {
        *DIV = Var;
    }
    const SmartType* aSmartType = new SmartType(AI->getAllocatedType(), &aDIType);
    return aSmartType;
}

const SmartType* SmartType::getSmartTypeFromFunction(Module &M, Function *F, DISubprogram *DIS) {
    MDNode *DIF = PassUtil::findDbgSubprogramDeclare(F);
    if (!DIF) {
        return NULL;
    }
    DISubprogram Sub(DIF);
    DIType aDIType = Sub.getType();
    const SmartType* retSmartType = new SmartType(F->getType()->getElementType(), &aDIType);
    if(DIS) {
        *DIS = Sub;
    }
    return retSmartType;
}

const SmartType* SmartType::getStructSmartTypeByName(Module &M, GlobalVariable* GV, std::string &name, bool isUnion) {
    std::string structName((isUnion ? "union." : "struct.") + name);

    TYPECONST Type *targetType = M.getTypeByName(structName);
    const EDIType *targetEDIType = EDIType::getStructEDITypeByName(name);

    const SmartType *retSmartType = NULL;
    if(targetType && targetEDIType) {
        retSmartType = new SmartType(targetType, targetEDIType);
    }
    return retSmartType;
}

std::vector<const SmartType*>* SmartType::getTopStructSmartTypes(Module &M, GlobalVariable* GV) {
    std::vector<std::string> names;
    std::vector<unsigned> flags;
    TypeUtil::parseTopStructTypes(M, GV->getType()->getElementType(), &names, &flags);
    std::vector<const SmartType*> *vector = new std::vector<const SmartType*>;
    for(unsigned i=0;i<names.size();i++) {
        std::string entryName = names[i];
        unsigned entryFlags = flags[i];
        const SmartType *aSmartType = NULL;
        if(!(entryFlags & TypeUtil::TYPE_ANONYMOUS) && !(entryFlags & TypeUtil::TYPE_UNNAMED)) {
            aSmartType = getStructSmartTypeByName(M, GV, entryName, (entryFlags & TypeUtil::TYPE_UNION));
        }
        if(aSmartType == NULL) {
            //this method can fail due to name clashing but is the only one possible for anonymous or unnamed struct types
            const SmartType *GVSmartType = getSmartTypeFromGV(M, GV);
            assert(GVSmartType && "Unable to find a match for anonymous or unnamed struct type");
            aSmartType = GVSmartType->getTopStructType(i);
            delete GVSmartType;
            assert(aSmartType != NULL);
        }
        vector->push_back(aSmartType);
    }

    return vector;
}

//===----------------------------------------------------------------------===//
// Private methods
//===----------------------------------------------------------------------===//

void SmartType::init(TYPECONST Type *type, const EDIType *aEDIType, bool useExceptions, bool forceRawTypeRepresentation) {
    this->type = type;
    this->aEDIType = *aEDIType;
    this->useExceptions = useExceptions;
    hasExplicitContainedEDITypes = false;
    isInconsistent = false;
    rawTypeRepresentation = false;
    unionMemberIdx = 0;
    if(aEDIType->isUnionTy()) {
        if(forceRawUnions) {
            rawTypeRepresentation = true;
        }
        else {
            unionMemberIdx = getUnionMemberIdx();
            if(unionMemberIdx == UINT_MAX) {
                rawTypeRepresentation = true;
                unionMemberIdx = 0;
            }
        }
    }
    else if(forceRawTypeRepresentation || (forceRawBitfields && BitFieldAggregation::hasBitFields(type, &(this->aEDIType)))) {
        rawTypeRepresentation = true;
    }
}

void SmartType::normalize() {
    if(isFunctionTy() && !hasExplicitContainedEDITypes) {
        flattenFunctionTy();
        hasExplicitContainedEDITypes = true;
    }
    if(!hasExplicitContainedEDITypes && !rawTypeRepresentation) {
        if(!BitFieldAggregation::getBitFieldAggregations(type, &aEDIType, bfas, true)) {
            //failed to determine bfas
#if DEBUG_BFAS
            SmartTypeErr("normalize: resorting to a raw type. Cannot determine bfas for: " << getDescription());
#endif
            rawTypeRepresentation = true;
        }
    }
}

void SmartType::flattenFunctionTy() {
    SmartType_assert(isFunctionTy() && !hasExplicitContainedEDITypes);
    SmartType_assert(explicitContainedEDITypes.size() == 0);
#if MAGIC_FLATTEN_FUNCTION_ARGS
    int ret = flattenFunctionArgs(type, &aEDIType, 0);
    if(ret < 0 || explicitContainedEDITypes.size() != type->getNumContainedTypes()) {
        SmartTypeLog("Warning: function flattening produced an inconsistent type!");
        isInconsistent = true;
    }
#else
    isInconsistent = true;
#endif
}

int SmartType::flattenFunctionArgs(TYPECONST Type *type, const EDIType *aEDIType, unsigned nextContainedType) {
    unsigned containedTypes = type->getNumContainedTypes();
    unsigned containedEDITypes = aEDIType->isUnionTy() ? 1 : aEDIType->getNumContainedTypes();
    unsigned containedEDIOptions = aEDIType->isUnionTy() ? aEDIType->getNumContainedTypes() : 1;
    int ret;

    unsigned nextContainedEDIType = 0;
    while(nextContainedEDIType < containedEDITypes) {
        SmartType_assert(nextContainedType < containedTypes);
        TYPECONST Type *containedType = type->getContainedType(nextContainedType);
        unsigned currExplicitEDITypes = explicitContainedEDITypes.size();
        unsigned i;
        for(i=nextContainedEDIType;i<nextContainedEDIType+containedEDIOptions;i++) {
            const EDIType containedEDIType = aEDIType->getContainedType(i);
            if(isTypeConsistent(containedType, &containedEDIType)) {
                explicitContainedEDITypes.push_back(new EDIType(containedEDIType));
                break;
            }
            if(!containedEDIType.isAggregateType()) {
                continue;
            }
            ret = flattenFunctionArgs(type, &containedEDIType, nextContainedType);
            if(ret == 0) {
                break;
            }
        }
        if(i >= nextContainedEDIType+containedEDIOptions) {
            while(explicitContainedEDITypes.size() > currExplicitEDITypes) {
                EDIType* aEDIType = explicitContainedEDITypes[explicitContainedEDITypes.size()-1];
                explicitContainedEDITypes.pop_back();
                delete aEDIType;
            }
            return -1;
        }
        nextContainedType += (explicitContainedEDITypes.size() - currExplicitEDITypes);
        nextContainedEDIType++;
    }
    return 0;
}

bool SmartType::isTy(bool isTyType, bool isTyEDIType, const char* source) const {
    bool check = (isTyType && isTyEDIType) || (!isTyType && !isTyEDIType);
    if(!check) {
        SmartTypeErr(source << " failed");
    }
    SmartType_assert(check);
    return isTyType;
}

//===----------------------------------------------------------------------===//
// Private static methods
//===----------------------------------------------------------------------===//
unsigned SmartType::getBFAFreeIdx(unsigned i, const std::vector<BitFieldAggregation> &inputBfas) {
    for(unsigned j=0;j<inputBfas.size();j++) {
        if(i<inputBfas[j].getEDITypeIndex()) {
            break;
        }
        else if(i>inputBfas[j].getEDITypeIndex()) {
            i += (inputBfas[j].getSize() - 1);
        }
        else {
            i = inputBfas[j].getRepresentativeEDITypeIndex();
            break;
        }
    }
    return i;
}

bool SmartType::isRawTypeConsistent(TYPECONST Type *type, const EDIType *aEDIType) {
    if(aEDIType->isVoidTy()) {
        return type->isVoidTy() || (type->isIntegerTy() && ((IntegerType*)type)->getBitWidth() == 8);
    }
    if(type->isFloatingPointTy()) {
        return aEDIType->isFloatingPointTy();
    }
    if(type->isIntegerTy()) {
        if(aEDIType->isCharTy() || aEDIType->isBoolTy()) {
            return (((IntegerType*)type)->getBitWidth() <= 8);
        }
        return aEDIType->isIntegerTy() || aEDIType->isEnumTy();
    }
    if(type->isFunctionTy()) {
        return aEDIType->isFunctionTy();
    }
    if(TypeUtil::isOpaqueTy(type)) {
        return (aEDIType->isOpaqueTy());
    }
    return false;
}

bool SmartType::isTypeConsistent2(std::vector<TYPECONST Type*> &nestedTypes, std::vector<const EDIType*> &nestedEDITypes, const SmartType *aSmartType) {
    assert(aSmartType->isUseExceptions());
    TYPECONST Type *type = aSmartType->getType();
    const EDIType *aEDIType = aSmartType->getEDIType();
    if(aEDIType->isPointerTy() || aEDIType->isUnionOrStructTy()) {
        for(unsigned j=0;j<nestedTypes.size();j++) {
            if(nestedTypes[j] == type && nestedEDITypes[j]->equals(aEDIType)) {
                return true;
            }
        }
    }

    unsigned nTypes = type->getNumContainedTypes();
    unsigned nEDITypes = aEDIType->getNumContainedTypes();
    if(nTypes == 0 || nEDITypes == 0) {
        if(nTypes != 0 || nEDITypes != 0) {
            return false;
        }
        return isRawTypeConsistent(type, aEDIType);
    }
    if(!aSmartType->verifyTy()) {
        return false;
    }
    unsigned numContainedTypes = aSmartType->getNumContainedTypes();
    nestedTypes.push_back(type);
    nestedEDITypes.push_back(aEDIType);
    for(unsigned i=0;i<numContainedTypes;i++) {
        const SmartType *containedSmartType = aSmartType->getContainedType(i);
        assert(containedSmartType->isUseExceptions());
        SmartType clonedSmartType(*containedSmartType);
        assert(clonedSmartType.isUseExceptions());
        delete containedSmartType;
        if(!isTypeConsistent2(nestedTypes, nestedEDITypes, &clonedSmartType)) {
            return false;
        }
    }
    nestedTypes.pop_back();
    nestedEDITypes.pop_back();
    return true;
}

bool SmartType::isTypeConsistent2(TYPECONST Type *type, const EDIType *aEDIType) {
    /* Exception-handling based isTypeConsistent(). Broken with -fno-exceptions. */
    static std::vector<TYPECONST Type*> nestedTypes;
    static std::vector<const EDIType*> nestedEDITypes;
    static unsigned level = 0;

    if(level == 0) {
        nestedTypes.clear();
        nestedEDITypes.clear();
    }

    bool checkTypeConsistent = false;
    bool useExceptions = true;
    level++;
    assert(useExceptions);
    TRY(
        const SmartType aSmartType(type, aEDIType, useExceptions);
        checkTypeConsistent = isTypeConsistent2(nestedTypes, nestedEDITypes, &aSmartType);
    )
    CATCH(std::exception& e,
        checkTypeConsistent = false;
    )
    level--;
    return checkTypeConsistent;
}

bool SmartType::isTypeConsistent(TYPECONST Type *type, const EDIType *aEDIType, bool useBfas, int *weakConsistencyLevel) {
    static std::vector<TYPECONST Type*> nestedTypes;
    static std::vector<const EDIType*> nestedEDITypes;
    static unsigned level = 0;

    if(level == 0) {
        if(weakConsistencyLevel) {
            *weakConsistencyLevel = INT_MAX;
        }
    }

    if(aEDIType->isPointerTy() || aEDIType->isUnionOrStructTy()) {
        for(unsigned j=0;j<nestedTypes.size();j++) {
            if(nestedTypes[j] == type && nestedEDITypes[j]->equals(aEDIType)) {
                return true;
            }
        }
    }

    unsigned nTypes = type->getNumContainedTypes();
    unsigned nEDITypes = aEDIType->getNumContainedTypes();
    if(nTypes == 0 || nEDITypes == 0) {
        if(nTypes != 0 || nEDITypes != 0) {
            return false;
        }
        return isRawTypeConsistent(type, aEDIType);
    }

    if(aEDIType->isOpaqueTy()) {
        return (TypeUtil::isOpaqueTy(type));
    }

    bool isArrayOrVectorTy = aEDIType->isArrayTy() || aEDIType->isVectorTy();
    unsigned nEDINumElements = aEDIType->getNumElements();
    if(aEDIType->isDerivedType() || isArrayOrVectorTy) {
        TYPECONST Type *nextType = type;
        if(aEDIType->isPointerTy()) {
            if(!type->isPointerTy()) {
                return false;
            }
            nextType = type->getContainedType(0);
        }
        else if(aEDIType->isArrayTy()) {
            if(!type->isArrayTy() || ((ArrayType*)type)->getNumElements() != nEDINumElements) {
                return false;
            }
            nextType = type->getContainedType(0);
        }
        else if(aEDIType->isVectorTy()) {
            if(!type->isVectorTy() || ((VectorType*)type)->getNumElements() != nEDINumElements) {
                return false;
            }
            nextType = type->getContainedType(0);
        }
        const EDIType aEDISubType(aEDIType->getContainedType(0));
        nestedEDITypes.push_back(aEDIType);
        nestedTypes.push_back(type);
        level++;
        bool ret = isTypeConsistent(nextType, &aEDISubType, useBfas, weakConsistencyLevel);
        level--;
        nestedTypes.pop_back();
        nestedEDITypes.pop_back();
        return ret;
    }
    else if(aEDIType->isCompositeType()) {
        if(!aEDIType->isUnionOrStructTy() && !aEDIType->isVectorTy() && !aEDIType->isFunctionTy()) {
            return false;
        }
        if(aEDIType->isUnionOrStructTy() && !type->isStructTy()) {
            return false;
        }
        if(aEDIType->isVectorTy() && !type->isVectorTy()) {
            return false;
        }
        if(aEDIType->isFunctionTy() && !type->isFunctionTy()) {
            return false;
        }
        if(aEDIType->isUnionTy() || aEDIType->isFunctionTy()) {
            if(weakConsistencyLevel) {
                *weakConsistencyLevel = level;
            }
            return true; //xxx we should be less conservative here
        }
        unsigned numContainedEDITypes = aEDIType->getNumContainedTypes();
        std::vector<BitFieldAggregation> myBfas;
        if(numContainedEDITypes != type->getNumContainedTypes()) {
            if(!useBfas) {
                return false;
            }
            if(!BitFieldAggregation::getBitFieldAggregations(type, aEDIType, myBfas, true)) {
                return false;
            }
            for(unsigned i=0;i<myBfas.size();i++) {
                numContainedEDITypes -= (myBfas[i].getSize() - 1);
            }
            if(numContainedEDITypes != type->getNumContainedTypes()) {
                return false;
            }
            nestedEDITypes.push_back(aEDIType);
            nestedTypes.push_back(type);
            level++;
            for(unsigned i=0;i<numContainedEDITypes;i++) {
                const EDIType aEDISubType(aEDIType->getContainedType(getBFAFreeIdx(i, myBfas)));
                if(!isTypeConsistent(type->getContainedType(i), &aEDISubType, useBfas, weakConsistencyLevel)) {
                    level--;
                    nestedTypes.pop_back();
                    nestedEDITypes.pop_back();
                    return false;
                }
            }
            level--;
            nestedTypes.pop_back();
            nestedEDITypes.pop_back();
            return true;
        }
        nestedEDITypes.push_back(aEDIType);
        nestedTypes.push_back(type);
        level++;
        for(unsigned i=0;i<numContainedEDITypes;i++) {
            const EDIType aEDISubType(aEDIType->getContainedType(i));
            if(!isTypeConsistent(type->getContainedType(i), &aEDISubType, useBfas, weakConsistencyLevel)) {
                level--;
                nestedTypes.pop_back();
                nestedEDITypes.pop_back();
                return false;
            }
        }
        level--;
        nestedTypes.pop_back();
        nestedEDITypes.pop_back();
        return true;
    }
    return false;
}

std::vector<TYPECONST Type*> SmartType::equalsNestedTypes;

}
