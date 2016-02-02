//===--- Type.cpp - Type representation and manipulation ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements type-related functionality.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/CharUnits.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeVisitor.h"
#include "clang/Basic/Specifiers.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
using namespace clang;

bool Qualifiers::isStrictSupersetOf(Qualifiers Other) const {
  return (*this != Other) &&
    // CVR qualifiers superset
    (((Mask & CVRMask) | (Other.Mask & CVRMask)) == (Mask & CVRMask)) &&
    // ObjC GC qualifiers superset
    ((getObjCGCAttr() == Other.getObjCGCAttr()) ||
     (hasObjCGCAttr() && !Other.hasObjCGCAttr())) &&
    // Address space superset.
    ((getAddressSpace() == Other.getAddressSpace()) ||
     (hasAddressSpace()&& !Other.hasAddressSpace())) &&
    // Lifetime qualifier superset.
    ((getObjCLifetime() == Other.getObjCLifetime()) ||
     (hasObjCLifetime() && !Other.hasObjCLifetime()));
}

const IdentifierInfo* QualType::getBaseTypeIdentifier() const {
  const Type* ty = getTypePtr();
  NamedDecl *ND = nullptr;
  if (ty->isPointerType() || ty->isReferenceType())
    return ty->getPointeeType().getBaseTypeIdentifier();
  else if (ty->isRecordType())
    ND = ty->getAs<RecordType>()->getDecl();
  else if (ty->isEnumeralType())
    ND = ty->getAs<EnumType>()->getDecl();
  else if (ty->getTypeClass() == Type::Typedef)
    ND = ty->getAs<TypedefType>()->getDecl();
  else if (ty->isArrayType())
    return ty->castAsArrayTypeUnsafe()->
        getElementType().getBaseTypeIdentifier();

  if (ND)
    return ND->getIdentifier();
  return nullptr;
}

bool QualType::isConstant(QualType T, ASTContext &Ctx) {
  if (T.isConstQualified())
    return true;

  if (const ArrayType *AT = Ctx.getAsArrayType(T))
    return AT->getElementType().isConstant(Ctx);

  return T.getAddressSpace() == LangAS::opencl_constant;
}

unsigned ConstantArrayType::getNumAddressingBits(ASTContext &Context,
                                                 QualType ElementType,
                                               const llvm::APInt &NumElements) {
  uint64_t ElementSize = Context.getTypeSizeInChars(ElementType).getQuantity();

  // Fast path the common cases so we can avoid the conservative computation
  // below, which in common cases allocates "large" APSInt values, which are
  // slow.

  // If the element size is a power of 2, we can directly compute the additional
  // number of addressing bits beyond those required for the element count.
  if (llvm::isPowerOf2_64(ElementSize)) {
    return NumElements.getActiveBits() + llvm::Log2_64(ElementSize);
  }

  // If both the element count and element size fit in 32-bits, we can do the
  // computation directly in 64-bits.
  if ((ElementSize >> 32) == 0 && NumElements.getBitWidth() <= 64 &&
      (NumElements.getZExtValue() >> 32) == 0) {
    uint64_t TotalSize = NumElements.getZExtValue() * ElementSize;
    return 64 - llvm::countLeadingZeros(TotalSize);
  }

  // Otherwise, use APSInt to handle arbitrary sized values.
  llvm::APSInt SizeExtended(NumElements, true);
  unsigned SizeTypeBits = Context.getTypeSize(Context.getSizeType());
  SizeExtended = SizeExtended.extend(std::max(SizeTypeBits,
                                              SizeExtended.getBitWidth()) * 2);

  llvm::APSInt TotalSize(llvm::APInt(SizeExtended.getBitWidth(), ElementSize));
  TotalSize *= SizeExtended;  

  return TotalSize.getActiveBits();
}

unsigned ConstantArrayType::getMaxSizeBits(ASTContext &Context) {
  unsigned Bits = Context.getTypeSize(Context.getSizeType());
  
  // Limit the number of bits in size_t so that maximal bit size fits 64 bit
  // integer (see PR8256).  We can do this as currently there is no hardware
  // that supports full 64-bit virtual space.
  if (Bits > 61)
    Bits = 61;

  return Bits;
}

DependentSizedArrayType::DependentSizedArrayType(const ASTContext &Context, 
                                                 QualType et, QualType can,
                                                 Expr *e, ArraySizeModifier sm,
                                                 unsigned tq,
                                                 SourceRange brackets)
    : ArrayType(DependentSizedArray, et, can, sm, tq, 
                (et->containsUnexpandedParameterPack() ||
                 (e && e->containsUnexpandedParameterPack()))),
      Context(Context), SizeExpr((Stmt*) e), Brackets(brackets) 
{
}

void DependentSizedArrayType::Profile(llvm::FoldingSetNodeID &ID,
                                      const ASTContext &Context,
                                      QualType ET,
                                      ArraySizeModifier SizeMod,
                                      unsigned TypeQuals,
                                      Expr *E) {
  ID.AddPointer(ET.getAsOpaquePtr());
  ID.AddInteger(SizeMod);
  ID.AddInteger(TypeQuals);
  E->Profile(ID, Context, true);
}

DependentSizedExtVectorType::DependentSizedExtVectorType(const
                                                         ASTContext &Context,
                                                         QualType ElementType,
                                                         QualType can, 
                                                         Expr *SizeExpr, 
                                                         SourceLocation loc)
    : Type(DependentSizedExtVector, can, /*Dependent=*/true,
           /*InstantiationDependent=*/true,
           ElementType->isVariablyModifiedType(), 
           (ElementType->containsUnexpandedParameterPack() ||
            (SizeExpr && SizeExpr->containsUnexpandedParameterPack()))),
      Context(Context), SizeExpr(SizeExpr), ElementType(ElementType),
      loc(loc) 
{
}

void
DependentSizedExtVectorType::Profile(llvm::FoldingSetNodeID &ID,
                                     const ASTContext &Context,
                                     QualType ElementType, Expr *SizeExpr) {
  ID.AddPointer(ElementType.getAsOpaquePtr());
  SizeExpr->Profile(ID, Context, true);
}

VectorType::VectorType(QualType vecType, unsigned nElements, QualType canonType,
                       VectorKind vecKind)
  : Type(Vector, canonType, vecType->isDependentType(),
         vecType->isInstantiationDependentType(),
         vecType->isVariablyModifiedType(),
         vecType->containsUnexpandedParameterPack()),
    ElementType(vecType) 
{
  VectorTypeBits.VecKind = vecKind;
  VectorTypeBits.NumElements = nElements;
}

VectorType::VectorType(TypeClass tc, QualType vecType, unsigned nElements,
                       QualType canonType, VectorKind vecKind)
  : Type(tc, canonType, vecType->isDependentType(),
         vecType->isInstantiationDependentType(),
         vecType->isVariablyModifiedType(),
         vecType->containsUnexpandedParameterPack()), 
    ElementType(vecType) 
{
  VectorTypeBits.VecKind = vecKind;
  VectorTypeBits.NumElements = nElements;
}

/// getArrayElementTypeNoTypeQual - If this is an array type, return the
/// element type of the array, potentially with type qualifiers missing.
/// This method should never be used when type qualifiers are meaningful.
const Type *Type::getArrayElementTypeNoTypeQual() const {
  // If this is directly an array type, return it.
  if (const ArrayType *ATy = dyn_cast<ArrayType>(this))
    return ATy->getElementType().getTypePtr();

  // If the canonical form of this type isn't the right kind, reject it.
  if (!isa<ArrayType>(CanonicalType))
    return nullptr;

  // If this is a typedef for an array type, strip the typedef off without
  // losing all typedef information.
  return cast<ArrayType>(getUnqualifiedDesugaredType())
    ->getElementType().getTypePtr();
}

/// getDesugaredType - Return the specified type with any "sugar" removed from
/// the type.  This takes off typedefs, typeof's etc.  If the outer level of
/// the type is already concrete, it returns it unmodified.  This is similar
/// to getting the canonical type, but it doesn't remove *all* typedefs.  For
/// example, it returns "T*" as "T*", (not as "int*"), because the pointer is
/// concrete.
QualType QualType::getDesugaredType(QualType T, const ASTContext &Context) {
  SplitQualType split = getSplitDesugaredType(T);
  return Context.getQualifiedType(split.Ty, split.Quals);
}

QualType QualType::getSingleStepDesugaredTypeImpl(QualType type,
                                                  const ASTContext &Context) {
  SplitQualType split = type.split();
  QualType desugar = split.Ty->getLocallyUnqualifiedSingleStepDesugaredType();
  return Context.getQualifiedType(desugar, split.Quals);
}

QualType Type::getLocallyUnqualifiedSingleStepDesugaredType() const {
  switch (getTypeClass()) {
#define ABSTRACT_TYPE(Class, Parent)
#define TYPE(Class, Parent) \
  case Type::Class: { \
    const Class##Type *ty = cast<Class##Type>(this); \
    if (!ty->isSugared()) return QualType(ty, 0); \
    return ty->desugar(); \
  }
#include "clang/AST/TypeNodes.def"
  }
  llvm_unreachable("bad type kind!");
}

SplitQualType QualType::getSplitDesugaredType(QualType T) {
  QualifierCollector Qs;

  QualType Cur = T;
  while (true) {
    const Type *CurTy = Qs.strip(Cur);
    switch (CurTy->getTypeClass()) {
#define ABSTRACT_TYPE(Class, Parent)
#define TYPE(Class, Parent) \
    case Type::Class: { \
      const Class##Type *Ty = cast<Class##Type>(CurTy); \
      if (!Ty->isSugared()) \
        return SplitQualType(Ty, Qs); \
      Cur = Ty->desugar(); \
      break; \
    }
#include "clang/AST/TypeNodes.def"
    }
  }
}

SplitQualType QualType::getSplitUnqualifiedTypeImpl(QualType type) {
  SplitQualType split = type.split();

  // All the qualifiers we've seen so far.
  Qualifiers quals = split.Quals;

  // The last type node we saw with any nodes inside it.
  const Type *lastTypeWithQuals = split.Ty;

  while (true) {
    QualType next;

    // Do a single-step desugar, aborting the loop if the type isn't
    // sugared.
    switch (split.Ty->getTypeClass()) {
#define ABSTRACT_TYPE(Class, Parent)
#define TYPE(Class, Parent) \
    case Type::Class: { \
      const Class##Type *ty = cast<Class##Type>(split.Ty); \
      if (!ty->isSugared()) goto done; \
      next = ty->desugar(); \
      break; \
    }
#include "clang/AST/TypeNodes.def"
    }

    // Otherwise, split the underlying type.  If that yields qualifiers,
    // update the information.
    split = next.split();
    if (!split.Quals.empty()) {
      lastTypeWithQuals = split.Ty;
      quals.addConsistentQualifiers(split.Quals);
    }
  }

 done:
  return SplitQualType(lastTypeWithQuals, quals);
}

QualType QualType::IgnoreParens(QualType T) {
  // FIXME: this seems inherently un-qualifiers-safe.
  while (const ParenType *PT = T->getAs<ParenType>())
    T = PT->getInnerType();
  return T;
}

/// \brief This will check for a T (which should be a Type which can act as
/// sugar, such as a TypedefType) by removing any existing sugar until it
/// reaches a T or a non-sugared type.
template<typename T> static const T *getAsSugar(const Type *Cur) {
  while (true) {
    if (const T *Sugar = dyn_cast<T>(Cur))
      return Sugar;
    switch (Cur->getTypeClass()) {
#define ABSTRACT_TYPE(Class, Parent)
#define TYPE(Class, Parent) \
    case Type::Class: { \
      const Class##Type *Ty = cast<Class##Type>(Cur); \
      if (!Ty->isSugared()) return 0; \
      Cur = Ty->desugar().getTypePtr(); \
      break; \
    }
#include "clang/AST/TypeNodes.def"
    }
  }
}

template <> const TypedefType *Type::getAs() const {
  return getAsSugar<TypedefType>(this);
}

template <> const TemplateSpecializationType *Type::getAs() const {
  return getAsSugar<TemplateSpecializationType>(this);
}

template <> const AttributedType *Type::getAs() const {
  return getAsSugar<AttributedType>(this);
}

/// getUnqualifiedDesugaredType - Pull any qualifiers and syntactic
/// sugar off the given type.  This should produce an object of the
/// same dynamic type as the canonical type.
const Type *Type::getUnqualifiedDesugaredType() const {
  const Type *Cur = this;

  while (true) {
    switch (Cur->getTypeClass()) {
#define ABSTRACT_TYPE(Class, Parent)
#define TYPE(Class, Parent) \
    case Class: { \
      const Class##Type *Ty = cast<Class##Type>(Cur); \
      if (!Ty->isSugared()) return Cur; \
      Cur = Ty->desugar().getTypePtr(); \
      break; \
    }
#include "clang/AST/TypeNodes.def"
    }
  }
}
bool Type::isClassType() const {
  if (const RecordType *RT = getAs<RecordType>())
    return RT->getDecl()->isClass();
  return false;
}
bool Type::isStructureType() const {
  if (const RecordType *RT = getAs<RecordType>())
    return RT->getDecl()->isStruct();
  return false;
}
bool Type::isInterfaceType() const {
  if (const RecordType *RT = getAs<RecordType>())
    return RT->getDecl()->isInterface();
  return false;
}
bool Type::isStructureOrClassType() const {
  if (const RecordType *RT = getAs<RecordType>()) {
    RecordDecl *RD = RT->getDecl();
    return RD->isStruct() || RD->isClass() || RD->isInterface();
  }
  return false;
}
bool Type::isVoidPointerType() const {
  if (const PointerType *PT = getAs<PointerType>())
    return PT->getPointeeType()->isVoidType();
  return false;
}

bool Type::isUnionType() const {
  if (const RecordType *RT = getAs<RecordType>())
    return RT->getDecl()->isUnion();
  return false;
}

bool Type::isComplexType() const {
  if (const ComplexType *CT = dyn_cast<ComplexType>(CanonicalType))
    return CT->getElementType()->isFloatingType();
  return false;
}

bool Type::isComplexIntegerType() const {
  // Check for GCC complex integer extension.
  return getAsComplexIntegerType();
}

const ComplexType *Type::getAsComplexIntegerType() const {
  if (const ComplexType *Complex = getAs<ComplexType>())
    if (Complex->getElementType()->isIntegerType())
      return Complex;
  return nullptr;
}

QualType Type::getPointeeType() const {
  if (const PointerType *PT = getAs<PointerType>())
    return PT->getPointeeType();
  if (const ObjCObjectPointerType *OPT = getAs<ObjCObjectPointerType>())
    return OPT->getPointeeType();
  if (const BlockPointerType *BPT = getAs<BlockPointerType>())
    return BPT->getPointeeType();
  if (const ReferenceType *RT = getAs<ReferenceType>())
    return RT->getPointeeType();
  if (const MemberPointerType *MPT = getAs<MemberPointerType>())
    return MPT->getPointeeType();
  if (const DecayedType *DT = getAs<DecayedType>())
    return DT->getPointeeType();
  return QualType();
}

const RecordType *Type::getAsStructureType() const {
  // If this is directly a structure type, return it.
  if (const RecordType *RT = dyn_cast<RecordType>(this)) {
    if (RT->getDecl()->isStruct())
      return RT;
  }

  // If the canonical form of this type isn't the right kind, reject it.
  if (const RecordType *RT = dyn_cast<RecordType>(CanonicalType)) {
    if (!RT->getDecl()->isStruct())
      return nullptr;

    // If this is a typedef for a structure type, strip the typedef off without
    // losing all typedef information.
    return cast<RecordType>(getUnqualifiedDesugaredType());
  }
  return nullptr;
}

const RecordType *Type::getAsUnionType() const {
  // If this is directly a union type, return it.
  if (const RecordType *RT = dyn_cast<RecordType>(this)) {
    if (RT->getDecl()->isUnion())
      return RT;
  }

  // If the canonical form of this type isn't the right kind, reject it.
  if (const RecordType *RT = dyn_cast<RecordType>(CanonicalType)) {
    if (!RT->getDecl()->isUnion())
      return nullptr;

    // If this is a typedef for a union type, strip the typedef off without
    // losing all typedef information.
    return cast<RecordType>(getUnqualifiedDesugaredType());
  }

  return nullptr;
}

ObjCObjectType::ObjCObjectType(QualType Canonical, QualType Base,
                               ObjCProtocolDecl * const *Protocols,
                               unsigned NumProtocols)
  : Type(ObjCObject, Canonical, false, false, false, false),
    BaseType(Base) 
{
  ObjCObjectTypeBits.NumProtocols = NumProtocols;
  assert(getNumProtocols() == NumProtocols &&
         "bitfield overflow in protocol count");
  if (NumProtocols)
    memcpy(getProtocolStorage(), Protocols,
           NumProtocols * sizeof(ObjCProtocolDecl*));
}

const ObjCObjectType *Type::getAsObjCQualifiedInterfaceType() const {
  // There is no sugar for ObjCObjectType's, just return the canonical
  // type pointer if it is the right class.  There is no typedef information to
  // return and these cannot be Address-space qualified.
  if (const ObjCObjectType *T = getAs<ObjCObjectType>())
    if (T->getNumProtocols() && T->getInterface())
      return T;
  return nullptr;
}

bool Type::isObjCQualifiedInterfaceType() const {
  return getAsObjCQualifiedInterfaceType() != nullptr;
}

const ObjCObjectPointerType *Type::getAsObjCQualifiedIdType() const {
  // There is no sugar for ObjCQualifiedIdType's, just return the canonical
  // type pointer if it is the right class.
  if (const ObjCObjectPointerType *OPT = getAs<ObjCObjectPointerType>()) {
    if (OPT->isObjCQualifiedIdType())
      return OPT;
  }
  return nullptr;
}

const ObjCObjectPointerType *Type::getAsObjCQualifiedClassType() const {
  // There is no sugar for ObjCQualifiedClassType's, just return the canonical
  // type pointer if it is the right class.
  if (const ObjCObjectPointerType *OPT = getAs<ObjCObjectPointerType>()) {
    if (OPT->isObjCQualifiedClassType())
      return OPT;
  }
  return nullptr;
}

const ObjCObjectPointerType *Type::getAsObjCInterfacePointerType() const {
  if (const ObjCObjectPointerType *OPT = getAs<ObjCObjectPointerType>()) {
    if (OPT->getInterfaceType())
      return OPT;
  }
  return nullptr;
}

const CXXRecordDecl *Type::getPointeeCXXRecordDecl() const {
  QualType PointeeType;
  if (const PointerType *PT = getAs<PointerType>())
    PointeeType = PT->getPointeeType();
  else if (const ReferenceType *RT = getAs<ReferenceType>())
    PointeeType = RT->getPointeeType();
  else
    return nullptr;

  if (const RecordType *RT = PointeeType->getAs<RecordType>())
    return dyn_cast<CXXRecordDecl>(RT->getDecl());

  return nullptr;
}

CXXRecordDecl *Type::getAsCXXRecordDecl() const {
  return dyn_cast_or_null<CXXRecordDecl>(getAsTagDecl());
}

TagDecl *Type::getAsTagDecl() const {
  if (const auto *TT = getAs<TagType>())
    return cast<TagDecl>(TT->getDecl());
  if (const auto *Injected = getAs<InjectedClassNameType>())
    return Injected->getDecl();

  return nullptr;
}

namespace {
  class GetContainedAutoVisitor :
    public TypeVisitor<GetContainedAutoVisitor, AutoType*> {
  public:
    using TypeVisitor<GetContainedAutoVisitor, AutoType*>::Visit;
    AutoType *Visit(QualType T) {
      if (T.isNull())
        return nullptr;
      return Visit(T.getTypePtr());
    }

    // The 'auto' type itself.
    AutoType *VisitAutoType(const AutoType *AT) {
      return const_cast<AutoType*>(AT);
    }

    // Only these types can contain the desired 'auto' type.
    AutoType *VisitPointerType(const PointerType *T) {
      return Visit(T->getPointeeType());
    }
    AutoType *VisitBlockPointerType(const BlockPointerType *T) {
      return Visit(T->getPointeeType());
    }
    AutoType *VisitReferenceType(const ReferenceType *T) {
      return Visit(T->getPointeeTypeAsWritten());
    }
    AutoType *VisitMemberPointerType(const MemberPointerType *T) {
      return Visit(T->getPointeeType());
    }
    AutoType *VisitArrayType(const ArrayType *T) {
      return Visit(T->getElementType());
    }
    AutoType *VisitDependentSizedExtVectorType(
      const DependentSizedExtVectorType *T) {
      return Visit(T->getElementType());
    }
    AutoType *VisitVectorType(const VectorType *T) {
      return Visit(T->getElementType());
    }
    AutoType *VisitFunctionType(const FunctionType *T) {
      return Visit(T->getReturnType());
    }
    AutoType *VisitParenType(const ParenType *T) {
      return Visit(T->getInnerType());
    }
    AutoType *VisitAttributedType(const AttributedType *T) {
      return Visit(T->getModifiedType());
    }
    AutoType *VisitAdjustedType(const AdjustedType *T) {
      return Visit(T->getOriginalType());
    }
  };
}

AutoType *Type::getContainedAutoType() const {
  return GetContainedAutoVisitor().Visit(this);
}

bool Type::hasIntegerRepresentation() const {
  if (const VectorType *VT = dyn_cast<VectorType>(CanonicalType))
    return VT->getElementType()->isIntegerType();
  else
    return isIntegerType();
}

/// \brief Determine whether this type is an integral type.
///
/// This routine determines whether the given type is an integral type per 
/// C++ [basic.fundamental]p7. Although the C standard does not define the
/// term "integral type", it has a similar term "integer type", and in C++
/// the two terms are equivalent. However, C's "integer type" includes 
/// enumeration types, while C++'s "integer type" does not. The \c ASTContext
/// parameter is used to determine whether we should be following the C or
/// C++ rules when determining whether this type is an integral/integer type.
///
/// For cases where C permits "an integer type" and C++ permits "an integral
/// type", use this routine.
///
/// For cases where C permits "an integer type" and C++ permits "an integral
/// or enumeration type", use \c isIntegralOrEnumerationType() instead. 
///
/// \param Ctx The context in which this type occurs.
///
/// \returns true if the type is considered an integral type, false otherwise.
bool Type::isIntegralType(ASTContext &Ctx) const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() >= BuiltinType::Bool &&
    BT->getKind() <= BuiltinType::Int128;
  
  if (!Ctx.getLangOpts().CPlusPlus)
    if (const EnumType *ET = dyn_cast<EnumType>(CanonicalType))
      return ET->getDecl()->isComplete(); // Complete enum types are integral in C.
  
  return false;
}


bool Type::isIntegralOrUnscopedEnumerationType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() >= BuiltinType::Bool &&
           BT->getKind() <= BuiltinType::Int128;

  // Check for a complete enum type; incomplete enum types are not properly an
  // enumeration type in the sense required here.
  // C++0x: However, if the underlying type of the enum is fixed, it is
  // considered complete.
  if (const EnumType *ET = dyn_cast<EnumType>(CanonicalType))
    return ET->getDecl()->isComplete() && !ET->getDecl()->isScoped();

  return false;
}



bool Type::isCharType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() == BuiltinType::Char_U ||
           BT->getKind() == BuiltinType::UChar ||
           BT->getKind() == BuiltinType::Char_S ||
           BT->getKind() == BuiltinType::SChar;
  return false;
}

bool Type::isWideCharType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() == BuiltinType::WChar_S ||
           BT->getKind() == BuiltinType::WChar_U;
  return false;
}

bool Type::isChar16Type() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() == BuiltinType::Char16;
  return false;
}

bool Type::isChar32Type() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() == BuiltinType::Char32;
  return false;
}

/// \brief Determine whether this type is any of the built-in character
/// types.
bool Type::isAnyCharacterType() const {
  const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType);
  if (!BT) return false;
  switch (BT->getKind()) {
  default: return false;
  case BuiltinType::Char_U:
  case BuiltinType::UChar:
  case BuiltinType::WChar_U:
  case BuiltinType::Char16:
  case BuiltinType::Char32:
  case BuiltinType::Char_S:
  case BuiltinType::SChar:
  case BuiltinType::WChar_S:
    return true;
  }
}

/// isSignedIntegerType - Return true if this is an integer type that is
/// signed, according to C99 6.2.5p4 [char, signed char, short, int, long..],
/// an enum decl which has a signed representation
bool Type::isSignedIntegerType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType)) {
    return BT->getKind() >= BuiltinType::Char_S &&
           BT->getKind() <= BuiltinType::Int128;
  }

  if (const EnumType *ET = dyn_cast<EnumType>(CanonicalType)) {
    // Incomplete enum types are not treated as integer types.
    // FIXME: In C++, enum types are never integer types.
    if (ET->getDecl()->isComplete() && !ET->getDecl()->isScoped())
      return ET->getDecl()->getIntegerType()->isSignedIntegerType();
  }

  return false;
}

bool Type::isSignedIntegerOrEnumerationType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType)) {
    return BT->getKind() >= BuiltinType::Char_S &&
    BT->getKind() <= BuiltinType::Int128;
  }
  
  if (const EnumType *ET = dyn_cast<EnumType>(CanonicalType)) {
    if (ET->getDecl()->isComplete())
      return ET->getDecl()->getIntegerType()->isSignedIntegerType();
  }
  
  return false;
}

bool Type::hasSignedIntegerRepresentation() const {
  if (const VectorType *VT = dyn_cast<VectorType>(CanonicalType))
    return VT->getElementType()->isSignedIntegerOrEnumerationType();
  else
    return isSignedIntegerOrEnumerationType();
}

/// isUnsignedIntegerType - Return true if this is an integer type that is
/// unsigned, according to C99 6.2.5p6 [which returns true for _Bool], an enum
/// decl which has an unsigned representation
bool Type::isUnsignedIntegerType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType)) {
    return BT->getKind() >= BuiltinType::Bool &&
           BT->getKind() <= BuiltinType::UInt128;
  }

  if (const EnumType *ET = dyn_cast<EnumType>(CanonicalType)) {
    // Incomplete enum types are not treated as integer types.
    // FIXME: In C++, enum types are never integer types.
    if (ET->getDecl()->isComplete() && !ET->getDecl()->isScoped())
      return ET->getDecl()->getIntegerType()->isUnsignedIntegerType();
  }

  return false;
}

bool Type::isUnsignedIntegerOrEnumerationType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType)) {
    return BT->getKind() >= BuiltinType::Bool &&
    BT->getKind() <= BuiltinType::UInt128;
  }
  
  if (const EnumType *ET = dyn_cast<EnumType>(CanonicalType)) {
    if (ET->getDecl()->isComplete())
      return ET->getDecl()->getIntegerType()->isUnsignedIntegerType();
  }
  
  return false;
}

bool Type::hasUnsignedIntegerRepresentation() const {
  if (const VectorType *VT = dyn_cast<VectorType>(CanonicalType))
    return VT->getElementType()->isUnsignedIntegerOrEnumerationType();
  else
    return isUnsignedIntegerOrEnumerationType();
}

bool Type::isFloatingType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() >= BuiltinType::Half &&
           BT->getKind() <= BuiltinType::LongDouble;
  if (const ComplexType *CT = dyn_cast<ComplexType>(CanonicalType))
    return CT->getElementType()->isFloatingType();
  return false;
}

bool Type::hasFloatingRepresentation() const {
  if (const VectorType *VT = dyn_cast<VectorType>(CanonicalType))
    return VT->getElementType()->isFloatingType();
  else
    return isFloatingType();
}

bool Type::isRealFloatingType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->isFloatingPoint();
  return false;
}

bool Type::isRealType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() >= BuiltinType::Bool &&
           BT->getKind() <= BuiltinType::LongDouble;
  if (const EnumType *ET = dyn_cast<EnumType>(CanonicalType))
      return ET->getDecl()->isComplete() && !ET->getDecl()->isScoped();
  return false;
}

bool Type::isArithmeticType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() >= BuiltinType::Bool &&
           BT->getKind() <= BuiltinType::LongDouble;
  if (const EnumType *ET = dyn_cast<EnumType>(CanonicalType))
    // GCC allows forward declaration of enum types (forbid by C99 6.7.2.3p2).
    // If a body isn't seen by the time we get here, return false.
    //
    // C++0x: Enumerations are not arithmetic types. For now, just return
    // false for scoped enumerations since that will disable any
    // unwanted implicit conversions.
    return !ET->getDecl()->isScoped() && ET->getDecl()->isComplete();
  return isa<ComplexType>(CanonicalType);
}

Type::ScalarTypeKind Type::getScalarTypeKind() const {
  assert(isScalarType());

  const Type *T = CanonicalType.getTypePtr();
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(T)) {
    if (BT->getKind() == BuiltinType::Bool) return STK_Bool;
    if (BT->getKind() == BuiltinType::NullPtr) return STK_CPointer;
    if (BT->isInteger()) return STK_Integral;
    if (BT->isFloatingPoint()) return STK_Floating;
    llvm_unreachable("unknown scalar builtin type");
  } else if (isa<PointerType>(T)) {
    return STK_CPointer;
  } else if (isa<BlockPointerType>(T)) {
    return STK_BlockPointer;
  } else if (isa<ObjCObjectPointerType>(T)) {
    return STK_ObjCObjectPointer;
  } else if (isa<MemberPointerType>(T)) {
    return STK_MemberPointer;
  } else if (isa<EnumType>(T)) {
    assert(cast<EnumType>(T)->getDecl()->isComplete());
    return STK_Integral;
  } else if (const ComplexType *CT = dyn_cast<ComplexType>(T)) {
    if (CT->getElementType()->isRealFloatingType())
      return STK_FloatingComplex;
    return STK_IntegralComplex;
  }

  llvm_unreachable("unknown scalar type");
}

/// \brief Determines whether the type is a C++ aggregate type or C
/// aggregate or union type.
///
/// An aggregate type is an array or a class type (struct, union, or
/// class) that has no user-declared constructors, no private or
/// protected non-static data members, no base classes, and no virtual
/// functions (C++ [dcl.init.aggr]p1). The notion of an aggregate type
/// subsumes the notion of C aggregates (C99 6.2.5p21) because it also
/// includes union types.
bool Type::isAggregateType() const {
  if (const RecordType *Record = dyn_cast<RecordType>(CanonicalType)) {
    if (CXXRecordDecl *ClassDecl = dyn_cast<CXXRecordDecl>(Record->getDecl()))
      return ClassDecl->isAggregate();

    return true;
  }

  return isa<ArrayType>(CanonicalType);
}

/// isConstantSizeType - Return true if this is not a variable sized type,
/// according to the rules of C99 6.7.5p3.  It is not legal to call this on
/// incomplete types or dependent types.
bool Type::isConstantSizeType() const {
  assert(!isIncompleteType() && "This doesn't make sense for incomplete types");
  assert(!isDependentType() && "This doesn't make sense for dependent types");
  // The VAT must have a size, as it is known to be complete.
  return !isa<VariableArrayType>(CanonicalType);
}

/// isIncompleteType - Return true if this is an incomplete type (C99 6.2.5p1)
/// - a type that can describe objects, but which lacks information needed to
/// determine its size.
bool Type::isIncompleteType(NamedDecl **Def) const {
  if (Def)
    *Def = nullptr;

  switch (CanonicalType->getTypeClass()) {
  default: return false;
  case Builtin:
    // Void is the only incomplete builtin type.  Per C99 6.2.5p19, it can never
    // be completed.
    return isVoidType();
  case Enum: {
    EnumDecl *EnumD = cast<EnumType>(CanonicalType)->getDecl();
    if (Def)
      *Def = EnumD;
    
    // An enumeration with fixed underlying type is complete (C++0x 7.2p3).
    if (EnumD->isFixed())
      return false;
    
    return !EnumD->isCompleteDefinition();
  }
  case Record: {
    // A tagged type (struct/union/enum/class) is incomplete if the decl is a
    // forward declaration, but not a full definition (C99 6.2.5p22).
    RecordDecl *Rec = cast<RecordType>(CanonicalType)->getDecl();
    if (Def)
      *Def = Rec;
    return !Rec->isCompleteDefinition();
  }
  case ConstantArray:
    // An array is incomplete if its element type is incomplete
    // (C++ [dcl.array]p1).
    // We don't handle variable arrays (they're not allowed in C++) or
    // dependent-sized arrays (dependent types are never treated as incomplete).
    return cast<ArrayType>(CanonicalType)->getElementType()
             ->isIncompleteType(Def);
  case IncompleteArray:
    // An array of unknown size is an incomplete type (C99 6.2.5p22).
    return true;
  case ObjCObject:
    return cast<ObjCObjectType>(CanonicalType)->getBaseType()
             ->isIncompleteType(Def);
  case ObjCInterface: {
    // ObjC interfaces are incomplete if they are @class, not @interface.
    ObjCInterfaceDecl *Interface
      = cast<ObjCInterfaceType>(CanonicalType)->getDecl();
    if (Def)
      *Def = Interface;
    return !Interface->hasDefinition();
  }
  }
}

bool QualType::isPODType(ASTContext &Context) const {
  // C++11 has a more relaxed definition of POD.
  if (Context.getLangOpts().CPlusPlus11)
    return isCXX11PODType(Context);

  return isCXX98PODType(Context);
}

bool QualType::isCXX98PODType(ASTContext &Context) const {
  // The compiler shouldn't query this for incomplete types, but the user might.
  // We return false for that case. Except for incomplete arrays of PODs, which
  // are PODs according to the standard.
  if (isNull())
    return 0;
  
  if ((*this)->isIncompleteArrayType())
    return Context.getBaseElementType(*this).isCXX98PODType(Context);
    
  if ((*this)->isIncompleteType())
    return false;

  if (Context.getLangOpts().ObjCAutoRefCount) {
    switch (getObjCLifetime()) {
    case Qualifiers::OCL_ExplicitNone:
      return true;
      
    case Qualifiers::OCL_Strong:
    case Qualifiers::OCL_Weak:
    case Qualifiers::OCL_Autoreleasing:
      return false;

    case Qualifiers::OCL_None:
      break;
    }        
  }
  
  QualType CanonicalType = getTypePtr()->CanonicalType;
  switch (CanonicalType->getTypeClass()) {
    // Everything not explicitly mentioned is not POD.
  default: return false;
  case Type::VariableArray:
  case Type::ConstantArray:
    // IncompleteArray is handled above.
    return Context.getBaseElementType(*this).isCXX98PODType(Context);
        
  case Type::ObjCObjectPointer:
  case Type::BlockPointer:
  case Type::Builtin:
  case Type::Complex:
  case Type::Pointer:
  case Type::MemberPointer:
  case Type::Vector:
  case Type::ExtVector:
    return true;

  case Type::Enum:
    return true;

  case Type::Record:
    if (CXXRecordDecl *ClassDecl
          = dyn_cast<CXXRecordDecl>(cast<RecordType>(CanonicalType)->getDecl()))
      return ClassDecl->isPOD();

    // C struct/union is POD.
    return true;
  }
}

bool QualType::isTrivialType(ASTContext &Context) const {
  // The compiler shouldn't query this for incomplete types, but the user might.
  // We return false for that case. Except for incomplete arrays of PODs, which
  // are PODs according to the standard.
  if (isNull())
    return 0;
  
  if ((*this)->isArrayType())
    return Context.getBaseElementType(*this).isTrivialType(Context);
  
  // Return false for incomplete types after skipping any incomplete array
  // types which are expressly allowed by the standard and thus our API.
  if ((*this)->isIncompleteType())
    return false;
  
  if (Context.getLangOpts().ObjCAutoRefCount) {
    switch (getObjCLifetime()) {
    case Qualifiers::OCL_ExplicitNone:
      return true;
      
    case Qualifiers::OCL_Strong:
    case Qualifiers::OCL_Weak:
    case Qualifiers::OCL_Autoreleasing:
      return false;
      
    case Qualifiers::OCL_None:
      if ((*this)->isObjCLifetimeType())
        return false;
      break;
    }        
  }
  
  QualType CanonicalType = getTypePtr()->CanonicalType;
  if (CanonicalType->isDependentType())
    return false;
  
  // C++0x [basic.types]p9:
  //   Scalar types, trivial class types, arrays of such types, and
  //   cv-qualified versions of these types are collectively called trivial
  //   types.
  
  // As an extension, Clang treats vector types as Scalar types.
  if (CanonicalType->isScalarType() || CanonicalType->isVectorType())
    return true;
  if (const RecordType *RT = CanonicalType->getAs<RecordType>()) {
    if (const CXXRecordDecl *ClassDecl =
        dyn_cast<CXXRecordDecl>(RT->getDecl())) {
      // C++11 [class]p6:
      //   A trivial class is a class that has a default constructor,
      //   has no non-trivial default constructors, and is trivially
      //   copyable.
      return ClassDecl->hasDefaultConstructor() &&
             !ClassDecl->hasNonTrivialDefaultConstructor() &&
             ClassDecl->isTriviallyCopyable();
    }
    
    return true;
  }
  
  // No other types can match.
  return false;
}

bool QualType::isTriviallyCopyableType(ASTContext &Context) const {
  if ((*this)->isArrayType())
    return Context.getBaseElementType(*this).isTrivialType(Context);

  if (Context.getLangOpts().ObjCAutoRefCount) {
    switch (getObjCLifetime()) {
    case Qualifiers::OCL_ExplicitNone:
      return true;
      
    case Qualifiers::OCL_Strong:
    case Qualifiers::OCL_Weak:
    case Qualifiers::OCL_Autoreleasing:
      return false;
      
    case Qualifiers::OCL_None:
      if ((*this)->isObjCLifetimeType())
        return false;
      break;
    }        
  }

  // C++11 [basic.types]p9
  //   Scalar types, trivially copyable class types, arrays of such types, and
  //   non-volatile const-qualified versions of these types are collectively
  //   called trivially copyable types.

  QualType CanonicalType = getCanonicalType();
  if (CanonicalType->isDependentType())
    return false;

  if (CanonicalType.isVolatileQualified())
    return false;

  // Return false for incomplete types after skipping any incomplete array types
  // which are expressly allowed by the standard and thus our API.
  if (CanonicalType->isIncompleteType())
    return false;
 
  // As an extension, Clang treats vector types as Scalar types.
  if (CanonicalType->isScalarType() || CanonicalType->isVectorType())
    return true;

  if (const RecordType *RT = CanonicalType->getAs<RecordType>()) {
    if (const CXXRecordDecl *ClassDecl =
          dyn_cast<CXXRecordDecl>(RT->getDecl())) {
      if (!ClassDecl->isTriviallyCopyable()) return false;
    }

    return true;
  }

  // No other types can match.
  return false;
}



bool Type::isLiteralType(const ASTContext &Ctx) const {
  if (isDependentType())
    return false;

  // C++1y [basic.types]p10:
  //   A type is a literal type if it is:
  //   -- cv void; or
  if (Ctx.getLangOpts().CPlusPlus14 && isVoidType())
    return true;

  // C++11 [basic.types]p10:
  //   A type is a literal type if it is:
  //   [...]
  //   -- an array of literal type other than an array of runtime bound; or
  if (isVariableArrayType())
    return false;
  const Type *BaseTy = getBaseElementTypeUnsafe();
  assert(BaseTy && "NULL element type");

  // Return false for incomplete types after skipping any incomplete array
  // types; those are expressly allowed by the standard and thus our API.
  if (BaseTy->isIncompleteType())
    return false;

  // C++11 [basic.types]p10:
  //   A type is a literal type if it is:
  //    -- a scalar type; or
  // As an extension, Clang treats vector types and complex types as
  // literal types.
  if (BaseTy->isScalarType() || BaseTy->isVectorType() ||
      BaseTy->isAnyComplexType())
    return true;
  //    -- a reference type; or
  if (BaseTy->isReferenceType())
    return true;
  //    -- a class type that has all of the following properties:
  if (const RecordType *RT = BaseTy->getAs<RecordType>()) {
    //    -- a trivial destructor,
    //    -- every constructor call and full-expression in the
    //       brace-or-equal-initializers for non-static data members (if any)
    //       is a constant expression,
    //    -- it is an aggregate type or has at least one constexpr
    //       constructor or constructor template that is not a copy or move
    //       constructor, and
    //    -- all non-static data members and base classes of literal types
    //
    // We resolve DR1361 by ignoring the second bullet.
    if (const CXXRecordDecl *ClassDecl =
        dyn_cast<CXXRecordDecl>(RT->getDecl()))
      return ClassDecl->isLiteral();

    return true;
  }

  // We treat _Atomic T as a literal type if T is a literal type.
  if (const AtomicType *AT = BaseTy->getAs<AtomicType>())
    return AT->getValueType()->isLiteralType(Ctx);

  // If this type hasn't been deduced yet, then conservatively assume that
  // it'll work out to be a literal type.
  if (isa<AutoType>(BaseTy->getCanonicalTypeInternal()))
    return true;

  return false;
}

bool Type::isStandardLayoutType() const {
  if (isDependentType())
    return false;

  // C++0x [basic.types]p9:
  //   Scalar types, standard-layout class types, arrays of such types, and
  //   cv-qualified versions of these types are collectively called
  //   standard-layout types.
  const Type *BaseTy = getBaseElementTypeUnsafe();
  assert(BaseTy && "NULL element type");

  // Return false for incomplete types after skipping any incomplete array
  // types which are expressly allowed by the standard and thus our API.
  if (BaseTy->isIncompleteType())
    return false;

  // As an extension, Clang treats vector types as Scalar types.
  if (BaseTy->isScalarType() || BaseTy->isVectorType()) return true;
  if (const RecordType *RT = BaseTy->getAs<RecordType>()) {
    if (const CXXRecordDecl *ClassDecl =
        dyn_cast<CXXRecordDecl>(RT->getDecl()))
      if (!ClassDecl->isStandardLayout())
        return false;

    // Default to 'true' for non-C++ class types.
    // FIXME: This is a bit dubious, but plain C structs should trivially meet
    // all the requirements of standard layout classes.
    return true;
  }

  // No other types can match.
  return false;
}

// This is effectively the intersection of isTrivialType and
// isStandardLayoutType. We implement it directly to avoid redundant
// conversions from a type to a CXXRecordDecl.
bool QualType::isCXX11PODType(ASTContext &Context) const {
  const Type *ty = getTypePtr();
  if (ty->isDependentType())
    return false;

  if (Context.getLangOpts().ObjCAutoRefCount) {
    switch (getObjCLifetime()) {
    case Qualifiers::OCL_ExplicitNone:
      return true;
      
    case Qualifiers::OCL_Strong:
    case Qualifiers::OCL_Weak:
    case Qualifiers::OCL_Autoreleasing:
      return false;

    case Qualifiers::OCL_None:
      break;
    }        
  }

  // C++11 [basic.types]p9:
  //   Scalar types, POD classes, arrays of such types, and cv-qualified
  //   versions of these types are collectively called trivial types.
  const Type *BaseTy = ty->getBaseElementTypeUnsafe();
  assert(BaseTy && "NULL element type");

  // Return false for incomplete types after skipping any incomplete array
  // types which are expressly allowed by the standard and thus our API.
  if (BaseTy->isIncompleteType())
    return false;

  // As an extension, Clang treats vector types as Scalar types.
  if (BaseTy->isScalarType() || BaseTy->isVectorType()) return true;
  if (const RecordType *RT = BaseTy->getAs<RecordType>()) {
    if (const CXXRecordDecl *ClassDecl =
        dyn_cast<CXXRecordDecl>(RT->getDecl())) {
      // C++11 [class]p10:
      //   A POD struct is a non-union class that is both a trivial class [...]
      if (!ClassDecl->isTrivial()) return false;

      // C++11 [class]p10:
      //   A POD struct is a non-union class that is both a trivial class and
      //   a standard-layout class [...]
      if (!ClassDecl->isStandardLayout()) return false;

      // C++11 [class]p10:
      //   A POD struct is a non-union class that is both a trivial class and
      //   a standard-layout class, and has no non-static data members of type
      //   non-POD struct, non-POD union (or array of such types). [...]
      //
      // We don't directly query the recursive aspect as the requiremets for
      // both standard-layout classes and trivial classes apply recursively
      // already.
    }

    return true;
  }

  // No other types can match.
  return false;
}

bool Type::isPromotableIntegerType() const {
  if (const BuiltinType *BT = getAs<BuiltinType>())
    switch (BT->getKind()) {
    case BuiltinType::Bool:
    case BuiltinType::Char_S:
    case BuiltinType::Char_U:
    case BuiltinType::SChar:
    case BuiltinType::UChar:
    case BuiltinType::Short:
    case BuiltinType::UShort:
    case BuiltinType::WChar_S:
    case BuiltinType::WChar_U:
    case BuiltinType::Char16:
    case BuiltinType::Char32:
      return true;
    default:
      return false;
    }

  // Enumerated types are promotable to their compatible integer types
  // (C99 6.3.1.1) a.k.a. its underlying type (C++ [conv.prom]p2).
  if (const EnumType *ET = getAs<EnumType>()){
    if (this->isDependentType() || ET->getDecl()->getPromotionType().isNull()
        || ET->getDecl()->isScoped())
      return false;
    
    return true;
  }
  
  return false;
}

bool Type::isSpecifierType() const {
  // Note that this intentionally does not use the canonical type.
  switch (getTypeClass()) {
  case Builtin:
  case Record:
  case Enum:
  case Typedef:
  case Complex:
  case TypeOfExpr:
  case TypeOf:
  case TemplateTypeParm:
  case SubstTemplateTypeParm:
  case TemplateSpecialization:
  case Elaborated:
  case DependentName:
  case DependentTemplateSpecialization:
  case ObjCInterface:
  case ObjCObject:
  case ObjCObjectPointer: // FIXME: object pointers aren't really specifiers
    return true;
  default:
    return false;
  }
}

ElaboratedTypeKeyword
TypeWithKeyword::getKeywordForTypeSpec(unsigned TypeSpec) {
  switch (TypeSpec) {
  default: return ETK_None;
  case TST_typename: return ETK_Typename;
  case TST_class: return ETK_Class;
  case TST_struct: return ETK_Struct;
  case TST_interface: return ETK_Interface;
  case TST_union: return ETK_Union;
  case TST_enum: return ETK_Enum;
  }
}

TagTypeKind
TypeWithKeyword::getTagTypeKindForTypeSpec(unsigned TypeSpec) {
  switch(TypeSpec) {
  case TST_class: return TTK_Class;
  case TST_struct: return TTK_Struct;
  case TST_interface: return TTK_Interface;
  case TST_union: return TTK_Union;
  case TST_enum: return TTK_Enum;
  }
  
  llvm_unreachable("Type specifier is not a tag type kind.");
}

ElaboratedTypeKeyword
TypeWithKeyword::getKeywordForTagTypeKind(TagTypeKind Kind) {
  switch (Kind) {
  case TTK_Class: return ETK_Class;
  case TTK_Struct: return ETK_Struct;
  case TTK_Interface: return ETK_Interface;
  case TTK_Union: return ETK_Union;
  case TTK_Enum: return ETK_Enum;
  }
  llvm_unreachable("Unknown tag type kind.");
}

TagTypeKind
TypeWithKeyword::getTagTypeKindForKeyword(ElaboratedTypeKeyword Keyword) {
  switch (Keyword) {
  case ETK_Class: return TTK_Class;
  case ETK_Struct: return TTK_Struct;
  case ETK_Interface: return TTK_Interface;
  case ETK_Union: return TTK_Union;
  case ETK_Enum: return TTK_Enum;
  case ETK_None: // Fall through.
  case ETK_Typename:
    llvm_unreachable("Elaborated type keyword is not a tag type kind.");
  }
  llvm_unreachable("Unknown elaborated type keyword.");
}

bool
TypeWithKeyword::KeywordIsTagTypeKind(ElaboratedTypeKeyword Keyword) {
  switch (Keyword) {
  case ETK_None:
  case ETK_Typename:
    return false;
  case ETK_Class:
  case ETK_Struct:
  case ETK_Interface:
  case ETK_Union:
  case ETK_Enum:
    return true;
  }
  llvm_unreachable("Unknown elaborated type keyword.");
}

StringRef TypeWithKeyword::getKeywordName(ElaboratedTypeKeyword Keyword) {
  switch (Keyword) {
  case ETK_None: return "";
  case ETK_Typename: return "typename";
  case ETK_Class:  return "class";
  case ETK_Struct: return "struct";
  case ETK_Interface: return "__interface";
  case ETK_Union:  return "union";
  case ETK_Enum:   return "enum";
  }

  llvm_unreachable("Unknown elaborated type keyword.");
}

DependentTemplateSpecializationType::DependentTemplateSpecializationType(
                         ElaboratedTypeKeyword Keyword,
                         NestedNameSpecifier *NNS, const IdentifierInfo *Name,
                         unsigned NumArgs, const TemplateArgument *Args,
                         QualType Canon)
  : TypeWithKeyword(Keyword, DependentTemplateSpecialization, Canon, true, true,
                    /*VariablyModified=*/false,
                    NNS && NNS->containsUnexpandedParameterPack()),
    NNS(NNS), Name(Name), NumArgs(NumArgs) {
  assert((!NNS || NNS->isDependent()) &&
         "DependentTemplateSpecializatonType requires dependent qualifier");
  for (unsigned I = 0; I != NumArgs; ++I) {
    if (Args[I].containsUnexpandedParameterPack())
      setContainsUnexpandedParameterPack();

    new (&getArgBuffer()[I]) TemplateArgument(Args[I]);
  }
}

void
DependentTemplateSpecializationType::Profile(llvm::FoldingSetNodeID &ID,
                                             const ASTContext &Context,
                                             ElaboratedTypeKeyword Keyword,
                                             NestedNameSpecifier *Qualifier,
                                             const IdentifierInfo *Name,
                                             unsigned NumArgs,
                                             const TemplateArgument *Args) {
  ID.AddInteger(Keyword);
  ID.AddPointer(Qualifier);
  ID.AddPointer(Name);
  for (unsigned Idx = 0; Idx < NumArgs; ++Idx)
    Args[Idx].Profile(ID, Context);
}

bool Type::isElaboratedTypeSpecifier() const {
  ElaboratedTypeKeyword Keyword;
  if (const ElaboratedType *Elab = dyn_cast<ElaboratedType>(this))
    Keyword = Elab->getKeyword();
  else if (const DependentNameType *DepName = dyn_cast<DependentNameType>(this))
    Keyword = DepName->getKeyword();
  else if (const DependentTemplateSpecializationType *DepTST =
             dyn_cast<DependentTemplateSpecializationType>(this))
    Keyword = DepTST->getKeyword();
  else
    return false;

  return TypeWithKeyword::KeywordIsTagTypeKind(Keyword);
}

const char *Type::getTypeClassName() const {
  switch (TypeBits.TC) {
#define ABSTRACT_TYPE(Derived, Base)
#define TYPE(Derived, Base) case Derived: return #Derived;
#include "clang/AST/TypeNodes.def"
  }
  
  llvm_unreachable("Invalid type class.");
}

StringRef BuiltinType::getName(const PrintingPolicy &Policy) const {
  switch (getKind()) {
  case Void:              return "void";
  case Bool:              return Policy.Bool ? "bool" : "_Bool";
  case Char_S:            return "char";
  case Char_U:            return "char";
  case SChar:             return "signed char";
  case Short:             return "short";
  case Int:               return "int";
  case Long:              return "long";
  case LongLong:          return "long long";
  case Int128:            return "__int128";
  case UChar:             return "unsigned char";
  case UShort:            return "unsigned short";
  case UInt:              return "unsigned int";
  case ULong:             return "unsigned long";
  case ULongLong:         return "unsigned long long";
  case UInt128:           return "unsigned __int128";
  case Half:              return Policy.Half ? "half" : "__fp16";
  case Float:             return "float";
  case Double:            return "double";
  case LongDouble:        return "long double";
  case WChar_S:
  case WChar_U:           return Policy.MSWChar ? "__wchar_t" : "wchar_t";
  case Char16:            return "char16_t";
  case Char32:            return "char32_t";
  case NullPtr:           return "nullptr_t";
  case Overload:          return "<overloaded function type>";
  case BoundMember:       return "<bound member function type>";
  case PseudoObject:      return "<pseudo-object type>";
  case Dependent:         return "<dependent type>";
  case UnknownAny:        return "<unknown type>";
  case ARCUnbridgedCast:  return "<ARC unbridged cast type>";
  case BuiltinFn:         return "<builtin fn type>";
  case ObjCId:            return "id";
  case ObjCClass:         return "Class";
  case ObjCSel:           return "SEL";
  case OCLImage1d:        return "image1d_t";
  case OCLImage1dArray:   return "image1d_array_t";
  case OCLImage1dBuffer:  return "image1d_buffer_t";
  case OCLImage2d:        return "image2d_t";
  case OCLImage2dArray:   return "image2d_array_t";
  case OCLImage3d:        return "image3d_t";
  case OCLSampler:        return "sampler_t";
  case OCLEvent:          return "event_t";
  }
  
  llvm_unreachable("Invalid builtin type.");
}

QualType QualType::getNonLValueExprType(const ASTContext &Context) const {
  if (const ReferenceType *RefType = getTypePtr()->getAs<ReferenceType>())
    return RefType->getPointeeType();
  
  // C++0x [basic.lval]:
  //   Class prvalues can have cv-qualified types; non-class prvalues always 
  //   have cv-unqualified types.
  //
  // See also C99 6.3.2.1p2.
  if (!Context.getLangOpts().CPlusPlus ||
      (!getTypePtr()->isDependentType() && !getTypePtr()->isRecordType()))
    return getUnqualifiedType();
  
  return *this;
}

StringRef FunctionType::getNameForCallConv(CallingConv CC) {
  switch (CC) {
  case CC_C: return "cdecl";
  case CC_X86StdCall: return "stdcall";
  case CC_X86FastCall: return "fastcall";
  case CC_X86ThisCall: return "thiscall";
  case CC_X86Pascal: return "pascal";
  case CC_X86VectorCall: return "vectorcall";
  case CC_X86_64Win64: return "ms_abi";
  case CC_X86_64SysV: return "sysv_abi";
  case CC_AAPCS: return "aapcs";
  case CC_AAPCS_VFP: return "aapcs-vfp";
  case CC_PnaclCall: return "pnaclcall";
  case CC_IntelOclBicc: return "intel_ocl_bicc";
  }

  llvm_unreachable("Invalid calling convention.");
}

FunctionProtoType::FunctionProtoType(QualType result, ArrayRef<QualType> params,
                                     QualType canonical,
                                     const ExtProtoInfo &epi)
    : FunctionType(FunctionProto, result, canonical,
                   result->isDependentType(),
                   result->isInstantiationDependentType(),
                   result->isVariablyModifiedType(),
                   result->containsUnexpandedParameterPack(), epi.ExtInfo),
      NumParams(params.size()),
      NumExceptions(epi.ExceptionSpec.Exceptions.size()),
      ExceptionSpecType(epi.ExceptionSpec.Type),
      HasAnyConsumedParams(epi.ConsumedParameters != nullptr),
      Variadic(epi.Variadic), HasTrailingReturn(epi.HasTrailingReturn) {
  assert(NumParams == params.size() && "function has too many parameters");

  FunctionTypeBits.TypeQuals = epi.TypeQuals;
  FunctionTypeBits.RefQualifier = epi.RefQualifier;

  // Fill in the trailing argument array.
  QualType *argSlot = reinterpret_cast<QualType*>(this+1);
  for (unsigned i = 0; i != NumParams; ++i) {
    if (params[i]->isDependentType())
      setDependent();
    else if (params[i]->isInstantiationDependentType())
      setInstantiationDependent();

    if (params[i]->containsUnexpandedParameterPack())
      setContainsUnexpandedParameterPack();

    argSlot[i] = params[i];
  }

  if (getExceptionSpecType() == EST_Dynamic) {
    // Fill in the exception array.
    QualType *exnSlot = argSlot + NumParams;
    unsigned I = 0;
    for (QualType ExceptionType : epi.ExceptionSpec.Exceptions) {
      // Note that a dependent exception specification does *not* make
      // a type dependent; it's not even part of the C++ type system.
      if (ExceptionType->isInstantiationDependentType())
        setInstantiationDependent();

      if (ExceptionType->containsUnexpandedParameterPack())
        setContainsUnexpandedParameterPack();

      exnSlot[I++] = ExceptionType;
    }
  } else if (getExceptionSpecType() == EST_ComputedNoexcept) {
    // Store the noexcept expression and context.
    Expr **noexSlot = reinterpret_cast<Expr **>(argSlot + NumParams);
    *noexSlot = epi.ExceptionSpec.NoexceptExpr;

    if (epi.ExceptionSpec.NoexceptExpr) {
      if (epi.ExceptionSpec.NoexceptExpr->isValueDependent() ||
          epi.ExceptionSpec.NoexceptExpr->isInstantiationDependent())
        setInstantiationDependent();

      if (epi.ExceptionSpec.NoexceptExpr->containsUnexpandedParameterPack())
        setContainsUnexpandedParameterPack();
    }
  } else if (getExceptionSpecType() == EST_Uninstantiated) {
    // Store the function decl from which we will resolve our
    // exception specification.
    FunctionDecl **slot =
        reinterpret_cast<FunctionDecl **>(argSlot + NumParams);
    slot[0] = epi.ExceptionSpec.SourceDecl;
    slot[1] = epi.ExceptionSpec.SourceTemplate;
    // This exception specification doesn't make the type dependent, because
    // it's not instantiated as part of instantiating the type.
  } else if (getExceptionSpecType() == EST_Unevaluated) {
    // Store the function decl from which we will resolve our
    // exception specification.
    FunctionDecl **slot =
        reinterpret_cast<FunctionDecl **>(argSlot + NumParams);
    slot[0] = epi.ExceptionSpec.SourceDecl;
  }

  if (epi.ConsumedParameters) {
    bool *consumedParams = const_cast<bool *>(getConsumedParamsBuffer());
    for (unsigned i = 0; i != NumParams; ++i)
      consumedParams[i] = epi.ConsumedParameters[i];
  }
}

bool FunctionProtoType::hasDependentExceptionSpec() const {
  if (Expr *NE = getNoexceptExpr())
    return NE->isValueDependent();
  for (QualType ET : exceptions())
    // A pack expansion with a non-dependent pattern is still dependent,
    // because we don't know whether the pattern is in the exception spec
    // or not (that depends on whether the pack has 0 expansions).
    if (ET->isDependentType() || ET->getAs<PackExpansionType>())
      return true;
  return false;
}

FunctionProtoType::NoexceptResult
FunctionProtoType::getNoexceptSpec(const ASTContext &ctx) const {
  ExceptionSpecificationType est = getExceptionSpecType();
  if (est == EST_BasicNoexcept)
    return NR_Nothrow;

  if (est != EST_ComputedNoexcept)
    return NR_NoNoexcept;

  Expr *noexceptExpr = getNoexceptExpr();
  if (!noexceptExpr)
    return NR_BadNoexcept;
  if (noexceptExpr->isValueDependent())
    return NR_Dependent;

  llvm::APSInt value;
  bool isICE = noexceptExpr->isIntegerConstantExpr(value, ctx, nullptr,
                                                   /*evaluated*/false);
  (void)isICE;
  assert(isICE && "AST should not contain bad noexcept expressions.");

  return value.getBoolValue() ? NR_Nothrow : NR_Throw;
}

bool FunctionProtoType::isNothrow(const ASTContext &Ctx,
                                  bool ResultIfDependent) const {
  ExceptionSpecificationType EST = getExceptionSpecType();
  assert(EST != EST_Unevaluated && EST != EST_Uninstantiated);
  if (EST == EST_DynamicNone || EST == EST_BasicNoexcept)
    return true;

  if (EST == EST_Dynamic && ResultIfDependent == true) {
    // A dynamic exception specification is throwing unless every exception
    // type is an (unexpanded) pack expansion type.
    for (unsigned I = 0, N = NumExceptions; I != N; ++I)
      if (!getExceptionType(I)->getAs<PackExpansionType>())
        return false;
    return ResultIfDependent;
  }

  if (EST != EST_ComputedNoexcept)
    return false;

  NoexceptResult NR = getNoexceptSpec(Ctx);
  if (NR == NR_Dependent)
    return ResultIfDependent;
  return NR == NR_Nothrow;
}

bool FunctionProtoType::isTemplateVariadic() const {
  for (unsigned ArgIdx = getNumParams(); ArgIdx; --ArgIdx)
    if (isa<PackExpansionType>(getParamType(ArgIdx - 1)))
      return true;
  
  return false;
}

void FunctionProtoType::Profile(llvm::FoldingSetNodeID &ID, QualType Result,
                                const QualType *ArgTys, unsigned NumParams,
                                const ExtProtoInfo &epi,
                                const ASTContext &Context) {

  // We have to be careful not to get ambiguous profile encodings.
  // Note that valid type pointers are never ambiguous with anything else.
  //
  // The encoding grammar begins:
  //      type type* bool int bool 
  // If that final bool is true, then there is a section for the EH spec:
  //      bool type*
  // This is followed by an optional "consumed argument" section of the
  // same length as the first type sequence:
  //      bool*
  // Finally, we have the ext info and trailing return type flag:
  //      int bool
  // 
  // There is no ambiguity between the consumed arguments and an empty EH
  // spec because of the leading 'bool' which unambiguously indicates
  // whether the following bool is the EH spec or part of the arguments.

  ID.AddPointer(Result.getAsOpaquePtr());
  for (unsigned i = 0; i != NumParams; ++i)
    ID.AddPointer(ArgTys[i].getAsOpaquePtr());
  // This method is relatively performance sensitive, so as a performance
  // shortcut, use one AddInteger call instead of four for the next four
  // fields.
  assert(!(unsigned(epi.Variadic) & ~1) &&
         !(unsigned(epi.TypeQuals) & ~255) &&
         !(unsigned(epi.RefQualifier) & ~3) &&
         !(unsigned(epi.ExceptionSpec.Type) & ~15) &&
         "Values larger than expected.");
  ID.AddInteger(unsigned(epi.Variadic) +
                (epi.TypeQuals << 1) +
                (epi.RefQualifier << 9) +
                (epi.ExceptionSpec.Type << 11));
  if (epi.ExceptionSpec.Type == EST_Dynamic) {
    for (QualType Ex : epi.ExceptionSpec.Exceptions)
      ID.AddPointer(Ex.getAsOpaquePtr());
  } else if (epi.ExceptionSpec.Type == EST_ComputedNoexcept &&
             epi.ExceptionSpec.NoexceptExpr) {
    epi.ExceptionSpec.NoexceptExpr->Profile(ID, Context, false);
  } else if (epi.ExceptionSpec.Type == EST_Uninstantiated ||
             epi.ExceptionSpec.Type == EST_Unevaluated) {
    ID.AddPointer(epi.ExceptionSpec.SourceDecl->getCanonicalDecl());
  }
  if (epi.ConsumedParameters) {
    for (unsigned i = 0; i != NumParams; ++i)
      ID.AddBoolean(epi.ConsumedParameters[i]);
  }
  epi.ExtInfo.Profile(ID);
  ID.AddBoolean(epi.HasTrailingReturn);
}

void FunctionProtoType::Profile(llvm::FoldingSetNodeID &ID,
                                const ASTContext &Ctx) {
  Profile(ID, getReturnType(), param_type_begin(), NumParams, getExtProtoInfo(),
          Ctx);
}

QualType TypedefType::desugar() const {
  return getDecl()->getUnderlyingType();
}

TypeOfExprType::TypeOfExprType(Expr *E, QualType can)
  : Type(TypeOfExpr, can, E->isTypeDependent(), 
         E->isInstantiationDependent(),
         E->getType()->isVariablyModifiedType(),
         E->containsUnexpandedParameterPack()), 
    TOExpr(E) {
}

bool TypeOfExprType::isSugared() const {
  return !TOExpr->isTypeDependent();
}

QualType TypeOfExprType::desugar() const {
  if (isSugared())
    return getUnderlyingExpr()->getType();
  
  return QualType(this, 0);
}

void DependentTypeOfExprType::Profile(llvm::FoldingSetNodeID &ID,
                                      const ASTContext &Context, Expr *E) {
  E->Profile(ID, Context, true);
}

DecltypeType::DecltypeType(Expr *E, QualType underlyingType, QualType can)
  // C++11 [temp.type]p2: "If an expression e involves a template parameter,
  // decltype(e) denotes a unique dependent type." Hence a decltype type is
  // type-dependent even if its expression is only instantiation-dependent.
  : Type(Decltype, can, E->isInstantiationDependent(),
         E->isInstantiationDependent(),
         E->getType()->isVariablyModifiedType(), 
         E->containsUnexpandedParameterPack()), 
    E(E),
  UnderlyingType(underlyingType) {
}

bool DecltypeType::isSugared() const { return !E->isInstantiationDependent(); }

QualType DecltypeType::desugar() const {
  if (isSugared())
    return getUnderlyingType();
  
  return QualType(this, 0);
}

DependentDecltypeType::DependentDecltypeType(const ASTContext &Context, Expr *E)
  : DecltypeType(E, Context.DependentTy), Context(Context) { }

void DependentDecltypeType::Profile(llvm::FoldingSetNodeID &ID,
                                    const ASTContext &Context, Expr *E) {
  E->Profile(ID, Context, true);
}

TagType::TagType(TypeClass TC, const TagDecl *D, QualType can)
  : Type(TC, can, D->isDependentType(), 
         /*InstantiationDependent=*/D->isDependentType(),
         /*VariablyModified=*/false, 
         /*ContainsUnexpandedParameterPack=*/false),
    decl(const_cast<TagDecl*>(D)) {}

static TagDecl *getInterestingTagDecl(TagDecl *decl) {
  for (auto I : decl->redecls()) {
    if (I->isCompleteDefinition() || I->isBeingDefined())
      return I;
  }
  // If there's no definition (not even in progress), return what we have.
  return decl;
}

UnaryTransformType::UnaryTransformType(QualType BaseType,
                                       QualType UnderlyingType,
                                       UTTKind UKind,
                                       QualType CanonicalType)
  : Type(UnaryTransform, CanonicalType, UnderlyingType->isDependentType(),
         UnderlyingType->isInstantiationDependentType(),
         UnderlyingType->isVariablyModifiedType(),
         BaseType->containsUnexpandedParameterPack())
  , BaseType(BaseType), UnderlyingType(UnderlyingType), UKind(UKind)
{}

TagDecl *TagType::getDecl() const {
  return getInterestingTagDecl(decl);
}

bool TagType::isBeingDefined() const {
  return getDecl()->isBeingDefined();
}

bool AttributedType::isMSTypeSpec() const {
  switch (getAttrKind()) {
  default:  return false;
  case attr_ptr32:
  case attr_ptr64:
  case attr_sptr:
  case attr_uptr:
    return true;
  }
  llvm_unreachable("invalid attr kind");
}

bool AttributedType::isCallingConv() const {
  switch (getAttrKind()) {
  case attr_ptr32:
  case attr_ptr64:
  case attr_sptr:
  case attr_uptr:
  case attr_address_space:
  case attr_regparm:
  case attr_vector_size:
  case attr_neon_vector_type:
  case attr_neon_polyvector_type:
  case attr_objc_gc:
  case attr_objc_ownership:
  case attr_noreturn:
      return false;
  case attr_pcs:
  case attr_pcs_vfp:
  case attr_cdecl:
  case attr_fastcall:
  case attr_stdcall:
  case attr_thiscall:
  case attr_vectorcall:
  case attr_pascal:
  case attr_ms_abi:
  case attr_sysv_abi:
  case attr_pnaclcall:
  case attr_inteloclbicc:
    return true;
  }
  llvm_unreachable("invalid attr kind");
}

CXXRecordDecl *InjectedClassNameType::getDecl() const {
  return cast<CXXRecordDecl>(getInterestingTagDecl(Decl));
}

IdentifierInfo *TemplateTypeParmType::getIdentifier() const {
  return isCanonicalUnqualified() ? nullptr : getDecl()->getIdentifier();
}

SubstTemplateTypeParmPackType::
SubstTemplateTypeParmPackType(const TemplateTypeParmType *Param, 
                              QualType Canon,
                              const TemplateArgument &ArgPack)
  : Type(SubstTemplateTypeParmPack, Canon, true, true, false, true), 
    Replaced(Param), 
    Arguments(ArgPack.pack_begin()), NumArguments(ArgPack.pack_size()) 
{ 
}

TemplateArgument SubstTemplateTypeParmPackType::getArgumentPack() const {
  return TemplateArgument(Arguments, NumArguments);
}

void SubstTemplateTypeParmPackType::Profile(llvm::FoldingSetNodeID &ID) {
  Profile(ID, getReplacedParameter(), getArgumentPack());
}

void SubstTemplateTypeParmPackType::Profile(llvm::FoldingSetNodeID &ID,
                                           const TemplateTypeParmType *Replaced,
                                            const TemplateArgument &ArgPack) {
  ID.AddPointer(Replaced);
  ID.AddInteger(ArgPack.pack_size());
  for (const auto &P : ArgPack.pack_elements())
    ID.AddPointer(P.getAsType().getAsOpaquePtr());
}

bool TemplateSpecializationType::
anyDependentTemplateArguments(const TemplateArgumentListInfo &Args,
                              bool &InstantiationDependent) {
  return anyDependentTemplateArguments(Args.getArgumentArray(), Args.size(),
                                       InstantiationDependent);
}

bool TemplateSpecializationType::
anyDependentTemplateArguments(const TemplateArgumentLoc *Args, unsigned N,
                              bool &InstantiationDependent) {
  for (unsigned i = 0; i != N; ++i) {
    if (Args[i].getArgument().isDependent()) {
      InstantiationDependent = true;
      return true;
    }
    
    if (Args[i].getArgument().isInstantiationDependent())
      InstantiationDependent = true;
  }
  return false;
}

TemplateSpecializationType::
TemplateSpecializationType(TemplateName T,
                           const TemplateArgument *Args, unsigned NumArgs,
                           QualType Canon, QualType AliasedType)
  : Type(TemplateSpecialization,
         Canon.isNull()? QualType(this, 0) : Canon,
         Canon.isNull()? true : Canon->isDependentType(),
         Canon.isNull()? true : Canon->isInstantiationDependentType(),
         false,
         T.containsUnexpandedParameterPack()),
    Template(T), NumArgs(NumArgs), TypeAlias(!AliasedType.isNull()) {
  assert(!T.getAsDependentTemplateName() && 
         "Use DependentTemplateSpecializationType for dependent template-name");
  assert((T.getKind() == TemplateName::Template ||
          T.getKind() == TemplateName::SubstTemplateTemplateParm ||
          T.getKind() == TemplateName::SubstTemplateTemplateParmPack) &&
         "Unexpected template name for TemplateSpecializationType");

  TemplateArgument *TemplateArgs
    = reinterpret_cast<TemplateArgument *>(this + 1);
  for (unsigned Arg = 0; Arg < NumArgs; ++Arg) {
    // Update instantiation-dependent and variably-modified bits.
    // If the canonical type exists and is non-dependent, the template
    // specialization type can be non-dependent even if one of the type
    // arguments is. Given:
    //   template<typename T> using U = int;
    // U<T> is always non-dependent, irrespective of the type T.
    // However, U<Ts> contains an unexpanded parameter pack, even though
    // its expansion (and thus its desugared type) doesn't.
    if (Args[Arg].isInstantiationDependent())
      setInstantiationDependent();
    if (Args[Arg].getKind() == TemplateArgument::Type &&
        Args[Arg].getAsType()->isVariablyModifiedType())
      setVariablyModified();
    if (Args[Arg].containsUnexpandedParameterPack())
      setContainsUnexpandedParameterPack();
    new (&TemplateArgs[Arg]) TemplateArgument(Args[Arg]);
  }

  // Store the aliased type if this is a type alias template specialization.
  if (TypeAlias) {
    TemplateArgument *Begin = reinterpret_cast<TemplateArgument *>(this + 1);
    *reinterpret_cast<QualType*>(Begin + getNumArgs()) = AliasedType;
  }
}

void
TemplateSpecializationType::Profile(llvm::FoldingSetNodeID &ID,
                                    TemplateName T,
                                    const TemplateArgument *Args,
                                    unsigned NumArgs,
                                    const ASTContext &Context) {
  T.Profile(ID);
  for (unsigned Idx = 0; Idx < NumArgs; ++Idx)
    Args[Idx].Profile(ID, Context);
}

QualType
QualifierCollector::apply(const ASTContext &Context, QualType QT) const {
  if (!hasNonFastQualifiers())
    return QT.withFastQualifiers(getFastQualifiers());

  return Context.getQualifiedType(QT, *this);
}

QualType
QualifierCollector::apply(const ASTContext &Context, const Type *T) const {
  if (!hasNonFastQualifiers())
    return QualType(T, getFastQualifiers());

  return Context.getQualifiedType(T, *this);
}

void ObjCObjectTypeImpl::Profile(llvm::FoldingSetNodeID &ID,
                                 QualType BaseType,
                                 ObjCProtocolDecl * const *Protocols,
                                 unsigned NumProtocols) {
  ID.AddPointer(BaseType.getAsOpaquePtr());
  for (unsigned i = 0; i != NumProtocols; i++)
    ID.AddPointer(Protocols[i]);
}

void ObjCObjectTypeImpl::Profile(llvm::FoldingSetNodeID &ID) {
  Profile(ID, getBaseType(), qual_begin(), getNumProtocols());
}

namespace {

/// \brief The cached properties of a type.
class CachedProperties {
  Linkage L;
  bool local;

public:
  CachedProperties(Linkage L, bool local) : L(L), local(local) {}

  Linkage getLinkage() const { return L; }
  bool hasLocalOrUnnamedType() const { return local; }

  friend CachedProperties merge(CachedProperties L, CachedProperties R) {
    Linkage MergedLinkage = minLinkage(L.L, R.L);
    return CachedProperties(MergedLinkage,
                         L.hasLocalOrUnnamedType() | R.hasLocalOrUnnamedType());
  }
};
}

static CachedProperties computeCachedProperties(const Type *T);

namespace clang {
/// The type-property cache.  This is templated so as to be
/// instantiated at an internal type to prevent unnecessary symbol
/// leakage.
template <class Private> class TypePropertyCache {
public:
  static CachedProperties get(QualType T) {
    return get(T.getTypePtr());
  }

  static CachedProperties get(const Type *T) {
    ensure(T);
    return CachedProperties(T->TypeBits.getLinkage(),
                            T->TypeBits.hasLocalOrUnnamedType());
  }

  static void ensure(const Type *T) {
    // If the cache is valid, we're okay.
    if (T->TypeBits.isCacheValid()) return;

    // If this type is non-canonical, ask its canonical type for the
    // relevant information.
    if (!T->isCanonicalUnqualified()) {
      const Type *CT = T->getCanonicalTypeInternal().getTypePtr();
      ensure(CT);
      T->TypeBits.CacheValid = true;
      T->TypeBits.CachedLinkage = CT->TypeBits.CachedLinkage;
      T->TypeBits.CachedLocalOrUnnamed = CT->TypeBits.CachedLocalOrUnnamed;
      return;
    }

    // Compute the cached properties and then set the cache.
    CachedProperties Result = computeCachedProperties(T);
    T->TypeBits.CacheValid = true;
    T->TypeBits.CachedLinkage = Result.getLinkage();
    T->TypeBits.CachedLocalOrUnnamed = Result.hasLocalOrUnnamedType();
  }
};
}

// Instantiate the friend template at a private class.  In a
// reasonable implementation, these symbols will be internal.
// It is terrible that this is the best way to accomplish this.
namespace { class Private {}; }
typedef TypePropertyCache<Private> Cache;

static CachedProperties computeCachedProperties(const Type *T) {
  switch (T->getTypeClass()) {
#define TYPE(Class,Base)
#define NON_CANONICAL_TYPE(Class,Base) case Type::Class:
#include "clang/AST/TypeNodes.def"
    llvm_unreachable("didn't expect a non-canonical type here");

#define TYPE(Class,Base)
#define DEPENDENT_TYPE(Class,Base) case Type::Class:
#define NON_CANONICAL_UNLESS_DEPENDENT_TYPE(Class,Base) case Type::Class:
#include "clang/AST/TypeNodes.def"
    // Treat instantiation-dependent types as external.
    assert(T->isInstantiationDependentType());
    return CachedProperties(ExternalLinkage, false);

  case Type::Auto:
    // Give non-deduced 'auto' types external linkage. We should only see them
    // here in error recovery.
    return CachedProperties(ExternalLinkage, false);

  case Type::Builtin:
    // C++ [basic.link]p8:
    //   A type is said to have linkage if and only if:
    //     - it is a fundamental type (3.9.1); or
    return CachedProperties(ExternalLinkage, false);

  case Type::Record:
  case Type::Enum: {
    const TagDecl *Tag = cast<TagType>(T)->getDecl();

    // C++ [basic.link]p8:
    //     - it is a class or enumeration type that is named (or has a name
    //       for linkage purposes (7.1.3)) and the name has linkage; or
    //     -  it is a specialization of a class template (14); or
    Linkage L = Tag->getLinkageInternal();
    bool IsLocalOrUnnamed =
      Tag->getDeclContext()->isFunctionOrMethod() ||
      !Tag->hasNameForLinkage();
    return CachedProperties(L, IsLocalOrUnnamed);
  }

    // C++ [basic.link]p8:
    //   - it is a compound type (3.9.2) other than a class or enumeration, 
    //     compounded exclusively from types that have linkage; or
  case Type::Complex:
    return Cache::get(cast<ComplexType>(T)->getElementType());
  case Type::Pointer:
    return Cache::get(cast<PointerType>(T)->getPointeeType());
  case Type::BlockPointer:
    return Cache::get(cast<BlockPointerType>(T)->getPointeeType());
  case Type::LValueReference:
  case Type::RValueReference:
    return Cache::get(cast<ReferenceType>(T)->getPointeeType());
  case Type::MemberPointer: {
    const MemberPointerType *MPT = cast<MemberPointerType>(T);
    return merge(Cache::get(MPT->getClass()),
                 Cache::get(MPT->getPointeeType()));
  }
  case Type::ConstantArray:
  case Type::IncompleteArray:
  case Type::VariableArray:
    return Cache::get(cast<ArrayType>(T)->getElementType());
  case Type::Vector:
  case Type::ExtVector:
    return Cache::get(cast<VectorType>(T)->getElementType());
  case Type::FunctionNoProto:
    return Cache::get(cast<FunctionType>(T)->getReturnType());
  case Type::FunctionProto: {
    const FunctionProtoType *FPT = cast<FunctionProtoType>(T);
    CachedProperties result = Cache::get(FPT->getReturnType());
    for (const auto &ai : FPT->param_types())
      result = merge(result, Cache::get(ai));
    return result;
  }
  case Type::ObjCInterface: {
    Linkage L = cast<ObjCInterfaceType>(T)->getDecl()->getLinkageInternal();
    return CachedProperties(L, false);
  }
  case Type::ObjCObject:
    return Cache::get(cast<ObjCObjectType>(T)->getBaseType());
  case Type::ObjCObjectPointer:
    return Cache::get(cast<ObjCObjectPointerType>(T)->getPointeeType());
  case Type::Atomic:
    return Cache::get(cast<AtomicType>(T)->getValueType());
  }

  llvm_unreachable("unhandled type class");
}

/// \brief Determine the linkage of this type.
Linkage Type::getLinkage() const {
  Cache::ensure(this);
  return TypeBits.getLinkage();
}

bool Type::hasUnnamedOrLocalType() const {
  Cache::ensure(this);
  return TypeBits.hasLocalOrUnnamedType();
}

static LinkageInfo computeLinkageInfo(QualType T);

static LinkageInfo computeLinkageInfo(const Type *T) {
  switch (T->getTypeClass()) {
#define TYPE(Class,Base)
#define NON_CANONICAL_TYPE(Class,Base) case Type::Class:
#include "clang/AST/TypeNodes.def"
    llvm_unreachable("didn't expect a non-canonical type here");

#define TYPE(Class,Base)
#define DEPENDENT_TYPE(Class,Base) case Type::Class:
#define NON_CANONICAL_UNLESS_DEPENDENT_TYPE(Class,Base) case Type::Class:
#include "clang/AST/TypeNodes.def"
    // Treat instantiation-dependent types as external.
    assert(T->isInstantiationDependentType());
    return LinkageInfo::external();

  case Type::Builtin:
    return LinkageInfo::external();

  case Type::Auto:
    return LinkageInfo::external();

  case Type::Record:
  case Type::Enum:
    return cast<TagType>(T)->getDecl()->getLinkageAndVisibility();

  case Type::Complex:
    return computeLinkageInfo(cast<ComplexType>(T)->getElementType());
  case Type::Pointer:
    return computeLinkageInfo(cast<PointerType>(T)->getPointeeType());
  case Type::BlockPointer:
    return computeLinkageInfo(cast<BlockPointerType>(T)->getPointeeType());
  case Type::LValueReference:
  case Type::RValueReference:
    return computeLinkageInfo(cast<ReferenceType>(T)->getPointeeType());
  case Type::MemberPointer: {
    const MemberPointerType *MPT = cast<MemberPointerType>(T);
    LinkageInfo LV = computeLinkageInfo(MPT->getClass());
    LV.merge(computeLinkageInfo(MPT->getPointeeType()));
    return LV;
  }
  case Type::ConstantArray:
  case Type::IncompleteArray:
  case Type::VariableArray:
    return computeLinkageInfo(cast<ArrayType>(T)->getElementType());
  case Type::Vector:
  case Type::ExtVector:
    return computeLinkageInfo(cast<VectorType>(T)->getElementType());
  case Type::FunctionNoProto:
    return computeLinkageInfo(cast<FunctionType>(T)->getReturnType());
  case Type::FunctionProto: {
    const FunctionProtoType *FPT = cast<FunctionProtoType>(T);
    LinkageInfo LV = computeLinkageInfo(FPT->getReturnType());
    for (const auto &ai : FPT->param_types())
      LV.merge(computeLinkageInfo(ai));
    return LV;
  }
  case Type::ObjCInterface:
    return cast<ObjCInterfaceType>(T)->getDecl()->getLinkageAndVisibility();
  case Type::ObjCObject:
    return computeLinkageInfo(cast<ObjCObjectType>(T)->getBaseType());
  case Type::ObjCObjectPointer:
    return computeLinkageInfo(cast<ObjCObjectPointerType>(T)->getPointeeType());
  case Type::Atomic:
    return computeLinkageInfo(cast<AtomicType>(T)->getValueType());
  }

  llvm_unreachable("unhandled type class");
}

static LinkageInfo computeLinkageInfo(QualType T) {
  return computeLinkageInfo(T.getTypePtr());
}

bool Type::isLinkageValid() const {
  if (!TypeBits.isCacheValid())
    return true;

  return computeLinkageInfo(getCanonicalTypeInternal()).getLinkage() ==
    TypeBits.getLinkage();
}

LinkageInfo Type::getLinkageAndVisibility() const {
  if (!isCanonicalUnqualified())
    return computeLinkageInfo(getCanonicalTypeInternal());

  LinkageInfo LV = computeLinkageInfo(this);
  assert(LV.getLinkage() == getLinkage());
  return LV;
}

Qualifiers::ObjCLifetime Type::getObjCARCImplicitLifetime() const {
  if (isObjCARCImplicitlyUnretainedType())
    return Qualifiers::OCL_ExplicitNone;
  return Qualifiers::OCL_Strong;
}

bool Type::isObjCARCImplicitlyUnretainedType() const {
  assert(isObjCLifetimeType() &&
         "cannot query implicit lifetime for non-inferrable type");

  const Type *canon = getCanonicalTypeInternal().getTypePtr();

  // Walk down to the base type.  We don't care about qualifiers for this.
  while (const ArrayType *array = dyn_cast<ArrayType>(canon))
    canon = array->getElementType().getTypePtr();

  if (const ObjCObjectPointerType *opt
        = dyn_cast<ObjCObjectPointerType>(canon)) {
    // Class and Class<Protocol> don't require retension.
    if (opt->getObjectType()->isObjCClass())
      return true;
  }

  return false;
}

bool Type::isObjCNSObjectType() const {
  if (const TypedefType *typedefType = dyn_cast<TypedefType>(this))
    return typedefType->getDecl()->hasAttr<ObjCNSObjectAttr>();
  return false;
}
bool Type::isObjCRetainableType() const {
  return isObjCObjectPointerType() ||
         isBlockPointerType() ||
         isObjCNSObjectType();
}
bool Type::isObjCIndirectLifetimeType() const {
  if (isObjCLifetimeType())
    return true;
  if (const PointerType *OPT = getAs<PointerType>())
    return OPT->getPointeeType()->isObjCIndirectLifetimeType();
  if (const ReferenceType *Ref = getAs<ReferenceType>())
    return Ref->getPointeeType()->isObjCIndirectLifetimeType();
  if (const MemberPointerType *MemPtr = getAs<MemberPointerType>())
    return MemPtr->getPointeeType()->isObjCIndirectLifetimeType();
  return false;
}

/// Returns true if objects of this type have lifetime semantics under
/// ARC.
bool Type::isObjCLifetimeType() const {
  const Type *type = this;
  while (const ArrayType *array = type->getAsArrayTypeUnsafe())
    type = array->getElementType().getTypePtr();
  return type->isObjCRetainableType();
}

/// \brief Determine whether the given type T is a "bridgable" Objective-C type,
/// which is either an Objective-C object pointer type or an 
bool Type::isObjCARCBridgableType() const {
  return isObjCObjectPointerType() || isBlockPointerType();
}

/// \brief Determine whether the given type T is a "bridgeable" C type.
bool Type::isCARCBridgableType() const {
  const PointerType *Pointer = getAs<PointerType>();
  if (!Pointer)
    return false;
  
  QualType Pointee = Pointer->getPointeeType();
  return Pointee->isVoidType() || Pointee->isRecordType();
}

bool Type::hasSizedVLAType() const {
  if (!isVariablyModifiedType()) return false;

  if (const PointerType *ptr = getAs<PointerType>())
    return ptr->getPointeeType()->hasSizedVLAType();
  if (const ReferenceType *ref = getAs<ReferenceType>())
    return ref->getPointeeType()->hasSizedVLAType();
  if (const ArrayType *arr = getAsArrayTypeUnsafe()) {
    if (isa<VariableArrayType>(arr) && 
        cast<VariableArrayType>(arr)->getSizeExpr())
      return true;

    return arr->getElementType()->hasSizedVLAType();
  }

  return false;
}

QualType::DestructionKind QualType::isDestructedTypeImpl(QualType type) {
  switch (type.getObjCLifetime()) {
  case Qualifiers::OCL_None:
  case Qualifiers::OCL_ExplicitNone:
  case Qualifiers::OCL_Autoreleasing:
    break;

  case Qualifiers::OCL_Strong:
    return DK_objc_strong_lifetime;
  case Qualifiers::OCL_Weak:
    return DK_objc_weak_lifetime;
  }

  /// Currently, the only destruction kind we recognize is C++ objects
  /// with non-trivial destructors.
  const CXXRecordDecl *record =
    type->getBaseElementTypeUnsafe()->getAsCXXRecordDecl();
  if (record && record->hasDefinition() && !record->hasTrivialDestructor())
    return DK_cxx_destructor;

  return DK_none;
}

CXXRecordDecl *MemberPointerType::getMostRecentCXXRecordDecl() const {
  return getClass()->getAsCXXRecordDecl()->getMostRecentDecl();
}
