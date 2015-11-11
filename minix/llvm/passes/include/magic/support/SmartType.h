#ifndef SMART_TYPE_H
#define SMART_TYPE_H

#include <pass.h>
#include <magic/support/EDIType.h>
#include <magic/support/TypeUtil.h>
#include <magic/support/BitFieldAggregation.h>

using namespace llvm;

namespace llvm {

#define SmartTypeLog(M) DEBUG(dbgs() << "SmartType: " << M << "\n")
#define SmartTypeErr(M) errs() << "SmartType: " << M << "\n"

#if HAVE_EXCEPTIONS
#define THROW(E) throw E
#define TRY(B) try{ B }
#define CATCH(E, B) catch(E){ B }
#else
#define THROW(E) assert(0 && "throw: Exceptions disabled")
#define TRY(B) assert(0 && "try: Exceptions disabled");
#define CATCH(E, B) assert(0 && "catch: Exceptions disabled");
#endif

#define SmartType_assert(X) do {              \
        if(!(X)) {                        \
            if(useExceptions) {           \
                THROW(std::exception());  \
            }                             \
            errs() << "Assertion failed, dumping object...\n"; \
            errs() << "Name is: " << this->aEDIType.getName() << "\n"; \
            errs() << *this; \
        }                                 \
        assert(X);                        \
    } while(0)

class SmartType {
  public:
      SmartType(const SmartType& et);
      SmartType(TYPECONST Type *type, const DIType *aDIType, bool useExceptions=false, bool forceRawTypeRepresentation=false);
      SmartType(TYPECONST Type *type, const EDIType *aEDIType, bool useExceptions=false, bool forceRawTypeRepresentation=false);
      ~SmartType();

      SmartType& operator=(const SmartType& et);
      void cloneFrom(const SmartType& et);

      const std::string getDescription() const;
      const SmartType* getContainedType(unsigned i) const;
      unsigned getNumContainedTypes() const;
      const DIDerivedType& getMember(unsigned i) const;
      unsigned getUnionMemberIdx() const;
      const SmartType* getTopStructType(unsigned index) const;

      TYPECONST Type *getType() const;
      const EDIType *getEDIType() const;
      bool isTypeConsistent() const;
      bool hasInnerPointers() const;
      bool isVoidTy() const;
      bool isPrimitiveTy() const;
      bool isAggregateType() const;
      bool isFunctionTy() const;
      bool isStructTy() const;
      bool isArrayTy() const;
      bool isPointerTy() const;
      bool isOpaqueTy() const;
      bool isPaddedTy() const;
      unsigned getNumElements() const;
      bool isUseExceptions() const;

      void verify() const;
      bool verifyTy() const;
      void print(raw_ostream &OS) const;
      bool equals(const SmartType* other, bool isDebug=false) const;
      bool hasRawTypeRepresentation() const;

      static const SmartType* getSmartTypeFromGV(Module &M, GlobalVariable *GV, DIGlobalVariable *DIG = NULL);
      static const SmartType* getSmartTypeFromLV(Module &M, AllocaInst *AI, DIVariable *DIV = NULL);
      static const SmartType* getSmartTypeFromFunction(Module &M, Function *F, DISubprogram *DIS = NULL);
      static const SmartType* getStructSmartTypeByName(Module &M, GlobalVariable* GV, std::string &name, bool isUnion=false);
      static std::vector<const SmartType*>* getTopStructSmartTypes(Module &M, GlobalVariable* GV);
      static bool isTypeConsistent(TYPECONST Type *type, const EDIType *aEDIType, bool useBfas=true, int *weakConsistencyLevel=NULL);

  private:
      TYPECONST Type *type;
      EDIType aEDIType;
      bool hasExplicitContainedEDITypes;
      bool isInconsistent;
      std::vector<EDIType*> explicitContainedEDITypes;
      std::vector<BitFieldAggregation> bfas;
      bool useExceptions;
      bool rawTypeRepresentation;
      unsigned unionMemberIdx;
      static std::vector<TYPECONST Type*> equalsNestedTypes;
      static bool forceRawUnions;
      static bool forceRawBitfields;

      void init(TYPECONST Type *type, const EDIType *aEDIType, bool useExceptions, bool forceRawTypeRepresentation);
      void normalize();
      void flattenFunctionTy();
      int flattenFunctionArgs(TYPECONST Type *type, const EDIType *aEDIType, unsigned nextContainedType);
      bool isTy(bool isTyType, bool isTyEDIType, const char* source) const;

      static unsigned getBFAFreeIdx(unsigned i, const std::vector<BitFieldAggregation> &inputBfas);
      static bool isRawTypeConsistent(TYPECONST Type *type, const EDIType *aEDIType);
      static bool isTypeConsistent2(TYPECONST Type *type, const EDIType *aEDIType);
      static bool isTypeConsistent2(std::vector<TYPECONST Type*> &nestedTypes, std::vector<const EDIType*> &nestedEDITypes, const SmartType *aSmartType);
};

inline raw_ostream &operator<<(raw_ostream &OS, const SmartType &aSmartType) {
    aSmartType.print(OS);
    return OS;
}

inline TYPECONST Type *SmartType::getType() const {
    return type;
}

inline const EDIType *SmartType::getEDIType() const {
    return &aEDIType;
}

inline bool SmartType::isTypeConsistent() const {
    if(isInconsistent) {
        return false;
    }
    if(isFunctionTy() || hasRawTypeRepresentation()) {
        return true;
    }
    return isTypeConsistent(type, &aEDIType);
}

inline bool SmartType::hasInnerPointers() const {
    return aEDIType.hasInnerPointers();
}

inline bool SmartType::isVoidTy() const {
    return isTy(type->isVoidTy(), aEDIType.isVoidTy(), "isVoidTy");
}

inline bool SmartType::isPrimitiveTy() const {
    if(aEDIType.isComplexFloatingPointTy()) {
        assert(type->isStructTy());
        return true;
    }
    return isTy(PassUtil::isPrimitiveTy(type), aEDIType.isPrimitiveType(), "isPrimitiveTy");
}

inline bool SmartType::isAggregateType() const {
    return isTy(type->isAggregateType(), aEDIType.isAggregateType(), "isAggregateType");
}

inline bool SmartType::isFunctionTy() const {
    if(isOpaqueTy()) {
        return false;
    }
    return isTy(type->isFunctionTy(), aEDIType.isFunctionTy(), "isFunctionTy");
}

inline bool SmartType::isStructTy() const {
    if(aEDIType.isComplexFloatingPointTy()) {
        assert(type->isStructTy());
        return false;
    }
    if(aEDIType.isArrayTy() && TypeUtil::isArrayAsStructTy(type)) {
        return false;
    }
    if(isOpaqueTy()) {
        return false;
    }
    return isTy(type->isStructTy(), aEDIType.isUnionOrStructTy(), "isStructTy");
}

inline bool SmartType::isArrayTy() const {
    if(aEDIType.isArrayTy() && TypeUtil::isArrayAsStructTy(type)) {
        return true;
    }
    if (hasRawTypeRepresentation()) { // only possible for structs and bitfields
        return false;
    }
    return isTy(type->isArrayTy(), aEDIType.isArrayTy(), "isArrayTy");
}

inline bool SmartType::isPointerTy() const {
    return isTy(type->isPointerTy(), aEDIType.isPointerTy(), "isPointerTy");
}

inline bool SmartType::isOpaqueTy() const {
    return TypeUtil::isOpaqueTy(type) || aEDIType.isOpaqueTy();
}

inline bool SmartType::isPaddedTy() const {
    if(!isAggregateType() || hasRawTypeRepresentation()) {
        return false;
    }
    return TypeUtil::isPaddedType(type);
}

inline unsigned SmartType::getNumElements() const {
    if(!isArrayTy()) {
        return 0;
    }
    unsigned EDINumElements = aEDIType.getNumElements();
    unsigned numElements;
    if(type->isArrayTy()) {
        numElements = ((ArrayType*)type)->getNumElements();
    }
    else {
        assert(type->isStructTy());
        numElements = type->getNumContainedTypes();
    }
    if(numElements == 0) {
        assert(EDINumElements <= 1 || EDINumElements==UINT_MAX);
        return 0;
    }
    assert(numElements == EDINumElements);
    return numElements;
}

inline bool SmartType::isUseExceptions() const {
    return useExceptions;
}

inline bool SmartType::verifyTy() const {
    if(isVoidTy()) return true;
    if(isPrimitiveTy()) return true;
    if(isAggregateType()) return true;
    if(isFunctionTy()) return true;
    if(isStructTy()) return true;
    if(isArrayTy()) return true;
    if(isPointerTy()) return true;
    if(isOpaqueTy()) return true;

    return false;
}

inline bool SmartType::hasRawTypeRepresentation() const {
    return rawTypeRepresentation;
}

}

#endif
