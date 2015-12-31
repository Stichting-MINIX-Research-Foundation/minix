//===--- SemaOpenMP.cpp - Semantic Analysis for OpenMP constructs ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file
/// \brief This file implements semantic analysis for OpenMP directives and
/// clauses.
///
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTMutationListener.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclOpenMP.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/StmtOpenMP.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Basic/OpenMPKinds.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/Initialization.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/Scope.h"
#include "clang/Sema/ScopeInfo.h"
#include "clang/Sema/SemaInternal.h"
using namespace clang;

//===----------------------------------------------------------------------===//
// Stack of data-sharing attributes for variables
//===----------------------------------------------------------------------===//

namespace {
/// \brief Default data sharing attributes, which can be applied to directive.
enum DefaultDataSharingAttributes {
  DSA_unspecified = 0, /// \brief Data sharing attribute not specified.
  DSA_none = 1 << 0,   /// \brief Default data sharing attribute 'none'.
  DSA_shared = 1 << 1  /// \brief Default data sharing attribute 'shared'.
};

template <class T> struct MatchesAny {
  explicit MatchesAny(ArrayRef<T> Arr) : Arr(std::move(Arr)) {}
  bool operator()(T Kind) {
    for (auto KindEl : Arr)
      if (KindEl == Kind)
        return true;
    return false;
  }

private:
  ArrayRef<T> Arr;
};
struct MatchesAlways {
  MatchesAlways() {}
  template <class T> bool operator()(T) { return true; }
};

typedef MatchesAny<OpenMPClauseKind> MatchesAnyClause;
typedef MatchesAny<OpenMPDirectiveKind> MatchesAnyDirective;

/// \brief Stack for tracking declarations used in OpenMP directives and
/// clauses and their data-sharing attributes.
class DSAStackTy {
public:
  struct DSAVarData {
    OpenMPDirectiveKind DKind;
    OpenMPClauseKind CKind;
    DeclRefExpr *RefExpr;
    SourceLocation ImplicitDSALoc;
    DSAVarData()
        : DKind(OMPD_unknown), CKind(OMPC_unknown), RefExpr(nullptr),
          ImplicitDSALoc() {}
  };

private:
  struct DSAInfo {
    OpenMPClauseKind Attributes;
    DeclRefExpr *RefExpr;
  };
  typedef llvm::SmallDenseMap<VarDecl *, DSAInfo, 64> DeclSAMapTy;
  typedef llvm::SmallDenseMap<VarDecl *, DeclRefExpr *, 64> AlignedMapTy;

  struct SharingMapTy {
    DeclSAMapTy SharingMap;
    AlignedMapTy AlignedMap;
    DefaultDataSharingAttributes DefaultAttr;
    SourceLocation DefaultAttrLoc;
    OpenMPDirectiveKind Directive;
    DeclarationNameInfo DirectiveName;
    Scope *CurScope;
    SourceLocation ConstructLoc;
    bool OrderedRegion;
    SourceLocation InnerTeamsRegionLoc;
    SharingMapTy(OpenMPDirectiveKind DKind, DeclarationNameInfo Name,
                 Scope *CurScope, SourceLocation Loc)
        : SharingMap(), AlignedMap(), DefaultAttr(DSA_unspecified),
          Directive(DKind), DirectiveName(std::move(Name)), CurScope(CurScope),
          ConstructLoc(Loc), OrderedRegion(false), InnerTeamsRegionLoc() {}
    SharingMapTy()
        : SharingMap(), AlignedMap(), DefaultAttr(DSA_unspecified),
          Directive(OMPD_unknown), DirectiveName(), CurScope(nullptr),
          ConstructLoc(), OrderedRegion(false), InnerTeamsRegionLoc() {}
  };

  typedef SmallVector<SharingMapTy, 64> StackTy;

  /// \brief Stack of used declaration and their data-sharing attributes.
  StackTy Stack;
  Sema &SemaRef;

  typedef SmallVector<SharingMapTy, 8>::reverse_iterator reverse_iterator;

  DSAVarData getDSA(StackTy::reverse_iterator Iter, VarDecl *D);

  /// \brief Checks if the variable is a local for OpenMP region.
  bool isOpenMPLocal(VarDecl *D, StackTy::reverse_iterator Iter);

public:
  explicit DSAStackTy(Sema &S) : Stack(1), SemaRef(S) {}

  void push(OpenMPDirectiveKind DKind, const DeclarationNameInfo &DirName,
            Scope *CurScope, SourceLocation Loc) {
    Stack.push_back(SharingMapTy(DKind, DirName, CurScope, Loc));
    Stack.back().DefaultAttrLoc = Loc;
  }

  void pop() {
    assert(Stack.size() > 1 && "Data-sharing attributes stack is empty!");
    Stack.pop_back();
  }

  /// \brief If 'aligned' declaration for given variable \a D was not seen yet,
  /// add it and return NULL; otherwise return previous occurrence's expression
  /// for diagnostics.
  DeclRefExpr *addUniqueAligned(VarDecl *D, DeclRefExpr *NewDE);

  /// \brief Adds explicit data sharing attribute to the specified declaration.
  void addDSA(VarDecl *D, DeclRefExpr *E, OpenMPClauseKind A);

  /// \brief Returns data sharing attributes from top of the stack for the
  /// specified declaration.
  DSAVarData getTopDSA(VarDecl *D, bool FromParent);
  /// \brief Returns data-sharing attributes for the specified declaration.
  DSAVarData getImplicitDSA(VarDecl *D, bool FromParent);
  /// \brief Checks if the specified variables has data-sharing attributes which
  /// match specified \a CPred predicate in any directive which matches \a DPred
  /// predicate.
  template <class ClausesPredicate, class DirectivesPredicate>
  DSAVarData hasDSA(VarDecl *D, ClausesPredicate CPred,
                    DirectivesPredicate DPred, bool FromParent);
  /// \brief Checks if the specified variables has data-sharing attributes which
  /// match specified \a CPred predicate in any innermost directive which
  /// matches \a DPred predicate.
  template <class ClausesPredicate, class DirectivesPredicate>
  DSAVarData hasInnermostDSA(VarDecl *D, ClausesPredicate CPred,
                             DirectivesPredicate DPred,
                             bool FromParent);
  /// \brief Finds a directive which matches specified \a DPred predicate.
  template <class NamedDirectivesPredicate>
  bool hasDirective(NamedDirectivesPredicate DPred, bool FromParent);

  /// \brief Returns currently analyzed directive.
  OpenMPDirectiveKind getCurrentDirective() const {
    return Stack.back().Directive;
  }
  /// \brief Returns parent directive.
  OpenMPDirectiveKind getParentDirective() const {
    if (Stack.size() > 2)
      return Stack[Stack.size() - 2].Directive;
    return OMPD_unknown;
  }

  /// \brief Set default data sharing attribute to none.
  void setDefaultDSANone(SourceLocation Loc) {
    Stack.back().DefaultAttr = DSA_none;
    Stack.back().DefaultAttrLoc = Loc;
  }
  /// \brief Set default data sharing attribute to shared.
  void setDefaultDSAShared(SourceLocation Loc) {
    Stack.back().DefaultAttr = DSA_shared;
    Stack.back().DefaultAttrLoc = Loc;
  }

  DefaultDataSharingAttributes getDefaultDSA() const {
    return Stack.back().DefaultAttr;
  }
  SourceLocation getDefaultDSALocation() const {
    return Stack.back().DefaultAttrLoc;
  }

  /// \brief Checks if the specified variable is a threadprivate.
  bool isThreadPrivate(VarDecl *D) {
    DSAVarData DVar = getTopDSA(D, false);
    return isOpenMPThreadPrivate(DVar.CKind);
  }

  /// \brief Marks current region as ordered (it has an 'ordered' clause).
  void setOrderedRegion(bool IsOrdered = true) {
    Stack.back().OrderedRegion = IsOrdered;
  }
  /// \brief Returns true, if parent region is ordered (has associated
  /// 'ordered' clause), false - otherwise.
  bool isParentOrderedRegion() const {
    if (Stack.size() > 2)
      return Stack[Stack.size() - 2].OrderedRegion;
    return false;
  }

  /// \brief Marks current target region as one with closely nested teams
  /// region.
  void setParentTeamsRegionLoc(SourceLocation TeamsRegionLoc) {
    if (Stack.size() > 2)
      Stack[Stack.size() - 2].InnerTeamsRegionLoc = TeamsRegionLoc;
  }
  /// \brief Returns true, if current region has closely nested teams region.
  bool hasInnerTeamsRegion() const {
    return getInnerTeamsRegionLoc().isValid();
  }
  /// \brief Returns location of the nested teams region (if any).
  SourceLocation getInnerTeamsRegionLoc() const {
    if (Stack.size() > 1)
      return Stack.back().InnerTeamsRegionLoc;
    return SourceLocation();
  }

  Scope *getCurScope() const { return Stack.back().CurScope; }
  Scope *getCurScope() { return Stack.back().CurScope; }
  SourceLocation getConstructLoc() { return Stack.back().ConstructLoc; }
};
bool isParallelOrTaskRegion(OpenMPDirectiveKind DKind) {
  return isOpenMPParallelDirective(DKind) || DKind == OMPD_task ||
         isOpenMPTeamsDirective(DKind) || DKind == OMPD_unknown;
}
} // namespace

DSAStackTy::DSAVarData DSAStackTy::getDSA(StackTy::reverse_iterator Iter,
                                          VarDecl *D) {
  DSAVarData DVar;
  if (Iter == std::prev(Stack.rend())) {
    // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
    // in a region but not in construct]
    //  File-scope or namespace-scope variables referenced in called routines
    //  in the region are shared unless they appear in a threadprivate
    //  directive.
    if (!D->isFunctionOrMethodVarDecl() && !isa<ParmVarDecl>(D))
      DVar.CKind = OMPC_shared;

    // OpenMP [2.9.1.2, Data-sharing Attribute Rules for Variables Referenced
    // in a region but not in construct]
    //  Variables with static storage duration that are declared in called
    //  routines in the region are shared.
    if (D->hasGlobalStorage())
      DVar.CKind = OMPC_shared;

    return DVar;
  }

  DVar.DKind = Iter->Directive;
  // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
  // in a Construct, C/C++, predetermined, p.1]
  // Variables with automatic storage duration that are declared in a scope
  // inside the construct are private.
  if (isOpenMPLocal(D, Iter) && D->isLocalVarDecl() &&
      (D->getStorageClass() == SC_Auto || D->getStorageClass() == SC_None)) {
    DVar.CKind = OMPC_private;
    return DVar;
  }

  // Explicitly specified attributes and local variables with predetermined
  // attributes.
  if (Iter->SharingMap.count(D)) {
    DVar.RefExpr = Iter->SharingMap[D].RefExpr;
    DVar.CKind = Iter->SharingMap[D].Attributes;
    DVar.ImplicitDSALoc = Iter->DefaultAttrLoc;
    return DVar;
  }

  // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
  // in a Construct, C/C++, implicitly determined, p.1]
  //  In a parallel or task construct, the data-sharing attributes of these
  //  variables are determined by the default clause, if present.
  switch (Iter->DefaultAttr) {
  case DSA_shared:
    DVar.CKind = OMPC_shared;
    DVar.ImplicitDSALoc = Iter->DefaultAttrLoc;
    return DVar;
  case DSA_none:
    return DVar;
  case DSA_unspecified:
    // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
    // in a Construct, implicitly determined, p.2]
    //  In a parallel construct, if no default clause is present, these
    //  variables are shared.
    DVar.ImplicitDSALoc = Iter->DefaultAttrLoc;
    if (isOpenMPParallelDirective(DVar.DKind) ||
        isOpenMPTeamsDirective(DVar.DKind)) {
      DVar.CKind = OMPC_shared;
      return DVar;
    }

    // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
    // in a Construct, implicitly determined, p.4]
    //  In a task construct, if no default clause is present, a variable that in
    //  the enclosing context is determined to be shared by all implicit tasks
    //  bound to the current team is shared.
    if (DVar.DKind == OMPD_task) {
      DSAVarData DVarTemp;
      for (StackTy::reverse_iterator I = std::next(Iter),
                                     EE = std::prev(Stack.rend());
           I != EE; ++I) {
        // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables
        // Referenced
        // in a Construct, implicitly determined, p.6]
        //  In a task construct, if no default clause is present, a variable
        //  whose data-sharing attribute is not determined by the rules above is
        //  firstprivate.
        DVarTemp = getDSA(I, D);
        if (DVarTemp.CKind != OMPC_shared) {
          DVar.RefExpr = nullptr;
          DVar.DKind = OMPD_task;
          DVar.CKind = OMPC_firstprivate;
          return DVar;
        }
        if (isParallelOrTaskRegion(I->Directive))
          break;
      }
      DVar.DKind = OMPD_task;
      DVar.CKind =
          (DVarTemp.CKind == OMPC_unknown) ? OMPC_firstprivate : OMPC_shared;
      return DVar;
    }
  }
  // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
  // in a Construct, implicitly determined, p.3]
  //  For constructs other than task, if no default clause is present, these
  //  variables inherit their data-sharing attributes from the enclosing
  //  context.
  return getDSA(std::next(Iter), D);
}

DeclRefExpr *DSAStackTy::addUniqueAligned(VarDecl *D, DeclRefExpr *NewDE) {
  assert(Stack.size() > 1 && "Data sharing attributes stack is empty");
  auto It = Stack.back().AlignedMap.find(D);
  if (It == Stack.back().AlignedMap.end()) {
    assert(NewDE && "Unexpected nullptr expr to be added into aligned map");
    Stack.back().AlignedMap[D] = NewDE;
    return nullptr;
  } else {
    assert(It->second && "Unexpected nullptr expr in the aligned map");
    return It->second;
  }
  return nullptr;
}

void DSAStackTy::addDSA(VarDecl *D, DeclRefExpr *E, OpenMPClauseKind A) {
  if (A == OMPC_threadprivate) {
    Stack[0].SharingMap[D].Attributes = A;
    Stack[0].SharingMap[D].RefExpr = E;
  } else {
    assert(Stack.size() > 1 && "Data-sharing attributes stack is empty");
    Stack.back().SharingMap[D].Attributes = A;
    Stack.back().SharingMap[D].RefExpr = E;
  }
}

bool DSAStackTy::isOpenMPLocal(VarDecl *D, StackTy::reverse_iterator Iter) {
  if (Stack.size() > 2) {
    reverse_iterator I = Iter, E = std::prev(Stack.rend());
    Scope *TopScope = nullptr;
    while (I != E && !isParallelOrTaskRegion(I->Directive)) {
      ++I;
    }
    if (I == E)
      return false;
    TopScope = I->CurScope ? I->CurScope->getParent() : nullptr;
    Scope *CurScope = getCurScope();
    while (CurScope != TopScope && !CurScope->isDeclScope(D)) {
      CurScope = CurScope->getParent();
    }
    return CurScope != TopScope;
  }
  return false;
}

DSAStackTy::DSAVarData DSAStackTy::getTopDSA(VarDecl *D, bool FromParent) {
  DSAVarData DVar;

  // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
  // in a Construct, C/C++, predetermined, p.1]
  //  Variables appearing in threadprivate directives are threadprivate.
  if (D->getTLSKind() != VarDecl::TLS_None ||
      D->getStorageClass() == SC_Register) {
    DVar.CKind = OMPC_threadprivate;
    return DVar;
  }
  if (Stack[0].SharingMap.count(D)) {
    DVar.RefExpr = Stack[0].SharingMap[D].RefExpr;
    DVar.CKind = OMPC_threadprivate;
    return DVar;
  }

  // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
  // in a Construct, C/C++, predetermined, p.1]
  // Variables with automatic storage duration that are declared in a scope
  // inside the construct are private.
  OpenMPDirectiveKind Kind =
      FromParent ? getParentDirective() : getCurrentDirective();
  auto StartI = std::next(Stack.rbegin());
  auto EndI = std::prev(Stack.rend());
  if (FromParent && StartI != EndI) {
    StartI = std::next(StartI);
  }
  if (!isParallelOrTaskRegion(Kind)) {
    if (isOpenMPLocal(D, StartI) &&
        ((D->isLocalVarDecl() && (D->getStorageClass() == SC_Auto ||
                                  D->getStorageClass() == SC_None)) ||
         isa<ParmVarDecl>(D))) {
      DVar.CKind = OMPC_private;
      return DVar;
    }
  }

  // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
  // in a Construct, C/C++, predetermined, p.4]
  //  Static data members are shared.
  if (D->isStaticDataMember()) {
    // Variables with const-qualified type having no mutable member may be
    // listed in a firstprivate clause, even if they are static data members.
    DSAVarData DVarTemp = hasDSA(D, MatchesAnyClause(OMPC_firstprivate),
                                 MatchesAlways(), FromParent);
    if (DVarTemp.CKind == OMPC_firstprivate && DVarTemp.RefExpr)
      return DVar;

    DVar.CKind = OMPC_shared;
    return DVar;
  }

  QualType Type = D->getType().getNonReferenceType().getCanonicalType();
  bool IsConstant = Type.isConstant(SemaRef.getASTContext());
  while (Type->isArrayType()) {
    QualType ElemType = cast<ArrayType>(Type.getTypePtr())->getElementType();
    Type = ElemType.getNonReferenceType().getCanonicalType();
  }
  // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
  // in a Construct, C/C++, predetermined, p.6]
  //  Variables with const qualified type having no mutable member are
  //  shared.
  CXXRecordDecl *RD =
      SemaRef.getLangOpts().CPlusPlus ? Type->getAsCXXRecordDecl() : nullptr;
  if (IsConstant &&
      !(SemaRef.getLangOpts().CPlusPlus && RD && RD->hasMutableFields())) {
    // Variables with const-qualified type having no mutable member may be
    // listed in a firstprivate clause, even if they are static data members.
    DSAVarData DVarTemp = hasDSA(D, MatchesAnyClause(OMPC_firstprivate),
                                 MatchesAlways(), FromParent);
    if (DVarTemp.CKind == OMPC_firstprivate && DVarTemp.RefExpr)
      return DVar;

    DVar.CKind = OMPC_shared;
    return DVar;
  }

  // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
  // in a Construct, C/C++, predetermined, p.7]
  //  Variables with static storage duration that are declared in a scope
  //  inside the construct are shared.
  if (D->isStaticLocal()) {
    DVar.CKind = OMPC_shared;
    return DVar;
  }

  // Explicitly specified attributes and local variables with predetermined
  // attributes.
  auto I = std::prev(StartI);
  if (I->SharingMap.count(D)) {
    DVar.RefExpr = I->SharingMap[D].RefExpr;
    DVar.CKind = I->SharingMap[D].Attributes;
    DVar.ImplicitDSALoc = I->DefaultAttrLoc;
  }

  return DVar;
}

DSAStackTy::DSAVarData DSAStackTy::getImplicitDSA(VarDecl *D, bool FromParent) {
  auto StartI = Stack.rbegin();
  auto EndI = std::prev(Stack.rend());
  if (FromParent && StartI != EndI) {
    StartI = std::next(StartI);
  }
  return getDSA(StartI, D);
}

template <class ClausesPredicate, class DirectivesPredicate>
DSAStackTy::DSAVarData DSAStackTy::hasDSA(VarDecl *D, ClausesPredicate CPred,
                                          DirectivesPredicate DPred,
                                          bool FromParent) {
  auto StartI = std::next(Stack.rbegin());
  auto EndI = std::prev(Stack.rend());
  if (FromParent && StartI != EndI) {
    StartI = std::next(StartI);
  }
  for (auto I = StartI, EE = EndI; I != EE; ++I) {
    if (!DPred(I->Directive) && !isParallelOrTaskRegion(I->Directive))
      continue;
    DSAVarData DVar = getDSA(I, D);
    if (CPred(DVar.CKind))
      return DVar;
  }
  return DSAVarData();
}

template <class ClausesPredicate, class DirectivesPredicate>
DSAStackTy::DSAVarData
DSAStackTy::hasInnermostDSA(VarDecl *D, ClausesPredicate CPred,
                            DirectivesPredicate DPred, bool FromParent) {
  auto StartI = std::next(Stack.rbegin());
  auto EndI = std::prev(Stack.rend());
  if (FromParent && StartI != EndI) {
    StartI = std::next(StartI);
  }
  for (auto I = StartI, EE = EndI; I != EE; ++I) {
    if (!DPred(I->Directive))
      break;
    DSAVarData DVar = getDSA(I, D);
    if (CPred(DVar.CKind))
      return DVar;
    return DSAVarData();
  }
  return DSAVarData();
}

template <class NamedDirectivesPredicate>
bool DSAStackTy::hasDirective(NamedDirectivesPredicate DPred, bool FromParent) {
  auto StartI = std::next(Stack.rbegin());
  auto EndI = std::prev(Stack.rend());
  if (FromParent && StartI != EndI) {
    StartI = std::next(StartI);
  }
  for (auto I = StartI, EE = EndI; I != EE; ++I) {
    if (DPred(I->Directive, I->DirectiveName, I->ConstructLoc))
      return true;
  }
  return false;
}

void Sema::InitDataSharingAttributesStack() {
  VarDataSharingAttributesStack = new DSAStackTy(*this);
}

#define DSAStack static_cast<DSAStackTy *>(VarDataSharingAttributesStack)

bool Sema::IsOpenMPCapturedVar(VarDecl *VD) {
  assert(LangOpts.OpenMP && "OpenMP is not allowed");
  if (DSAStack->getCurrentDirective() != OMPD_unknown) {
    auto DVarPrivate = DSAStack->getTopDSA(VD, /*FromParent=*/false);
    if (DVarPrivate.CKind != OMPC_unknown && isOpenMPPrivate(DVarPrivate.CKind))
      return true;
    DVarPrivate = DSAStack->hasDSA(VD, isOpenMPPrivate, MatchesAlways(),
                                   /*FromParent=*/false);
    return DVarPrivate.CKind != OMPC_unknown;
  }
  return false;
}

void Sema::DestroyDataSharingAttributesStack() { delete DSAStack; }

void Sema::StartOpenMPDSABlock(OpenMPDirectiveKind DKind,
                               const DeclarationNameInfo &DirName,
                               Scope *CurScope, SourceLocation Loc) {
  DSAStack->push(DKind, DirName, CurScope, Loc);
  PushExpressionEvaluationContext(PotentiallyEvaluated);
}

void Sema::EndOpenMPDSABlock(Stmt *CurDirective) {
  // OpenMP [2.14.3.5, Restrictions, C/C++, p.1]
  //  A variable of class type (or array thereof) that appears in a lastprivate
  //  clause requires an accessible, unambiguous default constructor for the
  //  class type, unless the list item is also specified in a firstprivate
  //  clause.
  if (auto D = dyn_cast_or_null<OMPExecutableDirective>(CurDirective)) {
    for (auto C : D->clauses()) {
      if (auto Clause = dyn_cast<OMPLastprivateClause>(C)) {
        for (auto VarRef : Clause->varlists()) {
          if (VarRef->isValueDependent() || VarRef->isTypeDependent())
            continue;
          auto VD = cast<VarDecl>(cast<DeclRefExpr>(VarRef)->getDecl());
          auto DVar = DSAStack->getTopDSA(VD, false);
          if (DVar.CKind == OMPC_lastprivate) {
            SourceLocation ELoc = VarRef->getExprLoc();
            auto Type = VarRef->getType();
            if (Type->isArrayType())
              Type = QualType(Type->getArrayElementTypeNoTypeQual(), 0);
            CXXRecordDecl *RD =
                getLangOpts().CPlusPlus ? Type->getAsCXXRecordDecl() : nullptr;
            // FIXME This code must be replaced by actual constructing of the
            // lastprivate variable.
            if (RD) {
              CXXConstructorDecl *CD = LookupDefaultConstructor(RD);
              PartialDiagnostic PD =
                  PartialDiagnostic(PartialDiagnostic::NullDiagnostic());
              if (!CD ||
                  CheckConstructorAccess(
                      ELoc, CD, InitializedEntity::InitializeTemporary(Type),
                      CD->getAccess(), PD) == AR_inaccessible ||
                  CD->isDeleted()) {
                Diag(ELoc, diag::err_omp_required_method)
                    << getOpenMPClauseName(OMPC_lastprivate) << 0;
                bool IsDecl = VD->isThisDeclarationADefinition(Context) ==
                              VarDecl::DeclarationOnly;
                Diag(VD->getLocation(), IsDecl ? diag::note_previous_decl
                                               : diag::note_defined_here)
                    << VD;
                Diag(RD->getLocation(), diag::note_previous_decl) << RD;
                continue;
              }
              MarkFunctionReferenced(ELoc, CD);
              DiagnoseUseOfDecl(CD, ELoc);
            }
          }
        }
      }
    }
  }

  DSAStack->pop();
  DiscardCleanupsInEvaluationContext();
  PopExpressionEvaluationContext();
}

namespace {

class VarDeclFilterCCC : public CorrectionCandidateCallback {
private:
  Sema &SemaRef;

public:
  explicit VarDeclFilterCCC(Sema &S) : SemaRef(S) {}
  bool ValidateCandidate(const TypoCorrection &Candidate) override {
    NamedDecl *ND = Candidate.getCorrectionDecl();
    if (VarDecl *VD = dyn_cast_or_null<VarDecl>(ND)) {
      return VD->hasGlobalStorage() &&
             SemaRef.isDeclInScope(ND, SemaRef.getCurLexicalContext(),
                                   SemaRef.getCurScope());
    }
    return false;
  }
};
} // namespace

ExprResult Sema::ActOnOpenMPIdExpression(Scope *CurScope,
                                         CXXScopeSpec &ScopeSpec,
                                         const DeclarationNameInfo &Id) {
  LookupResult Lookup(*this, Id, LookupOrdinaryName);
  LookupParsedName(Lookup, CurScope, &ScopeSpec, true);

  if (Lookup.isAmbiguous())
    return ExprError();

  VarDecl *VD;
  if (!Lookup.isSingleResult()) {
    if (TypoCorrection Corrected = CorrectTypo(
            Id, LookupOrdinaryName, CurScope, nullptr,
            llvm::make_unique<VarDeclFilterCCC>(*this), CTK_ErrorRecovery)) {
      diagnoseTypo(Corrected,
                   PDiag(Lookup.empty()
                             ? diag::err_undeclared_var_use_suggest
                             : diag::err_omp_expected_var_arg_suggest)
                       << Id.getName());
      VD = Corrected.getCorrectionDeclAs<VarDecl>();
    } else {
      Diag(Id.getLoc(), Lookup.empty() ? diag::err_undeclared_var_use
                                       : diag::err_omp_expected_var_arg)
          << Id.getName();
      return ExprError();
    }
  } else {
    if (!(VD = Lookup.getAsSingle<VarDecl>())) {
      Diag(Id.getLoc(), diag::err_omp_expected_var_arg) << Id.getName();
      Diag(Lookup.getFoundDecl()->getLocation(), diag::note_declared_at);
      return ExprError();
    }
  }
  Lookup.suppressDiagnostics();

  // OpenMP [2.9.2, Syntax, C/C++]
  //   Variables must be file-scope, namespace-scope, or static block-scope.
  if (!VD->hasGlobalStorage()) {
    Diag(Id.getLoc(), diag::err_omp_global_var_arg)
        << getOpenMPDirectiveName(OMPD_threadprivate) << !VD->isStaticLocal();
    bool IsDecl =
        VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
    Diag(VD->getLocation(),
         IsDecl ? diag::note_previous_decl : diag::note_defined_here)
        << VD;
    return ExprError();
  }

  VarDecl *CanonicalVD = VD->getCanonicalDecl();
  NamedDecl *ND = cast<NamedDecl>(CanonicalVD);
  // OpenMP [2.9.2, Restrictions, C/C++, p.2]
  //   A threadprivate directive for file-scope variables must appear outside
  //   any definition or declaration.
  if (CanonicalVD->getDeclContext()->isTranslationUnit() &&
      !getCurLexicalContext()->isTranslationUnit()) {
    Diag(Id.getLoc(), diag::err_omp_var_scope)
        << getOpenMPDirectiveName(OMPD_threadprivate) << VD;
    bool IsDecl =
        VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
    Diag(VD->getLocation(),
         IsDecl ? diag::note_previous_decl : diag::note_defined_here)
        << VD;
    return ExprError();
  }
  // OpenMP [2.9.2, Restrictions, C/C++, p.3]
  //   A threadprivate directive for static class member variables must appear
  //   in the class definition, in the same scope in which the member
  //   variables are declared.
  if (CanonicalVD->isStaticDataMember() &&
      !CanonicalVD->getDeclContext()->Equals(getCurLexicalContext())) {
    Diag(Id.getLoc(), diag::err_omp_var_scope)
        << getOpenMPDirectiveName(OMPD_threadprivate) << VD;
    bool IsDecl =
        VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
    Diag(VD->getLocation(),
         IsDecl ? diag::note_previous_decl : diag::note_defined_here)
        << VD;
    return ExprError();
  }
  // OpenMP [2.9.2, Restrictions, C/C++, p.4]
  //   A threadprivate directive for namespace-scope variables must appear
  //   outside any definition or declaration other than the namespace
  //   definition itself.
  if (CanonicalVD->getDeclContext()->isNamespace() &&
      (!getCurLexicalContext()->isFileContext() ||
       !getCurLexicalContext()->Encloses(CanonicalVD->getDeclContext()))) {
    Diag(Id.getLoc(), diag::err_omp_var_scope)
        << getOpenMPDirectiveName(OMPD_threadprivate) << VD;
    bool IsDecl =
        VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
    Diag(VD->getLocation(),
         IsDecl ? diag::note_previous_decl : diag::note_defined_here)
        << VD;
    return ExprError();
  }
  // OpenMP [2.9.2, Restrictions, C/C++, p.6]
  //   A threadprivate directive for static block-scope variables must appear
  //   in the scope of the variable and not in a nested scope.
  if (CanonicalVD->isStaticLocal() && CurScope &&
      !isDeclInScope(ND, getCurLexicalContext(), CurScope)) {
    Diag(Id.getLoc(), diag::err_omp_var_scope)
        << getOpenMPDirectiveName(OMPD_threadprivate) << VD;
    bool IsDecl =
        VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
    Diag(VD->getLocation(),
         IsDecl ? diag::note_previous_decl : diag::note_defined_here)
        << VD;
    return ExprError();
  }

  // OpenMP [2.9.2, Restrictions, C/C++, p.2-6]
  //   A threadprivate directive must lexically precede all references to any
  //   of the variables in its list.
  if (VD->isUsed()) {
    Diag(Id.getLoc(), diag::err_omp_var_used)
        << getOpenMPDirectiveName(OMPD_threadprivate) << VD;
    return ExprError();
  }

  QualType ExprType = VD->getType().getNonReferenceType();
  ExprResult DE = BuildDeclRefExpr(VD, ExprType, VK_LValue, Id.getLoc());
  return DE;
}

Sema::DeclGroupPtrTy
Sema::ActOnOpenMPThreadprivateDirective(SourceLocation Loc,
                                        ArrayRef<Expr *> VarList) {
  if (OMPThreadPrivateDecl *D = CheckOMPThreadPrivateDecl(Loc, VarList)) {
    CurContext->addDecl(D);
    return DeclGroupPtrTy::make(DeclGroupRef(D));
  }
  return DeclGroupPtrTy();
}

namespace {
class LocalVarRefChecker : public ConstStmtVisitor<LocalVarRefChecker, bool> {
  Sema &SemaRef;

public:
  bool VisitDeclRefExpr(const DeclRefExpr *E) {
    if (auto VD = dyn_cast<VarDecl>(E->getDecl())) {
      if (VD->hasLocalStorage()) {
        SemaRef.Diag(E->getLocStart(),
                     diag::err_omp_local_var_in_threadprivate_init)
            << E->getSourceRange();
        SemaRef.Diag(VD->getLocation(), diag::note_defined_here)
            << VD << VD->getSourceRange();
        return true;
      }
    }
    return false;
  }
  bool VisitStmt(const Stmt *S) {
    for (auto Child : S->children()) {
      if (Child && Visit(Child))
        return true;
    }
    return false;
  }
  explicit LocalVarRefChecker(Sema &SemaRef) : SemaRef(SemaRef) {}
};
} // namespace

OMPThreadPrivateDecl *
Sema::CheckOMPThreadPrivateDecl(SourceLocation Loc, ArrayRef<Expr *> VarList) {
  SmallVector<Expr *, 8> Vars;
  for (auto &RefExpr : VarList) {
    DeclRefExpr *DE = cast<DeclRefExpr>(RefExpr);
    VarDecl *VD = cast<VarDecl>(DE->getDecl());
    SourceLocation ILoc = DE->getExprLoc();

    // OpenMP [2.9.2, Restrictions, C/C++, p.10]
    //   A threadprivate variable must not have an incomplete type.
    if (RequireCompleteType(ILoc, VD->getType(),
                            diag::err_omp_threadprivate_incomplete_type)) {
      continue;
    }

    // OpenMP [2.9.2, Restrictions, C/C++, p.10]
    //   A threadprivate variable must not have a reference type.
    if (VD->getType()->isReferenceType()) {
      Diag(ILoc, diag::err_omp_ref_type_arg)
          << getOpenMPDirectiveName(OMPD_threadprivate) << VD->getType();
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }

    // Check if this is a TLS variable.
    if (VD->getTLSKind() != VarDecl::TLS_None ||
        VD->getStorageClass() == SC_Register) {
      Diag(ILoc, diag::err_omp_var_thread_local)
          << VD << ((VD->getTLSKind() != VarDecl::TLS_None) ? 0 : 1);
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }

    // Check if initial value of threadprivate variable reference variable with
    // local storage (it is not supported by runtime).
    if (auto Init = VD->getAnyInitializer()) {
      LocalVarRefChecker Checker(*this);
      if (Checker.Visit(Init))
        continue;
    }

    Vars.push_back(RefExpr);
    DSAStack->addDSA(VD, DE, OMPC_threadprivate);
    VD->addAttr(OMPThreadPrivateDeclAttr::CreateImplicit(
        Context, SourceRange(Loc, Loc)));
    if (auto *ML = Context.getASTMutationListener())
      ML->DeclarationMarkedOpenMPThreadPrivate(VD);
  }
  OMPThreadPrivateDecl *D = nullptr;
  if (!Vars.empty()) {
    D = OMPThreadPrivateDecl::Create(Context, getCurLexicalContext(), Loc,
                                     Vars);
    D->setAccess(AS_public);
  }
  return D;
}

static void ReportOriginalDSA(Sema &SemaRef, DSAStackTy *Stack,
                              const VarDecl *VD, DSAStackTy::DSAVarData DVar,
                              bool IsLoopIterVar = false) {
  if (DVar.RefExpr) {
    SemaRef.Diag(DVar.RefExpr->getExprLoc(), diag::note_omp_explicit_dsa)
        << getOpenMPClauseName(DVar.CKind);
    return;
  }
  enum {
    PDSA_StaticMemberShared,
    PDSA_StaticLocalVarShared,
    PDSA_LoopIterVarPrivate,
    PDSA_LoopIterVarLinear,
    PDSA_LoopIterVarLastprivate,
    PDSA_ConstVarShared,
    PDSA_GlobalVarShared,
    PDSA_TaskVarFirstprivate,
    PDSA_LocalVarPrivate,
    PDSA_Implicit
  } Reason = PDSA_Implicit;
  bool ReportHint = false;
  auto ReportLoc = VD->getLocation();
  if (IsLoopIterVar) {
    if (DVar.CKind == OMPC_private)
      Reason = PDSA_LoopIterVarPrivate;
    else if (DVar.CKind == OMPC_lastprivate)
      Reason = PDSA_LoopIterVarLastprivate;
    else
      Reason = PDSA_LoopIterVarLinear;
  } else if (DVar.DKind == OMPD_task && DVar.CKind == OMPC_firstprivate) {
    Reason = PDSA_TaskVarFirstprivate;
    ReportLoc = DVar.ImplicitDSALoc;
  } else if (VD->isStaticLocal())
    Reason = PDSA_StaticLocalVarShared;
  else if (VD->isStaticDataMember())
    Reason = PDSA_StaticMemberShared;
  else if (VD->isFileVarDecl())
    Reason = PDSA_GlobalVarShared;
  else if (VD->getType().isConstant(SemaRef.getASTContext()))
    Reason = PDSA_ConstVarShared;
  else if (VD->isLocalVarDecl() && DVar.CKind == OMPC_private) {
    ReportHint = true;
    Reason = PDSA_LocalVarPrivate;
  }
  if (Reason != PDSA_Implicit) {
    SemaRef.Diag(ReportLoc, diag::note_omp_predetermined_dsa)
        << Reason << ReportHint
        << getOpenMPDirectiveName(Stack->getCurrentDirective());
  } else if (DVar.ImplicitDSALoc.isValid()) {
    SemaRef.Diag(DVar.ImplicitDSALoc, diag::note_omp_implicit_dsa)
        << getOpenMPClauseName(DVar.CKind);
  }
}

namespace {
class DSAAttrChecker : public StmtVisitor<DSAAttrChecker, void> {
  DSAStackTy *Stack;
  Sema &SemaRef;
  bool ErrorFound;
  CapturedStmt *CS;
  llvm::SmallVector<Expr *, 8> ImplicitFirstprivate;
  llvm::DenseMap<VarDecl *, Expr *> VarsWithInheritedDSA;

public:
  void VisitDeclRefExpr(DeclRefExpr *E) {
    if (auto *VD = dyn_cast<VarDecl>(E->getDecl())) {
      // Skip internally declared variables.
      if (VD->isLocalVarDecl() && !CS->capturesVariable(VD))
        return;

      auto DVar = Stack->getTopDSA(VD, false);
      // Check if the variable has explicit DSA set and stop analysis if it so.
      if (DVar.RefExpr) return;

      auto ELoc = E->getExprLoc();
      auto DKind = Stack->getCurrentDirective();
      // The default(none) clause requires that each variable that is referenced
      // in the construct, and does not have a predetermined data-sharing
      // attribute, must have its data-sharing attribute explicitly determined
      // by being listed in a data-sharing attribute clause.
      if (DVar.CKind == OMPC_unknown && Stack->getDefaultDSA() == DSA_none &&
          isParallelOrTaskRegion(DKind) &&
          VarsWithInheritedDSA.count(VD) == 0) {
        VarsWithInheritedDSA[VD] = E;
        return;
      }

      // OpenMP [2.9.3.6, Restrictions, p.2]
      //  A list item that appears in a reduction clause of the innermost
      //  enclosing worksharing or parallel construct may not be accessed in an
      //  explicit task.
      DVar = Stack->hasInnermostDSA(VD, MatchesAnyClause(OMPC_reduction),
                                    [](OpenMPDirectiveKind K) -> bool {
                                      return isOpenMPParallelDirective(K) ||
                                             isOpenMPWorksharingDirective(K) ||
                                             isOpenMPTeamsDirective(K);
                                    },
                                    false);
      if (DKind == OMPD_task && DVar.CKind == OMPC_reduction) {
        ErrorFound = true;
        SemaRef.Diag(ELoc, diag::err_omp_reduction_in_task);
        ReportOriginalDSA(SemaRef, Stack, VD, DVar);
        return;
      }

      // Define implicit data-sharing attributes for task.
      DVar = Stack->getImplicitDSA(VD, false);
      if (DKind == OMPD_task && DVar.CKind != OMPC_shared)
        ImplicitFirstprivate.push_back(E);
    }
  }
  void VisitOMPExecutableDirective(OMPExecutableDirective *S) {
    for (auto *C : S->clauses()) {
      // Skip analysis of arguments of implicitly defined firstprivate clause
      // for task directives.
      if (C && (!isa<OMPFirstprivateClause>(C) || C->getLocStart().isValid()))
        for (auto *CC : C->children()) {
          if (CC)
            Visit(CC);
        }
    }
  }
  void VisitStmt(Stmt *S) {
    for (auto *C : S->children()) {
      if (C && !isa<OMPExecutableDirective>(C))
        Visit(C);
    }
  }

  bool isErrorFound() { return ErrorFound; }
  ArrayRef<Expr *> getImplicitFirstprivate() { return ImplicitFirstprivate; }
  llvm::DenseMap<VarDecl *, Expr *> &getVarsWithInheritedDSA() {
    return VarsWithInheritedDSA;
  }

  DSAAttrChecker(DSAStackTy *S, Sema &SemaRef, CapturedStmt *CS)
      : Stack(S), SemaRef(SemaRef), ErrorFound(false), CS(CS) {}
};
} // namespace

void Sema::ActOnOpenMPRegionStart(OpenMPDirectiveKind DKind, Scope *CurScope) {
  switch (DKind) {
  case OMPD_parallel: {
    QualType KmpInt32Ty = Context.getIntTypeForBitwidth(32, 1);
    QualType KmpInt32PtrTy = Context.getPointerType(KmpInt32Ty);
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(".global_tid.", KmpInt32PtrTy),
        std::make_pair(".bound_tid.", KmpInt32PtrTy),
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_simd: {
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_for: {
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_for_simd: {
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_sections: {
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_section: {
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_single: {
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_master: {
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_critical: {
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_parallel_for: {
    QualType KmpInt32Ty = Context.getIntTypeForBitwidth(32, 1);
    QualType KmpInt32PtrTy = Context.getPointerType(KmpInt32Ty);
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(".global_tid.", KmpInt32PtrTy),
        std::make_pair(".bound_tid.", KmpInt32PtrTy),
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_parallel_for_simd: {
    QualType KmpInt32Ty = Context.getIntTypeForBitwidth(32, 1);
    QualType KmpInt32PtrTy = Context.getPointerType(KmpInt32Ty);
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(".global_tid.", KmpInt32PtrTy),
        std::make_pair(".bound_tid.", KmpInt32PtrTy),
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_parallel_sections: {
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_task: {
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_ordered: {
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_atomic: {
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_target: {
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_teams: {
    QualType KmpInt32Ty = Context.getIntTypeForBitwidth(32, 1);
    QualType KmpInt32PtrTy = Context.getPointerType(KmpInt32Ty);
    Sema::CapturedParamNameType Params[] = {
        std::make_pair(".global_tid.", KmpInt32PtrTy),
        std::make_pair(".bound_tid.", KmpInt32PtrTy),
        std::make_pair(StringRef(), QualType()) // __context with shared vars
    };
    ActOnCapturedRegionStart(DSAStack->getConstructLoc(), CurScope, CR_OpenMP,
                             Params);
    break;
  }
  case OMPD_threadprivate:
  case OMPD_taskyield:
  case OMPD_barrier:
  case OMPD_taskwait:
  case OMPD_flush:
    llvm_unreachable("OpenMP Directive is not allowed");
  case OMPD_unknown:
    llvm_unreachable("Unknown OpenMP directive");
  }
}

static bool CheckNestingOfRegions(Sema &SemaRef, DSAStackTy *Stack,
                                  OpenMPDirectiveKind CurrentRegion,
                                  const DeclarationNameInfo &CurrentName,
                                  SourceLocation StartLoc) {
  // Allowed nesting of constructs
  // +------------------+-----------------+------------------------------------+
  // | Parent directive | Child directive | Closely (!), No-Closely(+), Both(*)|
  // +------------------+-----------------+------------------------------------+
  // | parallel         | parallel        | *                                  |
  // | parallel         | for             | *                                  |
  // | parallel         | for simd        | *                                  |
  // | parallel         | master          | *                                  |
  // | parallel         | critical        | *                                  |
  // | parallel         | simd            | *                                  |
  // | parallel         | sections        | *                                  |
  // | parallel         | section         | +                                  |
  // | parallel         | single          | *                                  |
  // | parallel         | parallel for    | *                                  |
  // | parallel         |parallel for simd| *                                  |
  // | parallel         |parallel sections| *                                  |
  // | parallel         | task            | *                                  |
  // | parallel         | taskyield       | *                                  |
  // | parallel         | barrier         | *                                  |
  // | parallel         | taskwait        | *                                  |
  // | parallel         | flush           | *                                  |
  // | parallel         | ordered         | +                                  |
  // | parallel         | atomic          | *                                  |
  // | parallel         | target          | *                                  |
  // | parallel         | teams           | +                                  |
  // +------------------+-----------------+------------------------------------+
  // | for              | parallel        | *                                  |
  // | for              | for             | +                                  |
  // | for              | for simd        | +                                  |
  // | for              | master          | +                                  |
  // | for              | critical        | *                                  |
  // | for              | simd            | *                                  |
  // | for              | sections        | +                                  |
  // | for              | section         | +                                  |
  // | for              | single          | +                                  |
  // | for              | parallel for    | *                                  |
  // | for              |parallel for simd| *                                  |
  // | for              |parallel sections| *                                  |
  // | for              | task            | *                                  |
  // | for              | taskyield       | *                                  |
  // | for              | barrier         | +                                  |
  // | for              | taskwait        | *                                  |
  // | for              | flush           | *                                  |
  // | for              | ordered         | * (if construct is ordered)        |
  // | for              | atomic          | *                                  |
  // | for              | target          | *                                  |
  // | for              | teams           | +                                  |
  // +------------------+-----------------+------------------------------------+
  // | master           | parallel        | *                                  |
  // | master           | for             | +                                  |
  // | master           | for simd        | +                                  |
  // | master           | master          | *                                  |
  // | master           | critical        | *                                  |
  // | master           | simd            | *                                  |
  // | master           | sections        | +                                  |
  // | master           | section         | +                                  |
  // | master           | single          | +                                  |
  // | master           | parallel for    | *                                  |
  // | master           |parallel for simd| *                                  |
  // | master           |parallel sections| *                                  |
  // | master           | task            | *                                  |
  // | master           | taskyield       | *                                  |
  // | master           | barrier         | +                                  |
  // | master           | taskwait        | *                                  |
  // | master           | flush           | *                                  |
  // | master           | ordered         | +                                  |
  // | master           | atomic          | *                                  |
  // | master           | target          | *                                  |
  // | master           | teams           | +                                  |
  // +------------------+-----------------+------------------------------------+
  // | critical         | parallel        | *                                  |
  // | critical         | for             | +                                  |
  // | critical         | for simd        | +                                  |
  // | critical         | master          | *                                  |
  // | critical         | critical        | * (should have different names)    |
  // | critical         | simd            | *                                  |
  // | critical         | sections        | +                                  |
  // | critical         | section         | +                                  |
  // | critical         | single          | +                                  |
  // | critical         | parallel for    | *                                  |
  // | critical         |parallel for simd| *                                  |
  // | critical         |parallel sections| *                                  |
  // | critical         | task            | *                                  |
  // | critical         | taskyield       | *                                  |
  // | critical         | barrier         | +                                  |
  // | critical         | taskwait        | *                                  |
  // | critical         | ordered         | +                                  |
  // | critical         | atomic          | *                                  |
  // | critical         | target          | *                                  |
  // | critical         | teams           | +                                  |
  // +------------------+-----------------+------------------------------------+
  // | simd             | parallel        |                                    |
  // | simd             | for             |                                    |
  // | simd             | for simd        |                                    |
  // | simd             | master          |                                    |
  // | simd             | critical        |                                    |
  // | simd             | simd            |                                    |
  // | simd             | sections        |                                    |
  // | simd             | section         |                                    |
  // | simd             | single          |                                    |
  // | simd             | parallel for    |                                    |
  // | simd             |parallel for simd|                                    |
  // | simd             |parallel sections|                                    |
  // | simd             | task            |                                    |
  // | simd             | taskyield       |                                    |
  // | simd             | barrier         |                                    |
  // | simd             | taskwait        |                                    |
  // | simd             | flush           |                                    |
  // | simd             | ordered         |                                    |
  // | simd             | atomic          |                                    |
  // | simd             | target          |                                    |
  // | simd             | teams           |                                    |
  // +------------------+-----------------+------------------------------------+
  // | for simd         | parallel        |                                    |
  // | for simd         | for             |                                    |
  // | for simd         | for simd        |                                    |
  // | for simd         | master          |                                    |
  // | for simd         | critical        |                                    |
  // | for simd         | simd            |                                    |
  // | for simd         | sections        |                                    |
  // | for simd         | section         |                                    |
  // | for simd         | single          |                                    |
  // | for simd         | parallel for    |                                    |
  // | for simd         |parallel for simd|                                    |
  // | for simd         |parallel sections|                                    |
  // | for simd         | task            |                                    |
  // | for simd         | taskyield       |                                    |
  // | for simd         | barrier         |                                    |
  // | for simd         | taskwait        |                                    |
  // | for simd         | flush           |                                    |
  // | for simd         | ordered         |                                    |
  // | for simd         | atomic          |                                    |
  // | for simd         | target          |                                    |
  // | for simd         | teams           |                                    |
  // +------------------+-----------------+------------------------------------+
  // | parallel for simd| parallel        |                                    |
  // | parallel for simd| for             |                                    |
  // | parallel for simd| for simd        |                                    |
  // | parallel for simd| master          |                                    |
  // | parallel for simd| critical        |                                    |
  // | parallel for simd| simd            |                                    |
  // | parallel for simd| sections        |                                    |
  // | parallel for simd| section         |                                    |
  // | parallel for simd| single          |                                    |
  // | parallel for simd| parallel for    |                                    |
  // | parallel for simd|parallel for simd|                                    |
  // | parallel for simd|parallel sections|                                    |
  // | parallel for simd| task            |                                    |
  // | parallel for simd| taskyield       |                                    |
  // | parallel for simd| barrier         |                                    |
  // | parallel for simd| taskwait        |                                    |
  // | parallel for simd| flush           |                                    |
  // | parallel for simd| ordered         |                                    |
  // | parallel for simd| atomic          |                                    |
  // | parallel for simd| target          |                                    |
  // | parallel for simd| teams           |                                    |
  // +------------------+-----------------+------------------------------------+
  // | sections         | parallel        | *                                  |
  // | sections         | for             | +                                  |
  // | sections         | for simd        | +                                  |
  // | sections         | master          | +                                  |
  // | sections         | critical        | *                                  |
  // | sections         | simd            | *                                  |
  // | sections         | sections        | +                                  |
  // | sections         | section         | *                                  |
  // | sections         | single          | +                                  |
  // | sections         | parallel for    | *                                  |
  // | sections         |parallel for simd| *                                  |
  // | sections         |parallel sections| *                                  |
  // | sections         | task            | *                                  |
  // | sections         | taskyield       | *                                  |
  // | sections         | barrier         | +                                  |
  // | sections         | taskwait        | *                                  |
  // | sections         | flush           | *                                  |
  // | sections         | ordered         | +                                  |
  // | sections         | atomic          | *                                  |
  // | sections         | target          | *                                  |
  // | sections         | teams           | +                                  |
  // +------------------+-----------------+------------------------------------+
  // | section          | parallel        | *                                  |
  // | section          | for             | +                                  |
  // | section          | for simd        | +                                  |
  // | section          | master          | +                                  |
  // | section          | critical        | *                                  |
  // | section          | simd            | *                                  |
  // | section          | sections        | +                                  |
  // | section          | section         | +                                  |
  // | section          | single          | +                                  |
  // | section          | parallel for    | *                                  |
  // | section          |parallel for simd| *                                  |
  // | section          |parallel sections| *                                  |
  // | section          | task            | *                                  |
  // | section          | taskyield       | *                                  |
  // | section          | barrier         | +                                  |
  // | section          | taskwait        | *                                  |
  // | section          | flush           | *                                  |
  // | section          | ordered         | +                                  |
  // | section          | atomic          | *                                  |
  // | section          | target          | *                                  |
  // | section          | teams           | +                                  |
  // +------------------+-----------------+------------------------------------+
  // | single           | parallel        | *                                  |
  // | single           | for             | +                                  |
  // | single           | for simd        | +                                  |
  // | single           | master          | +                                  |
  // | single           | critical        | *                                  |
  // | single           | simd            | *                                  |
  // | single           | sections        | +                                  |
  // | single           | section         | +                                  |
  // | single           | single          | +                                  |
  // | single           | parallel for    | *                                  |
  // | single           |parallel for simd| *                                  |
  // | single           |parallel sections| *                                  |
  // | single           | task            | *                                  |
  // | single           | taskyield       | *                                  |
  // | single           | barrier         | +                                  |
  // | single           | taskwait        | *                                  |
  // | single           | flush           | *                                  |
  // | single           | ordered         | +                                  |
  // | single           | atomic          | *                                  |
  // | single           | target          | *                                  |
  // | single           | teams           | +                                  |
  // +------------------+-----------------+------------------------------------+
  // | parallel for     | parallel        | *                                  |
  // | parallel for     | for             | +                                  |
  // | parallel for     | for simd        | +                                  |
  // | parallel for     | master          | +                                  |
  // | parallel for     | critical        | *                                  |
  // | parallel for     | simd            | *                                  |
  // | parallel for     | sections        | +                                  |
  // | parallel for     | section         | +                                  |
  // | parallel for     | single          | +                                  |
  // | parallel for     | parallel for    | *                                  |
  // | parallel for     |parallel for simd| *                                  |
  // | parallel for     |parallel sections| *                                  |
  // | parallel for     | task            | *                                  |
  // | parallel for     | taskyield       | *                                  |
  // | parallel for     | barrier         | +                                  |
  // | parallel for     | taskwait        | *                                  |
  // | parallel for     | flush           | *                                  |
  // | parallel for     | ordered         | * (if construct is ordered)        |
  // | parallel for     | atomic          | *                                  |
  // | parallel for     | target          | *                                  |
  // | parallel for     | teams           | +                                  |
  // +------------------+-----------------+------------------------------------+
  // | parallel sections| parallel        | *                                  |
  // | parallel sections| for             | +                                  |
  // | parallel sections| for simd        | +                                  |
  // | parallel sections| master          | +                                  |
  // | parallel sections| critical        | +                                  |
  // | parallel sections| simd            | *                                  |
  // | parallel sections| sections        | +                                  |
  // | parallel sections| section         | *                                  |
  // | parallel sections| single          | +                                  |
  // | parallel sections| parallel for    | *                                  |
  // | parallel sections|parallel for simd| *                                  |
  // | parallel sections|parallel sections| *                                  |
  // | parallel sections| task            | *                                  |
  // | parallel sections| taskyield       | *                                  |
  // | parallel sections| barrier         | +                                  |
  // | parallel sections| taskwait        | *                                  |
  // | parallel sections| flush           | *                                  |
  // | parallel sections| ordered         | +                                  |
  // | parallel sections| atomic          | *                                  |
  // | parallel sections| target          | *                                  |
  // | parallel sections| teams           | +                                  |
  // +------------------+-----------------+------------------------------------+
  // | task             | parallel        | *                                  |
  // | task             | for             | +                                  |
  // | task             | for simd        | +                                  |
  // | task             | master          | +                                  |
  // | task             | critical        | *                                  |
  // | task             | simd            | *                                  |
  // | task             | sections        | +                                  |
  // | task             | section         | +                                  |
  // | task             | single          | +                                  |
  // | task             | parallel for    | *                                  |
  // | task             |parallel for simd| *                                  |
  // | task             |parallel sections| *                                  |
  // | task             | task            | *                                  |
  // | task             | taskyield       | *                                  |
  // | task             | barrier         | +                                  |
  // | task             | taskwait        | *                                  |
  // | task             | flush           | *                                  |
  // | task             | ordered         | +                                  |
  // | task             | atomic          | *                                  |
  // | task             | target          | *                                  |
  // | task             | teams           | +                                  |
  // +------------------+-----------------+------------------------------------+
  // | ordered          | parallel        | *                                  |
  // | ordered          | for             | +                                  |
  // | ordered          | for simd        | +                                  |
  // | ordered          | master          | *                                  |
  // | ordered          | critical        | *                                  |
  // | ordered          | simd            | *                                  |
  // | ordered          | sections        | +                                  |
  // | ordered          | section         | +                                  |
  // | ordered          | single          | +                                  |
  // | ordered          | parallel for    | *                                  |
  // | ordered          |parallel for simd| *                                  |
  // | ordered          |parallel sections| *                                  |
  // | ordered          | task            | *                                  |
  // | ordered          | taskyield       | *                                  |
  // | ordered          | barrier         | +                                  |
  // | ordered          | taskwait        | *                                  |
  // | ordered          | flush           | *                                  |
  // | ordered          | ordered         | +                                  |
  // | ordered          | atomic          | *                                  |
  // | ordered          | target          | *                                  |
  // | ordered          | teams           | +                                  |
  // +------------------+-----------------+------------------------------------+
  // | atomic           | parallel        |                                    |
  // | atomic           | for             |                                    |
  // | atomic           | for simd        |                                    |
  // | atomic           | master          |                                    |
  // | atomic           | critical        |                                    |
  // | atomic           | simd            |                                    |
  // | atomic           | sections        |                                    |
  // | atomic           | section         |                                    |
  // | atomic           | single          |                                    |
  // | atomic           | parallel for    |                                    |
  // | atomic           |parallel for simd|                                    |
  // | atomic           |parallel sections|                                    |
  // | atomic           | task            |                                    |
  // | atomic           | taskyield       |                                    |
  // | atomic           | barrier         |                                    |
  // | atomic           | taskwait        |                                    |
  // | atomic           | flush           |                                    |
  // | atomic           | ordered         |                                    |
  // | atomic           | atomic          |                                    |
  // | atomic           | target          |                                    |
  // | atomic           | teams           |                                    |
  // +------------------+-----------------+------------------------------------+
  // | target           | parallel        | *                                  |
  // | target           | for             | *                                  |
  // | target           | for simd        | *                                  |
  // | target           | master          | *                                  |
  // | target           | critical        | *                                  |
  // | target           | simd            | *                                  |
  // | target           | sections        | *                                  |
  // | target           | section         | *                                  |
  // | target           | single          | *                                  |
  // | target           | parallel for    | *                                  |
  // | target           |parallel for simd| *                                  |
  // | target           |parallel sections| *                                  |
  // | target           | task            | *                                  |
  // | target           | taskyield       | *                                  |
  // | target           | barrier         | *                                  |
  // | target           | taskwait        | *                                  |
  // | target           | flush           | *                                  |
  // | target           | ordered         | *                                  |
  // | target           | atomic          | *                                  |
  // | target           | target          | *                                  |
  // | target           | teams           | *                                  |
  // +------------------+-----------------+------------------------------------+
  // | teams            | parallel        | *                                  |
  // | teams            | for             | +                                  |
  // | teams            | for simd        | +                                  |
  // | teams            | master          | +                                  |
  // | teams            | critical        | +                                  |
  // | teams            | simd            | +                                  |
  // | teams            | sections        | +                                  |
  // | teams            | section         | +                                  |
  // | teams            | single          | +                                  |
  // | teams            | parallel for    | *                                  |
  // | teams            |parallel for simd| *                                  |
  // | teams            |parallel sections| *                                  |
  // | teams            | task            | +                                  |
  // | teams            | taskyield       | +                                  |
  // | teams            | barrier         | +                                  |
  // | teams            | taskwait        | +                                  |
  // | teams            | flush           | +                                  |
  // | teams            | ordered         | +                                  |
  // | teams            | atomic          | +                                  |
  // | teams            | target          | +                                  |
  // | teams            | teams           | +                                  |
  // +------------------+-----------------+------------------------------------+
  if (Stack->getCurScope()) {
    auto ParentRegion = Stack->getParentDirective();
    bool NestingProhibited = false;
    bool CloseNesting = true;
    enum {
      NoRecommend,
      ShouldBeInParallelRegion,
      ShouldBeInOrderedRegion,
      ShouldBeInTargetRegion
    } Recommend = NoRecommend;
    if (isOpenMPSimdDirective(ParentRegion)) {
      // OpenMP [2.16, Nesting of Regions]
      // OpenMP constructs may not be nested inside a simd region.
      SemaRef.Diag(StartLoc, diag::err_omp_prohibited_region_simd);
      return true;
    }
    if (ParentRegion == OMPD_atomic) {
      // OpenMP [2.16, Nesting of Regions]
      // OpenMP constructs may not be nested inside an atomic region.
      SemaRef.Diag(StartLoc, diag::err_omp_prohibited_region_atomic);
      return true;
    }
    if (CurrentRegion == OMPD_section) {
      // OpenMP [2.7.2, sections Construct, Restrictions]
      // Orphaned section directives are prohibited. That is, the section
      // directives must appear within the sections construct and must not be
      // encountered elsewhere in the sections region.
      if (ParentRegion != OMPD_sections &&
          ParentRegion != OMPD_parallel_sections) {
        SemaRef.Diag(StartLoc, diag::err_omp_orphaned_section_directive)
            << (ParentRegion != OMPD_unknown)
            << getOpenMPDirectiveName(ParentRegion);
        return true;
      }
      return false;
    }
    // Allow some constructs to be orphaned (they could be used in functions,
    // called from OpenMP regions with the required preconditions).
    if (ParentRegion == OMPD_unknown)
      return false;
    if (CurrentRegion == OMPD_master) {
      // OpenMP [2.16, Nesting of Regions]
      // A master region may not be closely nested inside a worksharing,
      // atomic, or explicit task region.
      NestingProhibited = isOpenMPWorksharingDirective(ParentRegion) ||
                          ParentRegion == OMPD_task;
    } else if (CurrentRegion == OMPD_critical && CurrentName.getName()) {
      // OpenMP [2.16, Nesting of Regions]
      // A critical region may not be nested (closely or otherwise) inside a
      // critical region with the same name. Note that this restriction is not
      // sufficient to prevent deadlock.
      SourceLocation PreviousCriticalLoc;
      bool DeadLock =
          Stack->hasDirective([CurrentName, &PreviousCriticalLoc](
                                  OpenMPDirectiveKind K,
                                  const DeclarationNameInfo &DNI,
                                  SourceLocation Loc)
                                  ->bool {
                                if (K == OMPD_critical &&
                                    DNI.getName() == CurrentName.getName()) {
                                  PreviousCriticalLoc = Loc;
                                  return true;
                                } else
                                  return false;
                              },
                              false /* skip top directive */);
      if (DeadLock) {
        SemaRef.Diag(StartLoc,
                     diag::err_omp_prohibited_region_critical_same_name)
            << CurrentName.getName();
        if (PreviousCriticalLoc.isValid())
          SemaRef.Diag(PreviousCriticalLoc,
                       diag::note_omp_previous_critical_region);
        return true;
      }
    } else if (CurrentRegion == OMPD_barrier) {
      // OpenMP [2.16, Nesting of Regions]
      // A barrier region may not be closely nested inside a worksharing,
      // explicit task, critical, ordered, atomic, or master region.
      NestingProhibited =
          isOpenMPWorksharingDirective(ParentRegion) ||
          ParentRegion == OMPD_task || ParentRegion == OMPD_master ||
          ParentRegion == OMPD_critical || ParentRegion == OMPD_ordered;
    } else if (isOpenMPWorksharingDirective(CurrentRegion) &&
               !isOpenMPParallelDirective(CurrentRegion)) {
      // OpenMP [2.16, Nesting of Regions]
      // A worksharing region may not be closely nested inside a worksharing,
      // explicit task, critical, ordered, atomic, or master region.
      NestingProhibited =
          isOpenMPWorksharingDirective(ParentRegion) ||
          ParentRegion == OMPD_task || ParentRegion == OMPD_master ||
          ParentRegion == OMPD_critical || ParentRegion == OMPD_ordered;
      Recommend = ShouldBeInParallelRegion;
    } else if (CurrentRegion == OMPD_ordered) {
      // OpenMP [2.16, Nesting of Regions]
      // An ordered region may not be closely nested inside a critical,
      // atomic, or explicit task region.
      // An ordered region must be closely nested inside a loop region (or
      // parallel loop region) with an ordered clause.
      NestingProhibited = ParentRegion == OMPD_critical ||
                          ParentRegion == OMPD_task ||
                          !Stack->isParentOrderedRegion();
      Recommend = ShouldBeInOrderedRegion;
    } else if (isOpenMPTeamsDirective(CurrentRegion)) {
      // OpenMP [2.16, Nesting of Regions]
      // If specified, a teams construct must be contained within a target
      // construct.
      NestingProhibited = ParentRegion != OMPD_target;
      Recommend = ShouldBeInTargetRegion;
      Stack->setParentTeamsRegionLoc(Stack->getConstructLoc());
    }
    if (!NestingProhibited && isOpenMPTeamsDirective(ParentRegion)) {
      // OpenMP [2.16, Nesting of Regions]
      // distribute, parallel, parallel sections, parallel workshare, and the
      // parallel loop and parallel loop SIMD constructs are the only OpenMP
      // constructs that can be closely nested in the teams region.
      // TODO: add distribute directive.
      NestingProhibited = !isOpenMPParallelDirective(CurrentRegion);
      Recommend = ShouldBeInParallelRegion;
    }
    if (NestingProhibited) {
      SemaRef.Diag(StartLoc, diag::err_omp_prohibited_region)
          << CloseNesting << getOpenMPDirectiveName(ParentRegion) << Recommend
          << getOpenMPDirectiveName(CurrentRegion);
      return true;
    }
  }
  return false;
}

StmtResult Sema::ActOnOpenMPExecutableDirective(OpenMPDirectiveKind Kind,
                                                const DeclarationNameInfo &DirName,
                                                ArrayRef<OMPClause *> Clauses,
                                                Stmt *AStmt,
                                                SourceLocation StartLoc,
                                                SourceLocation EndLoc) {
  StmtResult Res = StmtError();
  if (CheckNestingOfRegions(*this, DSAStack, Kind, DirName, StartLoc))
    return StmtError();

  llvm::SmallVector<OMPClause *, 8> ClausesWithImplicit;
  llvm::DenseMap<VarDecl *, Expr *> VarsWithInheritedDSA;
  bool ErrorFound = false;
  ClausesWithImplicit.append(Clauses.begin(), Clauses.end());
  if (AStmt) {
    assert(isa<CapturedStmt>(AStmt) && "Captured statement expected");

    // Check default data sharing attributes for referenced variables.
    DSAAttrChecker DSAChecker(DSAStack, *this, cast<CapturedStmt>(AStmt));
    DSAChecker.Visit(cast<CapturedStmt>(AStmt)->getCapturedStmt());
    if (DSAChecker.isErrorFound())
      return StmtError();
    // Generate list of implicitly defined firstprivate variables.
    VarsWithInheritedDSA = DSAChecker.getVarsWithInheritedDSA();

    if (!DSAChecker.getImplicitFirstprivate().empty()) {
      if (OMPClause *Implicit = ActOnOpenMPFirstprivateClause(
              DSAChecker.getImplicitFirstprivate(), SourceLocation(),
              SourceLocation(), SourceLocation())) {
        ClausesWithImplicit.push_back(Implicit);
        ErrorFound = cast<OMPFirstprivateClause>(Implicit)->varlist_size() !=
                     DSAChecker.getImplicitFirstprivate().size();
      } else
        ErrorFound = true;
    }
  }

  switch (Kind) {
  case OMPD_parallel:
    Res = ActOnOpenMPParallelDirective(ClausesWithImplicit, AStmt, StartLoc,
                                       EndLoc);
    break;
  case OMPD_simd:
    Res = ActOnOpenMPSimdDirective(ClausesWithImplicit, AStmt, StartLoc, EndLoc,
                                   VarsWithInheritedDSA);
    break;
  case OMPD_for:
    Res = ActOnOpenMPForDirective(ClausesWithImplicit, AStmt, StartLoc, EndLoc,
                                  VarsWithInheritedDSA);
    break;
  case OMPD_for_simd:
    Res = ActOnOpenMPForSimdDirective(ClausesWithImplicit, AStmt, StartLoc,
                                      EndLoc, VarsWithInheritedDSA);
    break;
  case OMPD_sections:
    Res = ActOnOpenMPSectionsDirective(ClausesWithImplicit, AStmt, StartLoc,
                                       EndLoc);
    break;
  case OMPD_section:
    assert(ClausesWithImplicit.empty() &&
           "No clauses are allowed for 'omp section' directive");
    Res = ActOnOpenMPSectionDirective(AStmt, StartLoc, EndLoc);
    break;
  case OMPD_single:
    Res = ActOnOpenMPSingleDirective(ClausesWithImplicit, AStmt, StartLoc,
                                     EndLoc);
    break;
  case OMPD_master:
    assert(ClausesWithImplicit.empty() &&
           "No clauses are allowed for 'omp master' directive");
    Res = ActOnOpenMPMasterDirective(AStmt, StartLoc, EndLoc);
    break;
  case OMPD_critical:
    assert(ClausesWithImplicit.empty() &&
           "No clauses are allowed for 'omp critical' directive");
    Res = ActOnOpenMPCriticalDirective(DirName, AStmt, StartLoc, EndLoc);
    break;
  case OMPD_parallel_for:
    Res = ActOnOpenMPParallelForDirective(ClausesWithImplicit, AStmt, StartLoc,
                                          EndLoc, VarsWithInheritedDSA);
    break;
  case OMPD_parallel_for_simd:
    Res = ActOnOpenMPParallelForSimdDirective(
        ClausesWithImplicit, AStmt, StartLoc, EndLoc, VarsWithInheritedDSA);
    break;
  case OMPD_parallel_sections:
    Res = ActOnOpenMPParallelSectionsDirective(ClausesWithImplicit, AStmt,
                                               StartLoc, EndLoc);
    break;
  case OMPD_task:
    Res =
        ActOnOpenMPTaskDirective(ClausesWithImplicit, AStmt, StartLoc, EndLoc);
    break;
  case OMPD_taskyield:
    assert(ClausesWithImplicit.empty() &&
           "No clauses are allowed for 'omp taskyield' directive");
    assert(AStmt == nullptr &&
           "No associated statement allowed for 'omp taskyield' directive");
    Res = ActOnOpenMPTaskyieldDirective(StartLoc, EndLoc);
    break;
  case OMPD_barrier:
    assert(ClausesWithImplicit.empty() &&
           "No clauses are allowed for 'omp barrier' directive");
    assert(AStmt == nullptr &&
           "No associated statement allowed for 'omp barrier' directive");
    Res = ActOnOpenMPBarrierDirective(StartLoc, EndLoc);
    break;
  case OMPD_taskwait:
    assert(ClausesWithImplicit.empty() &&
           "No clauses are allowed for 'omp taskwait' directive");
    assert(AStmt == nullptr &&
           "No associated statement allowed for 'omp taskwait' directive");
    Res = ActOnOpenMPTaskwaitDirective(StartLoc, EndLoc);
    break;
  case OMPD_flush:
    assert(AStmt == nullptr &&
           "No associated statement allowed for 'omp flush' directive");
    Res = ActOnOpenMPFlushDirective(ClausesWithImplicit, StartLoc, EndLoc);
    break;
  case OMPD_ordered:
    assert(ClausesWithImplicit.empty() &&
           "No clauses are allowed for 'omp ordered' directive");
    Res = ActOnOpenMPOrderedDirective(AStmt, StartLoc, EndLoc);
    break;
  case OMPD_atomic:
    Res = ActOnOpenMPAtomicDirective(ClausesWithImplicit, AStmt, StartLoc,
                                     EndLoc);
    break;
  case OMPD_teams:
    Res =
        ActOnOpenMPTeamsDirective(ClausesWithImplicit, AStmt, StartLoc, EndLoc);
    break;
  case OMPD_target:
    Res = ActOnOpenMPTargetDirective(ClausesWithImplicit, AStmt, StartLoc,
                                     EndLoc);
    break;
  case OMPD_threadprivate:
    llvm_unreachable("OpenMP Directive is not allowed");
  case OMPD_unknown:
    llvm_unreachable("Unknown OpenMP directive");
  }

  for (auto P : VarsWithInheritedDSA) {
    Diag(P.second->getExprLoc(), diag::err_omp_no_dsa_for_variable)
        << P.first << P.second->getSourceRange();
  }
  if (!VarsWithInheritedDSA.empty())
    return StmtError();

  if (ErrorFound)
    return StmtError();
  return Res;
}

StmtResult Sema::ActOnOpenMPParallelDirective(ArrayRef<OMPClause *> Clauses,
                                              Stmt *AStmt,
                                              SourceLocation StartLoc,
                                              SourceLocation EndLoc) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");
  CapturedStmt *CS = cast<CapturedStmt>(AStmt);
  // 1.2.2 OpenMP Language Terminology
  // Structured block - An executable statement with a single entry at the
  // top and a single exit at the bottom.
  // The point of exit cannot be a branch out of the structured block.
  // longjmp() and throw() must not violate the entry/exit criteria.
  CS->getCapturedDecl()->setNothrow();

  getCurFunction()->setHasBranchProtectedScope();

  return OMPParallelDirective::Create(Context, StartLoc, EndLoc, Clauses,
                                      AStmt);
}

namespace {
/// \brief Helper class for checking canonical form of the OpenMP loops and
/// extracting iteration space of each loop in the loop nest, that will be used
/// for IR generation.
class OpenMPIterationSpaceChecker {
  /// \brief Reference to Sema.
  Sema &SemaRef;
  /// \brief A location for diagnostics (when there is no some better location).
  SourceLocation DefaultLoc;
  /// \brief A location for diagnostics (when increment is not compatible).
  SourceLocation ConditionLoc;
  /// \brief A source location for referring to loop init later.
  SourceRange InitSrcRange;
  /// \brief A source location for referring to condition later.
  SourceRange ConditionSrcRange;
  /// \brief A source location for referring to increment later.
  SourceRange IncrementSrcRange;
  /// \brief Loop variable.
  VarDecl *Var;
  /// \brief Reference to loop variable.
  DeclRefExpr *VarRef;
  /// \brief Lower bound (initializer for the var).
  Expr *LB;
  /// \brief Upper bound.
  Expr *UB;
  /// \brief Loop step (increment).
  Expr *Step;
  /// \brief This flag is true when condition is one of:
  ///   Var <  UB
  ///   Var <= UB
  ///   UB  >  Var
  ///   UB  >= Var
  bool TestIsLessOp;
  /// \brief This flag is true when condition is strict ( < or > ).
  bool TestIsStrictOp;
  /// \brief This flag is true when step is subtracted on each iteration.
  bool SubtractStep;

public:
  OpenMPIterationSpaceChecker(Sema &SemaRef, SourceLocation DefaultLoc)
      : SemaRef(SemaRef), DefaultLoc(DefaultLoc), ConditionLoc(DefaultLoc),
        InitSrcRange(SourceRange()), ConditionSrcRange(SourceRange()),
        IncrementSrcRange(SourceRange()), Var(nullptr), VarRef(nullptr),
        LB(nullptr), UB(nullptr), Step(nullptr), TestIsLessOp(false),
        TestIsStrictOp(false), SubtractStep(false) {}
  /// \brief Check init-expr for canonical loop form and save loop counter
  /// variable - #Var and its initialization value - #LB.
  bool CheckInit(Stmt *S);
  /// \brief Check test-expr for canonical form, save upper-bound (#UB), flags
  /// for less/greater and for strict/non-strict comparison.
  bool CheckCond(Expr *S);
  /// \brief Check incr-expr for canonical loop form and return true if it
  /// does not conform, otherwise save loop step (#Step).
  bool CheckInc(Expr *S);
  /// \brief Return the loop counter variable.
  VarDecl *GetLoopVar() const { return Var; }
  /// \brief Return the reference expression to loop counter variable.
  DeclRefExpr *GetLoopVarRefExpr() const { return VarRef; }
  /// \brief Source range of the loop init.
  SourceRange GetInitSrcRange() const { return InitSrcRange; }
  /// \brief Source range of the loop condition.
  SourceRange GetConditionSrcRange() const { return ConditionSrcRange; }
  /// \brief Source range of the loop increment.
  SourceRange GetIncrementSrcRange() const { return IncrementSrcRange; }
  /// \brief True if the step should be subtracted.
  bool ShouldSubtractStep() const { return SubtractStep; }
  /// \brief Build the expression to calculate the number of iterations.
  Expr *BuildNumIterations(Scope *S, const bool LimitedType) const;
  /// \brief Build reference expression to the counter be used for codegen.
  Expr *BuildCounterVar() const;
  /// \brief Build initization of the counter be used for codegen.
  Expr *BuildCounterInit() const;
  /// \brief Build step of the counter be used for codegen.
  Expr *BuildCounterStep() const;
  /// \brief Return true if any expression is dependent.
  bool Dependent() const;

private:
  /// \brief Check the right-hand side of an assignment in the increment
  /// expression.
  bool CheckIncRHS(Expr *RHS);
  /// \brief Helper to set loop counter variable and its initializer.
  bool SetVarAndLB(VarDecl *NewVar, DeclRefExpr *NewVarRefExpr, Expr *NewLB);
  /// \brief Helper to set upper bound.
  bool SetUB(Expr *NewUB, bool LessOp, bool StrictOp, const SourceRange &SR,
             const SourceLocation &SL);
  /// \brief Helper to set loop increment.
  bool SetStep(Expr *NewStep, bool Subtract);
};

bool OpenMPIterationSpaceChecker::Dependent() const {
  if (!Var) {
    assert(!LB && !UB && !Step);
    return false;
  }
  return Var->getType()->isDependentType() || (LB && LB->isValueDependent()) ||
         (UB && UB->isValueDependent()) || (Step && Step->isValueDependent());
}

bool OpenMPIterationSpaceChecker::SetVarAndLB(VarDecl *NewVar,
                                              DeclRefExpr *NewVarRefExpr,
                                              Expr *NewLB) {
  // State consistency checking to ensure correct usage.
  assert(Var == nullptr && LB == nullptr && VarRef == nullptr &&
         UB == nullptr && Step == nullptr && !TestIsLessOp && !TestIsStrictOp);
  if (!NewVar || !NewLB)
    return true;
  Var = NewVar;
  VarRef = NewVarRefExpr;
  LB = NewLB;
  return false;
}

bool OpenMPIterationSpaceChecker::SetUB(Expr *NewUB, bool LessOp, bool StrictOp,
                                        const SourceRange &SR,
                                        const SourceLocation &SL) {
  // State consistency checking to ensure correct usage.
  assert(Var != nullptr && LB != nullptr && UB == nullptr && Step == nullptr &&
         !TestIsLessOp && !TestIsStrictOp);
  if (!NewUB)
    return true;
  UB = NewUB;
  TestIsLessOp = LessOp;
  TestIsStrictOp = StrictOp;
  ConditionSrcRange = SR;
  ConditionLoc = SL;
  return false;
}

bool OpenMPIterationSpaceChecker::SetStep(Expr *NewStep, bool Subtract) {
  // State consistency checking to ensure correct usage.
  assert(Var != nullptr && LB != nullptr && Step == nullptr);
  if (!NewStep)
    return true;
  if (!NewStep->isValueDependent()) {
    // Check that the step is integer expression.
    SourceLocation StepLoc = NewStep->getLocStart();
    ExprResult Val =
        SemaRef.PerformOpenMPImplicitIntegerConversion(StepLoc, NewStep);
    if (Val.isInvalid())
      return true;
    NewStep = Val.get();

    // OpenMP [2.6, Canonical Loop Form, Restrictions]
    //  If test-expr is of form var relational-op b and relational-op is < or
    //  <= then incr-expr must cause var to increase on each iteration of the
    //  loop. If test-expr is of form var relational-op b and relational-op is
    //  > or >= then incr-expr must cause var to decrease on each iteration of
    //  the loop.
    //  If test-expr is of form b relational-op var and relational-op is < or
    //  <= then incr-expr must cause var to decrease on each iteration of the
    //  loop. If test-expr is of form b relational-op var and relational-op is
    //  > or >= then incr-expr must cause var to increase on each iteration of
    //  the loop.
    llvm::APSInt Result;
    bool IsConstant = NewStep->isIntegerConstantExpr(Result, SemaRef.Context);
    bool IsUnsigned = !NewStep->getType()->hasSignedIntegerRepresentation();
    bool IsConstNeg =
        IsConstant && Result.isSigned() && (Subtract != Result.isNegative());
    bool IsConstPos =
        IsConstant && Result.isSigned() && (Subtract == Result.isNegative());
    bool IsConstZero = IsConstant && !Result.getBoolValue();
    if (UB && (IsConstZero ||
               (TestIsLessOp ? (IsConstNeg || (IsUnsigned && Subtract))
                             : (IsConstPos || (IsUnsigned && !Subtract))))) {
      SemaRef.Diag(NewStep->getExprLoc(),
                   diag::err_omp_loop_incr_not_compatible)
          << Var << TestIsLessOp << NewStep->getSourceRange();
      SemaRef.Diag(ConditionLoc,
                   diag::note_omp_loop_cond_requres_compatible_incr)
          << TestIsLessOp << ConditionSrcRange;
      return true;
    }
    if (TestIsLessOp == Subtract) {
      NewStep = SemaRef.CreateBuiltinUnaryOp(NewStep->getExprLoc(), UO_Minus,
                                             NewStep).get();
      Subtract = !Subtract;
    }
  }

  Step = NewStep;
  SubtractStep = Subtract;
  return false;
}

bool OpenMPIterationSpaceChecker::CheckInit(Stmt *S) {
  // Check init-expr for canonical loop form and save loop counter
  // variable - #Var and its initialization value - #LB.
  // OpenMP [2.6] Canonical loop form. init-expr may be one of the following:
  //   var = lb
  //   integer-type var = lb
  //   random-access-iterator-type var = lb
  //   pointer-type var = lb
  //
  if (!S) {
    SemaRef.Diag(DefaultLoc, diag::err_omp_loop_not_canonical_init);
    return true;
  }
  InitSrcRange = S->getSourceRange();
  if (Expr *E = dyn_cast<Expr>(S))
    S = E->IgnoreParens();
  if (auto BO = dyn_cast<BinaryOperator>(S)) {
    if (BO->getOpcode() == BO_Assign)
      if (auto DRE = dyn_cast<DeclRefExpr>(BO->getLHS()->IgnoreParens()))
        return SetVarAndLB(dyn_cast<VarDecl>(DRE->getDecl()), DRE,
                           BO->getRHS());
  } else if (auto DS = dyn_cast<DeclStmt>(S)) {
    if (DS->isSingleDecl()) {
      if (auto Var = dyn_cast_or_null<VarDecl>(DS->getSingleDecl())) {
        if (Var->hasInit()) {
          // Accept non-canonical init form here but emit ext. warning.
          if (Var->getInitStyle() != VarDecl::CInit)
            SemaRef.Diag(S->getLocStart(),
                         diag::ext_omp_loop_not_canonical_init)
                << S->getSourceRange();
          return SetVarAndLB(Var, nullptr, Var->getInit());
        }
      }
    }
  } else if (auto CE = dyn_cast<CXXOperatorCallExpr>(S))
    if (CE->getOperator() == OO_Equal)
      if (auto DRE = dyn_cast<DeclRefExpr>(CE->getArg(0)))
        return SetVarAndLB(dyn_cast<VarDecl>(DRE->getDecl()), DRE,
                           CE->getArg(1));

  SemaRef.Diag(S->getLocStart(), diag::err_omp_loop_not_canonical_init)
      << S->getSourceRange();
  return true;
}

/// \brief Ignore parenthesizes, implicit casts, copy constructor and return the
/// variable (which may be the loop variable) if possible.
static const VarDecl *GetInitVarDecl(const Expr *E) {
  if (!E)
    return nullptr;
  E = E->IgnoreParenImpCasts();
  if (auto *CE = dyn_cast_or_null<CXXConstructExpr>(E))
    if (const CXXConstructorDecl *Ctor = CE->getConstructor())
      if (Ctor->isCopyConstructor() && CE->getNumArgs() == 1 &&
          CE->getArg(0) != nullptr)
        E = CE->getArg(0)->IgnoreParenImpCasts();
  auto DRE = dyn_cast_or_null<DeclRefExpr>(E);
  if (!DRE)
    return nullptr;
  return dyn_cast<VarDecl>(DRE->getDecl());
}

bool OpenMPIterationSpaceChecker::CheckCond(Expr *S) {
  // Check test-expr for canonical form, save upper-bound UB, flags for
  // less/greater and for strict/non-strict comparison.
  // OpenMP [2.6] Canonical loop form. Test-expr may be one of the following:
  //   var relational-op b
  //   b relational-op var
  //
  if (!S) {
    SemaRef.Diag(DefaultLoc, diag::err_omp_loop_not_canonical_cond) << Var;
    return true;
  }
  S = S->IgnoreParenImpCasts();
  SourceLocation CondLoc = S->getLocStart();
  if (auto BO = dyn_cast<BinaryOperator>(S)) {
    if (BO->isRelationalOp()) {
      if (GetInitVarDecl(BO->getLHS()) == Var)
        return SetUB(BO->getRHS(),
                     (BO->getOpcode() == BO_LT || BO->getOpcode() == BO_LE),
                     (BO->getOpcode() == BO_LT || BO->getOpcode() == BO_GT),
                     BO->getSourceRange(), BO->getOperatorLoc());
      if (GetInitVarDecl(BO->getRHS()) == Var)
        return SetUB(BO->getLHS(),
                     (BO->getOpcode() == BO_GT || BO->getOpcode() == BO_GE),
                     (BO->getOpcode() == BO_LT || BO->getOpcode() == BO_GT),
                     BO->getSourceRange(), BO->getOperatorLoc());
    }
  } else if (auto CE = dyn_cast<CXXOperatorCallExpr>(S)) {
    if (CE->getNumArgs() == 2) {
      auto Op = CE->getOperator();
      switch (Op) {
      case OO_Greater:
      case OO_GreaterEqual:
      case OO_Less:
      case OO_LessEqual:
        if (GetInitVarDecl(CE->getArg(0)) == Var)
          return SetUB(CE->getArg(1), Op == OO_Less || Op == OO_LessEqual,
                       Op == OO_Less || Op == OO_Greater, CE->getSourceRange(),
                       CE->getOperatorLoc());
        if (GetInitVarDecl(CE->getArg(1)) == Var)
          return SetUB(CE->getArg(0), Op == OO_Greater || Op == OO_GreaterEqual,
                       Op == OO_Less || Op == OO_Greater, CE->getSourceRange(),
                       CE->getOperatorLoc());
        break;
      default:
        break;
      }
    }
  }
  SemaRef.Diag(CondLoc, diag::err_omp_loop_not_canonical_cond)
      << S->getSourceRange() << Var;
  return true;
}

bool OpenMPIterationSpaceChecker::CheckIncRHS(Expr *RHS) {
  // RHS of canonical loop form increment can be:
  //   var + incr
  //   incr + var
  //   var - incr
  //
  RHS = RHS->IgnoreParenImpCasts();
  if (auto BO = dyn_cast<BinaryOperator>(RHS)) {
    if (BO->isAdditiveOp()) {
      bool IsAdd = BO->getOpcode() == BO_Add;
      if (GetInitVarDecl(BO->getLHS()) == Var)
        return SetStep(BO->getRHS(), !IsAdd);
      if (IsAdd && GetInitVarDecl(BO->getRHS()) == Var)
        return SetStep(BO->getLHS(), false);
    }
  } else if (auto CE = dyn_cast<CXXOperatorCallExpr>(RHS)) {
    bool IsAdd = CE->getOperator() == OO_Plus;
    if ((IsAdd || CE->getOperator() == OO_Minus) && CE->getNumArgs() == 2) {
      if (GetInitVarDecl(CE->getArg(0)) == Var)
        return SetStep(CE->getArg(1), !IsAdd);
      if (IsAdd && GetInitVarDecl(CE->getArg(1)) == Var)
        return SetStep(CE->getArg(0), false);
    }
  }
  SemaRef.Diag(RHS->getLocStart(), diag::err_omp_loop_not_canonical_incr)
      << RHS->getSourceRange() << Var;
  return true;
}

bool OpenMPIterationSpaceChecker::CheckInc(Expr *S) {
  // Check incr-expr for canonical loop form and return true if it
  // does not conform.
  // OpenMP [2.6] Canonical loop form. Test-expr may be one of the following:
  //   ++var
  //   var++
  //   --var
  //   var--
  //   var += incr
  //   var -= incr
  //   var = var + incr
  //   var = incr + var
  //   var = var - incr
  //
  if (!S) {
    SemaRef.Diag(DefaultLoc, diag::err_omp_loop_not_canonical_incr) << Var;
    return true;
  }
  IncrementSrcRange = S->getSourceRange();
  S = S->IgnoreParens();
  if (auto UO = dyn_cast<UnaryOperator>(S)) {
    if (UO->isIncrementDecrementOp() && GetInitVarDecl(UO->getSubExpr()) == Var)
      return SetStep(
          SemaRef.ActOnIntegerConstant(UO->getLocStart(),
                                       (UO->isDecrementOp() ? -1 : 1)).get(),
          false);
  } else if (auto BO = dyn_cast<BinaryOperator>(S)) {
    switch (BO->getOpcode()) {
    case BO_AddAssign:
    case BO_SubAssign:
      if (GetInitVarDecl(BO->getLHS()) == Var)
        return SetStep(BO->getRHS(), BO->getOpcode() == BO_SubAssign);
      break;
    case BO_Assign:
      if (GetInitVarDecl(BO->getLHS()) == Var)
        return CheckIncRHS(BO->getRHS());
      break;
    default:
      break;
    }
  } else if (auto CE = dyn_cast<CXXOperatorCallExpr>(S)) {
    switch (CE->getOperator()) {
    case OO_PlusPlus:
    case OO_MinusMinus:
      if (GetInitVarDecl(CE->getArg(0)) == Var)
        return SetStep(
            SemaRef.ActOnIntegerConstant(
                        CE->getLocStart(),
                        ((CE->getOperator() == OO_MinusMinus) ? -1 : 1)).get(),
            false);
      break;
    case OO_PlusEqual:
    case OO_MinusEqual:
      if (GetInitVarDecl(CE->getArg(0)) == Var)
        return SetStep(CE->getArg(1), CE->getOperator() == OO_MinusEqual);
      break;
    case OO_Equal:
      if (GetInitVarDecl(CE->getArg(0)) == Var)
        return CheckIncRHS(CE->getArg(1));
      break;
    default:
      break;
    }
  }
  SemaRef.Diag(S->getLocStart(), diag::err_omp_loop_not_canonical_incr)
      << S->getSourceRange() << Var;
  return true;
}

/// \brief Build the expression to calculate the number of iterations.
Expr *
OpenMPIterationSpaceChecker::BuildNumIterations(Scope *S,
                                                const bool LimitedType) const {
  ExprResult Diff;
  if (Var->getType()->isIntegerType() || Var->getType()->isPointerType() ||
      SemaRef.getLangOpts().CPlusPlus) {
    // Upper - Lower
    Expr *Upper = TestIsLessOp ? UB : LB;
    Expr *Lower = TestIsLessOp ? LB : UB;

    Diff = SemaRef.BuildBinOp(S, DefaultLoc, BO_Sub, Upper, Lower);

    if (!Diff.isUsable() && Var->getType()->getAsCXXRecordDecl()) {
      // BuildBinOp already emitted error, this one is to point user to upper
      // and lower bound, and to tell what is passed to 'operator-'.
      SemaRef.Diag(Upper->getLocStart(), diag::err_omp_loop_diff_cxx)
          << Upper->getSourceRange() << Lower->getSourceRange();
      return nullptr;
    }
  }

  if (!Diff.isUsable())
    return nullptr;

  // Upper - Lower [- 1]
  if (TestIsStrictOp)
    Diff = SemaRef.BuildBinOp(
        S, DefaultLoc, BO_Sub, Diff.get(),
        SemaRef.ActOnIntegerConstant(SourceLocation(), 1).get());
  if (!Diff.isUsable())
    return nullptr;

  // Upper - Lower [- 1] + Step
  Diff = SemaRef.BuildBinOp(S, DefaultLoc, BO_Add, Diff.get(),
                            Step->IgnoreImplicit());
  if (!Diff.isUsable())
    return nullptr;

  // Parentheses (for dumping/debugging purposes only).
  Diff = SemaRef.ActOnParenExpr(DefaultLoc, DefaultLoc, Diff.get());
  if (!Diff.isUsable())
    return nullptr;

  // (Upper - Lower [- 1] + Step) / Step
  Diff = SemaRef.BuildBinOp(S, DefaultLoc, BO_Div, Diff.get(),
                            Step->IgnoreImplicit());
  if (!Diff.isUsable())
    return nullptr;

  // OpenMP runtime requires 32-bit or 64-bit loop variables.
  if (LimitedType) {
    auto &C = SemaRef.Context;
    QualType Type = Diff.get()->getType();
    unsigned NewSize = (C.getTypeSize(Type) > 32) ? 64 : 32;
    if (NewSize != C.getTypeSize(Type)) {
      if (NewSize < C.getTypeSize(Type)) {
        assert(NewSize == 64 && "incorrect loop var size");
        SemaRef.Diag(DefaultLoc, diag::warn_omp_loop_64_bit_var)
            << InitSrcRange << ConditionSrcRange;
      }
      QualType NewType = C.getIntTypeForBitwidth(
          NewSize, Type->hasSignedIntegerRepresentation());
      Diff = SemaRef.PerformImplicitConversion(Diff.get(), NewType,
                                               Sema::AA_Converting, true);
      if (!Diff.isUsable())
        return nullptr;
    }
  }

  return Diff.get();
}

/// \brief Build reference expression to the counter be used for codegen.
Expr *OpenMPIterationSpaceChecker::BuildCounterVar() const {
  return DeclRefExpr::Create(SemaRef.Context, NestedNameSpecifierLoc(),
                             GetIncrementSrcRange().getBegin(), Var, false,
                             DefaultLoc, Var->getType(), VK_LValue);
}

/// \brief Build initization of the counter be used for codegen.
Expr *OpenMPIterationSpaceChecker::BuildCounterInit() const { return LB; }

/// \brief Build step of the counter be used for codegen.
Expr *OpenMPIterationSpaceChecker::BuildCounterStep() const { return Step; }

/// \brief Iteration space of a single for loop.
struct LoopIterationSpace {
  /// \brief This expression calculates the number of iterations in the loop.
  /// It is always possible to calculate it before starting the loop.
  Expr *NumIterations;
  /// \brief The loop counter variable.
  Expr *CounterVar;
  /// \brief This is initializer for the initial value of #CounterVar.
  Expr *CounterInit;
  /// \brief This is step for the #CounterVar used to generate its update:
  /// #CounterVar = #CounterInit + #CounterStep * CurrentIteration.
  Expr *CounterStep;
  /// \brief Should step be subtracted?
  bool Subtract;
  /// \brief Source range of the loop init.
  SourceRange InitSrcRange;
  /// \brief Source range of the loop condition.
  SourceRange CondSrcRange;
  /// \brief Source range of the loop increment.
  SourceRange IncSrcRange;
};

} // namespace

/// \brief Called on a for stmt to check and extract its iteration space
/// for further processing (such as collapsing).
static bool CheckOpenMPIterationSpace(
    OpenMPDirectiveKind DKind, Stmt *S, Sema &SemaRef, DSAStackTy &DSA,
    unsigned CurrentNestedLoopCount, unsigned NestedLoopCount,
    Expr *NestedLoopCountExpr,
    llvm::DenseMap<VarDecl *, Expr *> &VarsWithImplicitDSA,
    LoopIterationSpace &ResultIterSpace) {
  // OpenMP [2.6, Canonical Loop Form]
  //   for (init-expr; test-expr; incr-expr) structured-block
  auto For = dyn_cast_or_null<ForStmt>(S);
  if (!For) {
    SemaRef.Diag(S->getLocStart(), diag::err_omp_not_for)
        << (NestedLoopCountExpr != nullptr) << getOpenMPDirectiveName(DKind)
        << NestedLoopCount << (CurrentNestedLoopCount > 0)
        << CurrentNestedLoopCount;
    if (NestedLoopCount > 1)
      SemaRef.Diag(NestedLoopCountExpr->getExprLoc(),
                   diag::note_omp_collapse_expr)
          << NestedLoopCountExpr->getSourceRange();
    return true;
  }
  assert(For->getBody());

  OpenMPIterationSpaceChecker ISC(SemaRef, For->getForLoc());

  // Check init.
  auto Init = For->getInit();
  if (ISC.CheckInit(Init)) {
    return true;
  }

  bool HasErrors = false;

  // Check loop variable's type.
  auto Var = ISC.GetLoopVar();

  // OpenMP [2.6, Canonical Loop Form]
  // Var is one of the following:
  //   A variable of signed or unsigned integer type.
  //   For C++, a variable of a random access iterator type.
  //   For C, a variable of a pointer type.
  auto VarType = Var->getType();
  if (!VarType->isDependentType() && !VarType->isIntegerType() &&
      !VarType->isPointerType() &&
      !(SemaRef.getLangOpts().CPlusPlus && VarType->isOverloadableType())) {
    SemaRef.Diag(Init->getLocStart(), diag::err_omp_loop_variable_type)
        << SemaRef.getLangOpts().CPlusPlus;
    HasErrors = true;
  }

  // OpenMP, 2.14.1.1 Data-sharing Attribute Rules for Variables Referenced in a
  // Construct
  // The loop iteration variable(s) in the associated for-loop(s) of a for or
  // parallel for construct is (are) private.
  // The loop iteration variable in the associated for-loop of a simd construct
  // with just one associated for-loop is linear with a constant-linear-step
  // that is the increment of the associated for-loop.
  // Exclude loop var from the list of variables with implicitly defined data
  // sharing attributes.
  VarsWithImplicitDSA.erase(Var);

  // OpenMP [2.14.1.1, Data-sharing Attribute Rules for Variables Referenced in
  // a Construct, C/C++].
  // The loop iteration variable in the associated for-loop of a simd construct
  // with just one associated for-loop may be listed in a linear clause with a
  // constant-linear-step that is the increment of the associated for-loop.
  // The loop iteration variable(s) in the associated for-loop(s) of a for or
  // parallel for construct may be listed in a private or lastprivate clause.
  DSAStackTy::DSAVarData DVar = DSA.getTopDSA(Var, false);
  auto LoopVarRefExpr = ISC.GetLoopVarRefExpr();
  // If LoopVarRefExpr is nullptr it means the corresponding loop variable is
  // declared in the loop and it is predetermined as a private.
  auto PredeterminedCKind =
      isOpenMPSimdDirective(DKind)
          ? ((NestedLoopCount == 1) ? OMPC_linear : OMPC_lastprivate)
          : OMPC_private;
  if (((isOpenMPSimdDirective(DKind) && DVar.CKind != OMPC_unknown &&
        DVar.CKind != PredeterminedCKind) ||
       (isOpenMPWorksharingDirective(DKind) && !isOpenMPSimdDirective(DKind) &&
        DVar.CKind != OMPC_unknown && DVar.CKind != OMPC_private &&
        DVar.CKind != OMPC_lastprivate)) &&
      (DVar.CKind != OMPC_private || DVar.RefExpr != nullptr)) {
    SemaRef.Diag(Init->getLocStart(), diag::err_omp_loop_var_dsa)
        << getOpenMPClauseName(DVar.CKind) << getOpenMPDirectiveName(DKind)
        << getOpenMPClauseName(PredeterminedCKind);
    ReportOriginalDSA(SemaRef, &DSA, Var, DVar, true);
    HasErrors = true;
  } else if (LoopVarRefExpr != nullptr) {
    // Make the loop iteration variable private (for worksharing constructs),
    // linear (for simd directives with the only one associated loop) or
    // lastprivate (for simd directives with several collapsed loops).
    // FIXME: the next check and error message must be removed once the
    // capturing of global variables in loops is fixed.
    if (DVar.CKind == OMPC_unknown)
      DVar = DSA.hasDSA(Var, isOpenMPPrivate, MatchesAlways(),
                        /*FromParent=*/false);
    if (!Var->hasLocalStorage() && DVar.CKind == OMPC_unknown) {
      SemaRef.Diag(Init->getLocStart(), diag::err_omp_global_loop_var_dsa)
          << getOpenMPClauseName(PredeterminedCKind)
          << getOpenMPDirectiveName(DKind);
      HasErrors = true;
    } else
      DSA.addDSA(Var, LoopVarRefExpr, PredeterminedCKind);
  }

  assert(isOpenMPLoopDirective(DKind) && "DSA for non-loop vars");

  // Check test-expr.
  HasErrors |= ISC.CheckCond(For->getCond());

  // Check incr-expr.
  HasErrors |= ISC.CheckInc(For->getInc());

  if (ISC.Dependent() || SemaRef.CurContext->isDependentContext() || HasErrors)
    return HasErrors;

  // Build the loop's iteration space representation.
  ResultIterSpace.NumIterations = ISC.BuildNumIterations(
      DSA.getCurScope(), /* LimitedType */ isOpenMPWorksharingDirective(DKind));
  ResultIterSpace.CounterVar = ISC.BuildCounterVar();
  ResultIterSpace.CounterInit = ISC.BuildCounterInit();
  ResultIterSpace.CounterStep = ISC.BuildCounterStep();
  ResultIterSpace.InitSrcRange = ISC.GetInitSrcRange();
  ResultIterSpace.CondSrcRange = ISC.GetConditionSrcRange();
  ResultIterSpace.IncSrcRange = ISC.GetIncrementSrcRange();
  ResultIterSpace.Subtract = ISC.ShouldSubtractStep();

  HasErrors |= (ResultIterSpace.NumIterations == nullptr ||
                ResultIterSpace.CounterVar == nullptr ||
                ResultIterSpace.CounterInit == nullptr ||
                ResultIterSpace.CounterStep == nullptr);

  return HasErrors;
}

/// \brief Build a variable declaration for OpenMP loop iteration variable.
static VarDecl *BuildVarDecl(Sema &SemaRef, SourceLocation Loc, QualType Type,
                             StringRef Name) {
  DeclContext *DC = SemaRef.CurContext;
  IdentifierInfo *II = &SemaRef.PP.getIdentifierTable().get(Name);
  TypeSourceInfo *TInfo = SemaRef.Context.getTrivialTypeSourceInfo(Type, Loc);
  VarDecl *Decl =
      VarDecl::Create(SemaRef.Context, DC, Loc, Loc, II, Type, TInfo, SC_None);
  Decl->setImplicit();
  return Decl;
}

/// \brief Build 'VarRef = Start + Iter * Step'.
static ExprResult BuildCounterUpdate(Sema &SemaRef, Scope *S,
                                     SourceLocation Loc, ExprResult VarRef,
                                     ExprResult Start, ExprResult Iter,
                                     ExprResult Step, bool Subtract) {
  // Add parentheses (for debugging purposes only).
  Iter = SemaRef.ActOnParenExpr(Loc, Loc, Iter.get());
  if (!VarRef.isUsable() || !Start.isUsable() || !Iter.isUsable() ||
      !Step.isUsable())
    return ExprError();

  ExprResult Update = SemaRef.BuildBinOp(S, Loc, BO_Mul, Iter.get(),
                                         Step.get()->IgnoreImplicit());
  if (!Update.isUsable())
    return ExprError();

  // Build 'VarRef = Start + Iter * Step'.
  Update = SemaRef.BuildBinOp(S, Loc, (Subtract ? BO_Sub : BO_Add),
                              Start.get()->IgnoreImplicit(), Update.get());
  if (!Update.isUsable())
    return ExprError();

  Update = SemaRef.PerformImplicitConversion(
      Update.get(), VarRef.get()->getType(), Sema::AA_Converting, true);
  if (!Update.isUsable())
    return ExprError();

  Update = SemaRef.BuildBinOp(S, Loc, BO_Assign, VarRef.get(), Update.get());
  return Update;
}

/// \brief Convert integer expression \a E to make it have at least \a Bits
/// bits.
static ExprResult WidenIterationCount(unsigned Bits, Expr *E,
                                      Sema &SemaRef) {
  if (E == nullptr)
    return ExprError();
  auto &C = SemaRef.Context;
  QualType OldType = E->getType();
  unsigned HasBits = C.getTypeSize(OldType);
  if (HasBits >= Bits)
    return ExprResult(E);
  // OK to convert to signed, because new type has more bits than old.
  QualType NewType = C.getIntTypeForBitwidth(Bits, /* Signed */ true);
  return SemaRef.PerformImplicitConversion(E, NewType, Sema::AA_Converting,
                                           true);
}

/// \brief Check if the given expression \a E is a constant integer that fits
/// into \a Bits bits.
static bool FitsInto(unsigned Bits, bool Signed, Expr *E, Sema &SemaRef) {
  if (E == nullptr)
    return false;
  llvm::APSInt Result;
  if (E->isIntegerConstantExpr(Result, SemaRef.Context))
    return Signed ? Result.isSignedIntN(Bits) : Result.isIntN(Bits);
  return false;
}

/// \brief Called on a for stmt to check itself and nested loops (if any).
/// \return Returns 0 if one of the collapsed stmts is not canonical for loop,
/// number of collapsed loops otherwise.
static unsigned
CheckOpenMPLoop(OpenMPDirectiveKind DKind, Expr *NestedLoopCountExpr,
                Stmt *AStmt, Sema &SemaRef, DSAStackTy &DSA,
                llvm::DenseMap<VarDecl *, Expr *> &VarsWithImplicitDSA,
                OMPLoopDirective::HelperExprs &Built) {
  unsigned NestedLoopCount = 1;
  if (NestedLoopCountExpr) {
    // Found 'collapse' clause - calculate collapse number.
    llvm::APSInt Result;
    if (NestedLoopCountExpr->EvaluateAsInt(Result, SemaRef.getASTContext()))
      NestedLoopCount = Result.getLimitedValue();
  }
  // This is helper routine for loop directives (e.g., 'for', 'simd',
  // 'for simd', etc.).
  SmallVector<LoopIterationSpace, 4> IterSpaces;
  IterSpaces.resize(NestedLoopCount);
  Stmt *CurStmt = AStmt->IgnoreContainers(/* IgnoreCaptured */ true);
  for (unsigned Cnt = 0; Cnt < NestedLoopCount; ++Cnt) {
    if (CheckOpenMPIterationSpace(DKind, CurStmt, SemaRef, DSA, Cnt,
                                  NestedLoopCount, NestedLoopCountExpr,
                                  VarsWithImplicitDSA, IterSpaces[Cnt]))
      return 0;
    // Move on to the next nested for loop, or to the loop body.
    // OpenMP [2.8.1, simd construct, Restrictions]
    // All loops associated with the construct must be perfectly nested; that
    // is, there must be no intervening code nor any OpenMP directive between
    // any two loops.
    CurStmt = cast<ForStmt>(CurStmt)->getBody()->IgnoreContainers();
  }

  Built.clear(/* size */ NestedLoopCount);

  if (SemaRef.CurContext->isDependentContext())
    return NestedLoopCount;

  // An example of what is generated for the following code:
  //
  //   #pragma omp simd collapse(2)
  //   for (i = 0; i < NI; ++i)
  //     for (j = J0; j < NJ; j+=2) {
  //     <loop body>
  //   }
  //
  // We generate the code below.
  // Note: the loop body may be outlined in CodeGen.
  // Note: some counters may be C++ classes, operator- is used to find number of
  // iterations and operator+= to calculate counter value.
  // Note: decltype(NumIterations) must be integer type (in 'omp for', only i32
  // or i64 is currently supported).
  //
  //   #define NumIterations (NI * ((NJ - J0 - 1 + 2) / 2))
  //   for (int[32|64]_t IV = 0; IV < NumIterations; ++IV ) {
  //     .local.i = IV / ((NJ - J0 - 1 + 2) / 2);
  //     .local.j = J0 + (IV % ((NJ - J0 - 1 + 2) / 2)) * 2;
  //     // similar updates for vars in clauses (e.g. 'linear')
  //     <loop body (using local i and j)>
  //   }
  //   i = NI; // assign final values of counters
  //   j = NJ;
  //

  // Last iteration number is (I1 * I2 * ... In) - 1, where I1, I2 ... In are
  // the iteration counts of the collapsed for loops.
  auto N0 = IterSpaces[0].NumIterations;
  ExprResult LastIteration32 = WidenIterationCount(32 /* Bits */, N0, SemaRef);
  ExprResult LastIteration64 = WidenIterationCount(64 /* Bits */, N0, SemaRef);

  if (!LastIteration32.isUsable() || !LastIteration64.isUsable())
    return NestedLoopCount;

  auto &C = SemaRef.Context;
  bool AllCountsNeedLessThan32Bits = C.getTypeSize(N0->getType()) < 32;

  Scope *CurScope = DSA.getCurScope();
  for (unsigned Cnt = 1; Cnt < NestedLoopCount; ++Cnt) {
    auto N = IterSpaces[Cnt].NumIterations;
    AllCountsNeedLessThan32Bits &= C.getTypeSize(N->getType()) < 32;
    if (LastIteration32.isUsable())
      LastIteration32 = SemaRef.BuildBinOp(CurScope, SourceLocation(), BO_Mul,
                                           LastIteration32.get(), N);
    if (LastIteration64.isUsable())
      LastIteration64 = SemaRef.BuildBinOp(CurScope, SourceLocation(), BO_Mul,
                                           LastIteration64.get(), N);
  }

  // Choose either the 32-bit or 64-bit version.
  ExprResult LastIteration = LastIteration64;
  if (LastIteration32.isUsable() &&
      C.getTypeSize(LastIteration32.get()->getType()) == 32 &&
      (AllCountsNeedLessThan32Bits || NestedLoopCount == 1 ||
       FitsInto(
           32 /* Bits */,
           LastIteration32.get()->getType()->hasSignedIntegerRepresentation(),
           LastIteration64.get(), SemaRef)))
    LastIteration = LastIteration32;

  if (!LastIteration.isUsable())
    return 0;

  // Save the number of iterations.
  ExprResult NumIterations = LastIteration;
  {
    LastIteration = SemaRef.BuildBinOp(
        CurScope, SourceLocation(), BO_Sub, LastIteration.get(),
        SemaRef.ActOnIntegerConstant(SourceLocation(), 1).get());
    if (!LastIteration.isUsable())
      return 0;
  }

  // Calculate the last iteration number beforehand instead of doing this on
  // each iteration. Do not do this if the number of iterations may be kfold-ed.
  llvm::APSInt Result;
  bool IsConstant =
      LastIteration.get()->isIntegerConstantExpr(Result, SemaRef.Context);
  ExprResult CalcLastIteration;
  if (!IsConstant) {
    SourceLocation SaveLoc;
    VarDecl *SaveVar =
        BuildVarDecl(SemaRef, SaveLoc, LastIteration.get()->getType(),
                     ".omp.last.iteration");
    ExprResult SaveRef = SemaRef.BuildDeclRefExpr(
        SaveVar, LastIteration.get()->getType(), VK_LValue, SaveLoc);
    CalcLastIteration = SemaRef.BuildBinOp(CurScope, SaveLoc, BO_Assign,
                                           SaveRef.get(), LastIteration.get());
    LastIteration = SaveRef;

    // Prepare SaveRef + 1.
    NumIterations = SemaRef.BuildBinOp(
        CurScope, SaveLoc, BO_Add, SaveRef.get(),
        SemaRef.ActOnIntegerConstant(SourceLocation(), 1).get());
    if (!NumIterations.isUsable())
      return 0;
  }

  SourceLocation InitLoc = IterSpaces[0].InitSrcRange.getBegin();

  // Precondition tests if there is at least one iteration (LastIteration > 0).
  ExprResult PreCond = SemaRef.BuildBinOp(
      CurScope, InitLoc, BO_GT, LastIteration.get(),
      SemaRef.ActOnIntegerConstant(SourceLocation(), 0).get());

  QualType VType = LastIteration.get()->getType();
  // Build variables passed into runtime, nesessary for worksharing directives.
  ExprResult LB, UB, IL, ST, EUB;
  if (isOpenMPWorksharingDirective(DKind)) {
    // Lower bound variable, initialized with zero.
    VarDecl *LBDecl = BuildVarDecl(SemaRef, InitLoc, VType, ".omp.lb");
    LB = SemaRef.BuildDeclRefExpr(LBDecl, VType, VK_LValue, InitLoc);
    SemaRef.AddInitializerToDecl(
        LBDecl, SemaRef.ActOnIntegerConstant(InitLoc, 0).get(),
        /*DirectInit*/ false, /*TypeMayContainAuto*/ false);

    // Upper bound variable, initialized with last iteration number.
    VarDecl *UBDecl = BuildVarDecl(SemaRef, InitLoc, VType, ".omp.ub");
    UB = SemaRef.BuildDeclRefExpr(UBDecl, VType, VK_LValue, InitLoc);
    SemaRef.AddInitializerToDecl(UBDecl, LastIteration.get(),
                                 /*DirectInit*/ false,
                                 /*TypeMayContainAuto*/ false);

    // A 32-bit variable-flag where runtime returns 1 for the last iteration.
    // This will be used to implement clause 'lastprivate'.
    QualType Int32Ty = SemaRef.Context.getIntTypeForBitwidth(32, true);
    VarDecl *ILDecl = BuildVarDecl(SemaRef, InitLoc, Int32Ty, ".omp.is_last");
    IL = SemaRef.BuildDeclRefExpr(ILDecl, Int32Ty, VK_LValue, InitLoc);
    SemaRef.AddInitializerToDecl(
        ILDecl, SemaRef.ActOnIntegerConstant(InitLoc, 0).get(),
        /*DirectInit*/ false, /*TypeMayContainAuto*/ false);

    // Stride variable returned by runtime (we initialize it to 1 by default).
    VarDecl *STDecl = BuildVarDecl(SemaRef, InitLoc, VType, ".omp.stride");
    ST = SemaRef.BuildDeclRefExpr(STDecl, VType, VK_LValue, InitLoc);
    SemaRef.AddInitializerToDecl(
        STDecl, SemaRef.ActOnIntegerConstant(InitLoc, 1).get(),
        /*DirectInit*/ false, /*TypeMayContainAuto*/ false);

    // Build expression: UB = min(UB, LastIteration)
    // It is nesessary for CodeGen of directives with static scheduling.
    ExprResult IsUBGreater = SemaRef.BuildBinOp(CurScope, InitLoc, BO_GT,
                                                UB.get(), LastIteration.get());
    ExprResult CondOp = SemaRef.ActOnConditionalOp(
        InitLoc, InitLoc, IsUBGreater.get(), LastIteration.get(), UB.get());
    EUB = SemaRef.BuildBinOp(CurScope, InitLoc, BO_Assign, UB.get(),
                             CondOp.get());
    EUB = SemaRef.ActOnFinishFullExpr(EUB.get());
  }

  // Build the iteration variable and its initialization before loop.
  ExprResult IV;
  ExprResult Init;
  {
    VarDecl *IVDecl = BuildVarDecl(SemaRef, InitLoc, VType, ".omp.iv");
    IV = SemaRef.BuildDeclRefExpr(IVDecl, VType, VK_LValue, InitLoc);
    Expr *RHS = isOpenMPWorksharingDirective(DKind)
                    ? LB.get()
                    : SemaRef.ActOnIntegerConstant(SourceLocation(), 0).get();
    Init = SemaRef.BuildBinOp(CurScope, InitLoc, BO_Assign, IV.get(), RHS);
    Init = SemaRef.ActOnFinishFullExpr(Init.get());
  }

  // Loop condition (IV < NumIterations) or (IV <= UB) for worksharing loops.
  SourceLocation CondLoc;
  ExprResult Cond =
      isOpenMPWorksharingDirective(DKind)
          ? SemaRef.BuildBinOp(CurScope, CondLoc, BO_LE, IV.get(), UB.get())
          : SemaRef.BuildBinOp(CurScope, CondLoc, BO_LT, IV.get(),
                               NumIterations.get());
  // Loop condition with 1 iteration separated (IV < LastIteration)
  ExprResult SeparatedCond = SemaRef.BuildBinOp(CurScope, CondLoc, BO_LT,
                                                IV.get(), LastIteration.get());

  // Loop increment (IV = IV + 1)
  SourceLocation IncLoc;
  ExprResult Inc =
      SemaRef.BuildBinOp(CurScope, IncLoc, BO_Add, IV.get(),
                         SemaRef.ActOnIntegerConstant(IncLoc, 1).get());
  if (!Inc.isUsable())
    return 0;
  Inc = SemaRef.BuildBinOp(CurScope, IncLoc, BO_Assign, IV.get(), Inc.get());
  Inc = SemaRef.ActOnFinishFullExpr(Inc.get());
  if (!Inc.isUsable())
    return 0;

  // Increments for worksharing loops (LB = LB + ST; UB = UB + ST).
  // Used for directives with static scheduling.
  ExprResult NextLB, NextUB;
  if (isOpenMPWorksharingDirective(DKind)) {
    // LB + ST
    NextLB = SemaRef.BuildBinOp(CurScope, IncLoc, BO_Add, LB.get(), ST.get());
    if (!NextLB.isUsable())
      return 0;
    // LB = LB + ST
    NextLB =
        SemaRef.BuildBinOp(CurScope, IncLoc, BO_Assign, LB.get(), NextLB.get());
    NextLB = SemaRef.ActOnFinishFullExpr(NextLB.get());
    if (!NextLB.isUsable())
      return 0;
    // UB + ST
    NextUB = SemaRef.BuildBinOp(CurScope, IncLoc, BO_Add, UB.get(), ST.get());
    if (!NextUB.isUsable())
      return 0;
    // UB = UB + ST
    NextUB =
        SemaRef.BuildBinOp(CurScope, IncLoc, BO_Assign, UB.get(), NextUB.get());
    NextUB = SemaRef.ActOnFinishFullExpr(NextUB.get());
    if (!NextUB.isUsable())
      return 0;
  }

  // Build updates and final values of the loop counters.
  bool HasErrors = false;
  Built.Counters.resize(NestedLoopCount);
  Built.Updates.resize(NestedLoopCount);
  Built.Finals.resize(NestedLoopCount);
  {
    ExprResult Div;
    // Go from inner nested loop to outer.
    for (int Cnt = NestedLoopCount - 1; Cnt >= 0; --Cnt) {
      LoopIterationSpace &IS = IterSpaces[Cnt];
      SourceLocation UpdLoc = IS.IncSrcRange.getBegin();
      // Build: Iter = (IV / Div) % IS.NumIters
      // where Div is product of previous iterations' IS.NumIters.
      ExprResult Iter;
      if (Div.isUsable()) {
        Iter =
            SemaRef.BuildBinOp(CurScope, UpdLoc, BO_Div, IV.get(), Div.get());
      } else {
        Iter = IV;
        assert((Cnt == (int)NestedLoopCount - 1) &&
               "unusable div expected on first iteration only");
      }

      if (Cnt != 0 && Iter.isUsable())
        Iter = SemaRef.BuildBinOp(CurScope, UpdLoc, BO_Rem, Iter.get(),
                                  IS.NumIterations);
      if (!Iter.isUsable()) {
        HasErrors = true;
        break;
      }

      // Build update: IS.CounterVar = IS.Start + Iter * IS.Step
      ExprResult Update =
          BuildCounterUpdate(SemaRef, CurScope, UpdLoc, IS.CounterVar,
                             IS.CounterInit, Iter, IS.CounterStep, IS.Subtract);
      if (!Update.isUsable()) {
        HasErrors = true;
        break;
      }

      // Build final: IS.CounterVar = IS.Start + IS.NumIters * IS.Step
      ExprResult Final = BuildCounterUpdate(
          SemaRef, CurScope, UpdLoc, IS.CounterVar, IS.CounterInit,
          IS.NumIterations, IS.CounterStep, IS.Subtract);
      if (!Final.isUsable()) {
        HasErrors = true;
        break;
      }

      // Build Div for the next iteration: Div <- Div * IS.NumIters
      if (Cnt != 0) {
        if (Div.isUnset())
          Div = IS.NumIterations;
        else
          Div = SemaRef.BuildBinOp(CurScope, UpdLoc, BO_Mul, Div.get(),
                                   IS.NumIterations);

        // Add parentheses (for debugging purposes only).
        if (Div.isUsable())
          Div = SemaRef.ActOnParenExpr(UpdLoc, UpdLoc, Div.get());
        if (!Div.isUsable()) {
          HasErrors = true;
          break;
        }
      }
      if (!Update.isUsable() || !Final.isUsable()) {
        HasErrors = true;
        break;
      }
      // Save results
      Built.Counters[Cnt] = IS.CounterVar;
      Built.Updates[Cnt] = Update.get();
      Built.Finals[Cnt] = Final.get();
    }
  }

  if (HasErrors)
    return 0;

  // Save results
  Built.IterationVarRef = IV.get();
  Built.LastIteration = LastIteration.get();
  Built.CalcLastIteration = CalcLastIteration.get();
  Built.PreCond = PreCond.get();
  Built.Cond = Cond.get();
  Built.SeparatedCond = SeparatedCond.get();
  Built.Init = Init.get();
  Built.Inc = Inc.get();
  Built.LB = LB.get();
  Built.UB = UB.get();
  Built.IL = IL.get();
  Built.ST = ST.get();
  Built.EUB = EUB.get();
  Built.NLB = NextLB.get();
  Built.NUB = NextUB.get();

  return NestedLoopCount;
}

static Expr *GetCollapseNumberExpr(ArrayRef<OMPClause *> Clauses) {
  auto CollapseFilter = [](const OMPClause *C) -> bool {
    return C->getClauseKind() == OMPC_collapse;
  };
  OMPExecutableDirective::filtered_clause_iterator<decltype(CollapseFilter)> I(
      Clauses, CollapseFilter);
  if (I)
    return cast<OMPCollapseClause>(*I)->getNumForLoops();
  return nullptr;
}

StmtResult Sema::ActOnOpenMPSimdDirective(
    ArrayRef<OMPClause *> Clauses, Stmt *AStmt, SourceLocation StartLoc,
    SourceLocation EndLoc,
    llvm::DenseMap<VarDecl *, Expr *> &VarsWithImplicitDSA) {
  OMPLoopDirective::HelperExprs B;
  // In presence of clause 'collapse', it will define the nested loops number.
  unsigned NestedLoopCount =
      CheckOpenMPLoop(OMPD_simd, GetCollapseNumberExpr(Clauses), AStmt, *this,
                      *DSAStack, VarsWithImplicitDSA, B);
  if (NestedLoopCount == 0)
    return StmtError();

  assert((CurContext->isDependentContext() || B.builtAll()) &&
         "omp simd loop exprs were not built");

  getCurFunction()->setHasBranchProtectedScope();
  return OMPSimdDirective::Create(Context, StartLoc, EndLoc, NestedLoopCount,
                                  Clauses, AStmt, B);
}

StmtResult Sema::ActOnOpenMPForDirective(
    ArrayRef<OMPClause *> Clauses, Stmt *AStmt, SourceLocation StartLoc,
    SourceLocation EndLoc,
    llvm::DenseMap<VarDecl *, Expr *> &VarsWithImplicitDSA) {
  OMPLoopDirective::HelperExprs B;
  // In presence of clause 'collapse', it will define the nested loops number.
  unsigned NestedLoopCount =
      CheckOpenMPLoop(OMPD_for, GetCollapseNumberExpr(Clauses), AStmt, *this,
                      *DSAStack, VarsWithImplicitDSA, B);
  if (NestedLoopCount == 0)
    return StmtError();

  assert((CurContext->isDependentContext() || B.builtAll()) &&
         "omp for loop exprs were not built");

  getCurFunction()->setHasBranchProtectedScope();
  return OMPForDirective::Create(Context, StartLoc, EndLoc, NestedLoopCount,
                                 Clauses, AStmt, B);
}

StmtResult Sema::ActOnOpenMPForSimdDirective(
    ArrayRef<OMPClause *> Clauses, Stmt *AStmt, SourceLocation StartLoc,
    SourceLocation EndLoc,
    llvm::DenseMap<VarDecl *, Expr *> &VarsWithImplicitDSA) {
  OMPLoopDirective::HelperExprs B;
  // In presence of clause 'collapse', it will define the nested loops number.
  unsigned NestedLoopCount =
      CheckOpenMPLoop(OMPD_for_simd, GetCollapseNumberExpr(Clauses), AStmt,
                      *this, *DSAStack, VarsWithImplicitDSA, B);
  if (NestedLoopCount == 0)
    return StmtError();

  assert((CurContext->isDependentContext() || B.builtAll()) &&
         "omp for simd loop exprs were not built");

  getCurFunction()->setHasBranchProtectedScope();
  return OMPForSimdDirective::Create(Context, StartLoc, EndLoc, NestedLoopCount,
                                     Clauses, AStmt, B);
}

StmtResult Sema::ActOnOpenMPSectionsDirective(ArrayRef<OMPClause *> Clauses,
                                              Stmt *AStmt,
                                              SourceLocation StartLoc,
                                              SourceLocation EndLoc) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");
  auto BaseStmt = AStmt;
  while (CapturedStmt *CS = dyn_cast_or_null<CapturedStmt>(BaseStmt))
    BaseStmt = CS->getCapturedStmt();
  if (auto C = dyn_cast_or_null<CompoundStmt>(BaseStmt)) {
    auto S = C->children();
    if (!S)
      return StmtError();
    // All associated statements must be '#pragma omp section' except for
    // the first one.
    for (++S; S; ++S) {
      auto SectionStmt = *S;
      if (!SectionStmt || !isa<OMPSectionDirective>(SectionStmt)) {
        if (SectionStmt)
          Diag(SectionStmt->getLocStart(),
               diag::err_omp_sections_substmt_not_section);
        return StmtError();
      }
    }
  } else {
    Diag(AStmt->getLocStart(), diag::err_omp_sections_not_compound_stmt);
    return StmtError();
  }

  getCurFunction()->setHasBranchProtectedScope();

  return OMPSectionsDirective::Create(Context, StartLoc, EndLoc, Clauses,
                                      AStmt);
}

StmtResult Sema::ActOnOpenMPSectionDirective(Stmt *AStmt,
                                             SourceLocation StartLoc,
                                             SourceLocation EndLoc) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");

  getCurFunction()->setHasBranchProtectedScope();

  return OMPSectionDirective::Create(Context, StartLoc, EndLoc, AStmt);
}

StmtResult Sema::ActOnOpenMPSingleDirective(ArrayRef<OMPClause *> Clauses,
                                            Stmt *AStmt,
                                            SourceLocation StartLoc,
                                            SourceLocation EndLoc) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");

  getCurFunction()->setHasBranchProtectedScope();

  return OMPSingleDirective::Create(Context, StartLoc, EndLoc, Clauses, AStmt);
}

StmtResult Sema::ActOnOpenMPMasterDirective(Stmt *AStmt,
                                            SourceLocation StartLoc,
                                            SourceLocation EndLoc) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");

  getCurFunction()->setHasBranchProtectedScope();

  return OMPMasterDirective::Create(Context, StartLoc, EndLoc, AStmt);
}

StmtResult
Sema::ActOnOpenMPCriticalDirective(const DeclarationNameInfo &DirName,
                                   Stmt *AStmt, SourceLocation StartLoc,
                                   SourceLocation EndLoc) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");

  getCurFunction()->setHasBranchProtectedScope();

  return OMPCriticalDirective::Create(Context, DirName, StartLoc, EndLoc,
                                      AStmt);
}

StmtResult Sema::ActOnOpenMPParallelForDirective(
    ArrayRef<OMPClause *> Clauses, Stmt *AStmt, SourceLocation StartLoc,
    SourceLocation EndLoc,
    llvm::DenseMap<VarDecl *, Expr *> &VarsWithImplicitDSA) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");
  CapturedStmt *CS = cast<CapturedStmt>(AStmt);
  // 1.2.2 OpenMP Language Terminology
  // Structured block - An executable statement with a single entry at the
  // top and a single exit at the bottom.
  // The point of exit cannot be a branch out of the structured block.
  // longjmp() and throw() must not violate the entry/exit criteria.
  CS->getCapturedDecl()->setNothrow();

  OMPLoopDirective::HelperExprs B;
  // In presence of clause 'collapse', it will define the nested loops number.
  unsigned NestedLoopCount =
      CheckOpenMPLoop(OMPD_parallel_for, GetCollapseNumberExpr(Clauses), AStmt,
                      *this, *DSAStack, VarsWithImplicitDSA, B);
  if (NestedLoopCount == 0)
    return StmtError();

  assert((CurContext->isDependentContext() || B.builtAll()) &&
         "omp parallel for loop exprs were not built");

  getCurFunction()->setHasBranchProtectedScope();
  return OMPParallelForDirective::Create(Context, StartLoc, EndLoc,
                                         NestedLoopCount, Clauses, AStmt, B);
}

StmtResult Sema::ActOnOpenMPParallelForSimdDirective(
    ArrayRef<OMPClause *> Clauses, Stmt *AStmt, SourceLocation StartLoc,
    SourceLocation EndLoc,
    llvm::DenseMap<VarDecl *, Expr *> &VarsWithImplicitDSA) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");
  CapturedStmt *CS = cast<CapturedStmt>(AStmt);
  // 1.2.2 OpenMP Language Terminology
  // Structured block - An executable statement with a single entry at the
  // top and a single exit at the bottom.
  // The point of exit cannot be a branch out of the structured block.
  // longjmp() and throw() must not violate the entry/exit criteria.
  CS->getCapturedDecl()->setNothrow();

  OMPLoopDirective::HelperExprs B;
  // In presence of clause 'collapse', it will define the nested loops number.
  unsigned NestedLoopCount =
      CheckOpenMPLoop(OMPD_parallel_for_simd, GetCollapseNumberExpr(Clauses),
                      AStmt, *this, *DSAStack, VarsWithImplicitDSA, B);
  if (NestedLoopCount == 0)
    return StmtError();

  getCurFunction()->setHasBranchProtectedScope();
  return OMPParallelForSimdDirective::Create(
      Context, StartLoc, EndLoc, NestedLoopCount, Clauses, AStmt, B);
}

StmtResult
Sema::ActOnOpenMPParallelSectionsDirective(ArrayRef<OMPClause *> Clauses,
                                           Stmt *AStmt, SourceLocation StartLoc,
                                           SourceLocation EndLoc) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");
  auto BaseStmt = AStmt;
  while (CapturedStmt *CS = dyn_cast_or_null<CapturedStmt>(BaseStmt))
    BaseStmt = CS->getCapturedStmt();
  if (auto C = dyn_cast_or_null<CompoundStmt>(BaseStmt)) {
    auto S = C->children();
    if (!S)
      return StmtError();
    // All associated statements must be '#pragma omp section' except for
    // the first one.
    for (++S; S; ++S) {
      auto SectionStmt = *S;
      if (!SectionStmt || !isa<OMPSectionDirective>(SectionStmt)) {
        if (SectionStmt)
          Diag(SectionStmt->getLocStart(),
               diag::err_omp_parallel_sections_substmt_not_section);
        return StmtError();
      }
    }
  } else {
    Diag(AStmt->getLocStart(),
         diag::err_omp_parallel_sections_not_compound_stmt);
    return StmtError();
  }

  getCurFunction()->setHasBranchProtectedScope();

  return OMPParallelSectionsDirective::Create(Context, StartLoc, EndLoc,
                                              Clauses, AStmt);
}

StmtResult Sema::ActOnOpenMPTaskDirective(ArrayRef<OMPClause *> Clauses,
                                          Stmt *AStmt, SourceLocation StartLoc,
                                          SourceLocation EndLoc) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");
  CapturedStmt *CS = cast<CapturedStmt>(AStmt);
  // 1.2.2 OpenMP Language Terminology
  // Structured block - An executable statement with a single entry at the
  // top and a single exit at the bottom.
  // The point of exit cannot be a branch out of the structured block.
  // longjmp() and throw() must not violate the entry/exit criteria.
  CS->getCapturedDecl()->setNothrow();

  getCurFunction()->setHasBranchProtectedScope();

  return OMPTaskDirective::Create(Context, StartLoc, EndLoc, Clauses, AStmt);
}

StmtResult Sema::ActOnOpenMPTaskyieldDirective(SourceLocation StartLoc,
                                               SourceLocation EndLoc) {
  return OMPTaskyieldDirective::Create(Context, StartLoc, EndLoc);
}

StmtResult Sema::ActOnOpenMPBarrierDirective(SourceLocation StartLoc,
                                             SourceLocation EndLoc) {
  return OMPBarrierDirective::Create(Context, StartLoc, EndLoc);
}

StmtResult Sema::ActOnOpenMPTaskwaitDirective(SourceLocation StartLoc,
                                              SourceLocation EndLoc) {
  return OMPTaskwaitDirective::Create(Context, StartLoc, EndLoc);
}

StmtResult Sema::ActOnOpenMPFlushDirective(ArrayRef<OMPClause *> Clauses,
                                           SourceLocation StartLoc,
                                           SourceLocation EndLoc) {
  assert(Clauses.size() <= 1 && "Extra clauses in flush directive");
  return OMPFlushDirective::Create(Context, StartLoc, EndLoc, Clauses);
}

StmtResult Sema::ActOnOpenMPOrderedDirective(Stmt *AStmt,
                                             SourceLocation StartLoc,
                                             SourceLocation EndLoc) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");

  getCurFunction()->setHasBranchProtectedScope();

  return OMPOrderedDirective::Create(Context, StartLoc, EndLoc, AStmt);
}

StmtResult Sema::ActOnOpenMPAtomicDirective(ArrayRef<OMPClause *> Clauses,
                                            Stmt *AStmt,
                                            SourceLocation StartLoc,
                                            SourceLocation EndLoc) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");
  auto CS = cast<CapturedStmt>(AStmt);
  // 1.2.2 OpenMP Language Terminology
  // Structured block - An executable statement with a single entry at the
  // top and a single exit at the bottom.
  // The point of exit cannot be a branch out of the structured block.
  // longjmp() and throw() must not violate the entry/exit criteria.
  // TODO further analysis of associated statements and clauses.
  OpenMPClauseKind AtomicKind = OMPC_unknown;
  SourceLocation AtomicKindLoc;
  for (auto *C : Clauses) {
    if (C->getClauseKind() == OMPC_read || C->getClauseKind() == OMPC_write ||
        C->getClauseKind() == OMPC_update ||
        C->getClauseKind() == OMPC_capture) {
      if (AtomicKind != OMPC_unknown) {
        Diag(C->getLocStart(), diag::err_omp_atomic_several_clauses)
            << SourceRange(C->getLocStart(), C->getLocEnd());
        Diag(AtomicKindLoc, diag::note_omp_atomic_previous_clause)
            << getOpenMPClauseName(AtomicKind);
      } else {
        AtomicKind = C->getClauseKind();
        AtomicKindLoc = C->getLocStart();
      }
    }
  }

  auto Body = CS->getCapturedStmt();
  Expr *X = nullptr;
  Expr *V = nullptr;
  Expr *E = nullptr;
  // OpenMP [2.12.6, atomic Construct]
  // In the next expressions:
  // * x and v (as applicable) are both l-value expressions with scalar type.
  // * During the execution of an atomic region, multiple syntactic
  // occurrences of x must designate the same storage location.
  // * Neither of v and expr (as applicable) may access the storage location
  // designated by x.
  // * Neither of x and expr (as applicable) may access the storage location
  // designated by v.
  // * expr is an expression with scalar type.
  // * binop is one of +, *, -, /, &, ^, |, <<, or >>.
  // * binop, binop=, ++, and -- are not overloaded operators.
  // * The expression x binop expr must be numerically equivalent to x binop
  // (expr). This requirement is satisfied if the operators in expr have
  // precedence greater than binop, or by using parentheses around expr or
  // subexpressions of expr.
  // * The expression expr binop x must be numerically equivalent to (expr)
  // binop x. This requirement is satisfied if the operators in expr have
  // precedence equal to or greater than binop, or by using parentheses around
  // expr or subexpressions of expr.
  // * For forms that allow multiple occurrences of x, the number of times
  // that x is evaluated is unspecified.
  enum {
    NotAnExpression,
    NotAnAssignmentOp,
    NotAScalarType,
    NotAnLValue,
    NoError
  } ErrorFound = NoError;
  if (AtomicKind == OMPC_read) {
    SourceLocation ErrorLoc, NoteLoc;
    SourceRange ErrorRange, NoteRange;
    // If clause is read:
    //  v = x;
    if (auto AtomicBody = dyn_cast<Expr>(Body)) {
      auto AtomicBinOp =
          dyn_cast<BinaryOperator>(AtomicBody->IgnoreParenImpCasts());
      if (AtomicBinOp && AtomicBinOp->getOpcode() == BO_Assign) {
        X = AtomicBinOp->getRHS()->IgnoreParenImpCasts();
        V = AtomicBinOp->getLHS()->IgnoreParenImpCasts();
        if ((X->isInstantiationDependent() || X->getType()->isScalarType()) &&
            (V->isInstantiationDependent() || V->getType()->isScalarType())) {
          if (!X->isLValue() || !V->isLValue()) {
            auto NotLValueExpr = X->isLValue() ? V : X;
            ErrorFound = NotAnLValue;
            ErrorLoc = AtomicBinOp->getExprLoc();
            ErrorRange = AtomicBinOp->getSourceRange();
            NoteLoc = NotLValueExpr->getExprLoc();
            NoteRange = NotLValueExpr->getSourceRange();
          }
        } else if (!X->isInstantiationDependent() ||
                   !V->isInstantiationDependent()) {
          auto NotScalarExpr =
              (X->isInstantiationDependent() || X->getType()->isScalarType())
                  ? V
                  : X;
          ErrorFound = NotAScalarType;
          ErrorLoc = AtomicBinOp->getExprLoc();
          ErrorRange = AtomicBinOp->getSourceRange();
          NoteLoc = NotScalarExpr->getExprLoc();
          NoteRange = NotScalarExpr->getSourceRange();
        }
      } else {
        ErrorFound = NotAnAssignmentOp;
        ErrorLoc = AtomicBody->getExprLoc();
        ErrorRange = AtomicBody->getSourceRange();
        NoteLoc = AtomicBinOp ? AtomicBinOp->getOperatorLoc()
                              : AtomicBody->getExprLoc();
        NoteRange = AtomicBinOp ? AtomicBinOp->getSourceRange()
                                : AtomicBody->getSourceRange();
      }
    } else {
      ErrorFound = NotAnExpression;
      NoteLoc = ErrorLoc = Body->getLocStart();
      NoteRange = ErrorRange = SourceRange(NoteLoc, NoteLoc);
    }
    if (ErrorFound != NoError) {
      Diag(ErrorLoc, diag::err_omp_atomic_read_not_expression_statement)
          << ErrorRange;
      Diag(NoteLoc, diag::note_omp_atomic_read_write) << ErrorFound
                                                      << NoteRange;
      return StmtError();
    } else if (CurContext->isDependentContext())
      V = X = nullptr;
  } else if (AtomicKind == OMPC_write) {
    SourceLocation ErrorLoc, NoteLoc;
    SourceRange ErrorRange, NoteRange;
    // If clause is write:
    //  x = expr;
    if (auto AtomicBody = dyn_cast<Expr>(Body)) {
      auto AtomicBinOp =
          dyn_cast<BinaryOperator>(AtomicBody->IgnoreParenImpCasts());
      if (AtomicBinOp && AtomicBinOp->getOpcode() == BO_Assign) {
        X = AtomicBinOp->getLHS()->IgnoreParenImpCasts();
        E = AtomicBinOp->getRHS()->IgnoreParenImpCasts();
        if ((X->isInstantiationDependent() || X->getType()->isScalarType()) &&
            (E->isInstantiationDependent() || E->getType()->isScalarType())) {
          if (!X->isLValue()) {
            ErrorFound = NotAnLValue;
            ErrorLoc = AtomicBinOp->getExprLoc();
            ErrorRange = AtomicBinOp->getSourceRange();
            NoteLoc = X->getExprLoc();
            NoteRange = X->getSourceRange();
          }
        } else if (!X->isInstantiationDependent() ||
                   !E->isInstantiationDependent()) {
          auto NotScalarExpr =
              (X->isInstantiationDependent() || X->getType()->isScalarType())
                  ? E
                  : X;
          ErrorFound = NotAScalarType;
          ErrorLoc = AtomicBinOp->getExprLoc();
          ErrorRange = AtomicBinOp->getSourceRange();
          NoteLoc = NotScalarExpr->getExprLoc();
          NoteRange = NotScalarExpr->getSourceRange();
        }
      } else {
        ErrorFound = NotAnAssignmentOp;
        ErrorLoc = AtomicBody->getExprLoc();
        ErrorRange = AtomicBody->getSourceRange();
        NoteLoc = AtomicBinOp ? AtomicBinOp->getOperatorLoc()
                              : AtomicBody->getExprLoc();
        NoteRange = AtomicBinOp ? AtomicBinOp->getSourceRange()
                                : AtomicBody->getSourceRange();
      }
    } else {
      ErrorFound = NotAnExpression;
      NoteLoc = ErrorLoc = Body->getLocStart();
      NoteRange = ErrorRange = SourceRange(NoteLoc, NoteLoc);
    }
    if (ErrorFound != NoError) {
      Diag(ErrorLoc, diag::err_omp_atomic_write_not_expression_statement)
          << ErrorRange;
      Diag(NoteLoc, diag::note_omp_atomic_read_write) << ErrorFound
                                                      << NoteRange;
      return StmtError();
    } else if (CurContext->isDependentContext())
      E = X = nullptr;
  } else if (AtomicKind == OMPC_update || AtomicKind == OMPC_unknown) {
    if (!isa<Expr>(Body)) {
      Diag(Body->getLocStart(),
           diag::err_omp_atomic_update_not_expression_statement)
          << (AtomicKind == OMPC_update);
      return StmtError();
    }
  } else if (AtomicKind == OMPC_capture) {
    if (isa<Expr>(Body) && !isa<BinaryOperator>(Body)) {
      Diag(Body->getLocStart(),
           diag::err_omp_atomic_capture_not_expression_statement);
      return StmtError();
    } else if (!isa<Expr>(Body) && !isa<CompoundStmt>(Body)) {
      Diag(Body->getLocStart(),
           diag::err_omp_atomic_capture_not_compound_statement);
      return StmtError();
    }
  }

  getCurFunction()->setHasBranchProtectedScope();

  return OMPAtomicDirective::Create(Context, StartLoc, EndLoc, Clauses, AStmt,
                                    X, V, E);
}

StmtResult Sema::ActOnOpenMPTargetDirective(ArrayRef<OMPClause *> Clauses,
                                            Stmt *AStmt,
                                            SourceLocation StartLoc,
                                            SourceLocation EndLoc) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");

  // OpenMP [2.16, Nesting of Regions]
  // If specified, a teams construct must be contained within a target
  // construct. That target construct must contain no statements or directives
  // outside of the teams construct.
  if (DSAStack->hasInnerTeamsRegion()) {
    auto S = AStmt->IgnoreContainers(/*IgnoreCaptured*/ true);
    bool OMPTeamsFound = true;
    if (auto *CS = dyn_cast<CompoundStmt>(S)) {
      auto I = CS->body_begin();
      while (I != CS->body_end()) {
        auto OED = dyn_cast<OMPExecutableDirective>(*I);
        if (!OED || !isOpenMPTeamsDirective(OED->getDirectiveKind())) {
          OMPTeamsFound = false;
          break;
        }
        ++I;
      }
      assert(I != CS->body_end() && "Not found statement");
      S = *I;
    }
    if (!OMPTeamsFound) {
      Diag(StartLoc, diag::err_omp_target_contains_not_only_teams);
      Diag(DSAStack->getInnerTeamsRegionLoc(),
           diag::note_omp_nested_teams_construct_here);
      Diag(S->getLocStart(), diag::note_omp_nested_statement_here)
          << isa<OMPExecutableDirective>(S);
      return StmtError();
    }
  }

  getCurFunction()->setHasBranchProtectedScope();

  return OMPTargetDirective::Create(Context, StartLoc, EndLoc, Clauses, AStmt);
}

StmtResult Sema::ActOnOpenMPTeamsDirective(ArrayRef<OMPClause *> Clauses,
                                           Stmt *AStmt, SourceLocation StartLoc,
                                           SourceLocation EndLoc) {
  assert(AStmt && isa<CapturedStmt>(AStmt) && "Captured statement expected");
  CapturedStmt *CS = cast<CapturedStmt>(AStmt);
  // 1.2.2 OpenMP Language Terminology
  // Structured block - An executable statement with a single entry at the
  // top and a single exit at the bottom.
  // The point of exit cannot be a branch out of the structured block.
  // longjmp() and throw() must not violate the entry/exit criteria.
  CS->getCapturedDecl()->setNothrow();

  getCurFunction()->setHasBranchProtectedScope();

  return OMPTeamsDirective::Create(Context, StartLoc, EndLoc, Clauses, AStmt);
}

OMPClause *Sema::ActOnOpenMPSingleExprClause(OpenMPClauseKind Kind, Expr *Expr,
                                             SourceLocation StartLoc,
                                             SourceLocation LParenLoc,
                                             SourceLocation EndLoc) {
  OMPClause *Res = nullptr;
  switch (Kind) {
  case OMPC_if:
    Res = ActOnOpenMPIfClause(Expr, StartLoc, LParenLoc, EndLoc);
    break;
  case OMPC_final:
    Res = ActOnOpenMPFinalClause(Expr, StartLoc, LParenLoc, EndLoc);
    break;
  case OMPC_num_threads:
    Res = ActOnOpenMPNumThreadsClause(Expr, StartLoc, LParenLoc, EndLoc);
    break;
  case OMPC_safelen:
    Res = ActOnOpenMPSafelenClause(Expr, StartLoc, LParenLoc, EndLoc);
    break;
  case OMPC_collapse:
    Res = ActOnOpenMPCollapseClause(Expr, StartLoc, LParenLoc, EndLoc);
    break;
  case OMPC_default:
  case OMPC_proc_bind:
  case OMPC_schedule:
  case OMPC_private:
  case OMPC_firstprivate:
  case OMPC_lastprivate:
  case OMPC_shared:
  case OMPC_reduction:
  case OMPC_linear:
  case OMPC_aligned:
  case OMPC_copyin:
  case OMPC_copyprivate:
  case OMPC_ordered:
  case OMPC_nowait:
  case OMPC_untied:
  case OMPC_mergeable:
  case OMPC_threadprivate:
  case OMPC_flush:
  case OMPC_read:
  case OMPC_write:
  case OMPC_update:
  case OMPC_capture:
  case OMPC_seq_cst:
  case OMPC_unknown:
    llvm_unreachable("Clause is not allowed.");
  }
  return Res;
}

OMPClause *Sema::ActOnOpenMPIfClause(Expr *Condition, SourceLocation StartLoc,
                                     SourceLocation LParenLoc,
                                     SourceLocation EndLoc) {
  Expr *ValExpr = Condition;
  if (!Condition->isValueDependent() && !Condition->isTypeDependent() &&
      !Condition->isInstantiationDependent() &&
      !Condition->containsUnexpandedParameterPack()) {
    ExprResult Val = ActOnBooleanCondition(DSAStack->getCurScope(),
                                           Condition->getExprLoc(), Condition);
    if (Val.isInvalid())
      return nullptr;

    ValExpr = Val.get();
  }

  return new (Context) OMPIfClause(ValExpr, StartLoc, LParenLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPFinalClause(Expr *Condition,
                                        SourceLocation StartLoc,
                                        SourceLocation LParenLoc,
                                        SourceLocation EndLoc) {
  Expr *ValExpr = Condition;
  if (!Condition->isValueDependent() && !Condition->isTypeDependent() &&
      !Condition->isInstantiationDependent() &&
      !Condition->containsUnexpandedParameterPack()) {
    ExprResult Val = ActOnBooleanCondition(DSAStack->getCurScope(),
                                           Condition->getExprLoc(), Condition);
    if (Val.isInvalid())
      return nullptr;

    ValExpr = Val.get();
  }

  return new (Context) OMPFinalClause(ValExpr, StartLoc, LParenLoc, EndLoc);
}
ExprResult Sema::PerformOpenMPImplicitIntegerConversion(SourceLocation Loc,
                                                        Expr *Op) {
  if (!Op)
    return ExprError();

  class IntConvertDiagnoser : public ICEConvertDiagnoser {
  public:
    IntConvertDiagnoser()
        : ICEConvertDiagnoser(/*AllowScopedEnumerations*/ false, false, true) {}
    SemaDiagnosticBuilder diagnoseNotInt(Sema &S, SourceLocation Loc,
                                         QualType T) override {
      return S.Diag(Loc, diag::err_omp_not_integral) << T;
    }
    SemaDiagnosticBuilder diagnoseIncomplete(Sema &S, SourceLocation Loc,
                                             QualType T) override {
      return S.Diag(Loc, diag::err_omp_incomplete_type) << T;
    }
    SemaDiagnosticBuilder diagnoseExplicitConv(Sema &S, SourceLocation Loc,
                                               QualType T,
                                               QualType ConvTy) override {
      return S.Diag(Loc, diag::err_omp_explicit_conversion) << T << ConvTy;
    }
    SemaDiagnosticBuilder noteExplicitConv(Sema &S, CXXConversionDecl *Conv,
                                           QualType ConvTy) override {
      return S.Diag(Conv->getLocation(), diag::note_omp_conversion_here)
             << ConvTy->isEnumeralType() << ConvTy;
    }
    SemaDiagnosticBuilder diagnoseAmbiguous(Sema &S, SourceLocation Loc,
                                            QualType T) override {
      return S.Diag(Loc, diag::err_omp_ambiguous_conversion) << T;
    }
    SemaDiagnosticBuilder noteAmbiguous(Sema &S, CXXConversionDecl *Conv,
                                        QualType ConvTy) override {
      return S.Diag(Conv->getLocation(), diag::note_omp_conversion_here)
             << ConvTy->isEnumeralType() << ConvTy;
    }
    SemaDiagnosticBuilder diagnoseConversion(Sema &, SourceLocation, QualType,
                                             QualType) override {
      llvm_unreachable("conversion functions are permitted");
    }
  } ConvertDiagnoser;
  return PerformContextualImplicitConversion(Loc, Op, ConvertDiagnoser);
}

OMPClause *Sema::ActOnOpenMPNumThreadsClause(Expr *NumThreads,
                                             SourceLocation StartLoc,
                                             SourceLocation LParenLoc,
                                             SourceLocation EndLoc) {
  Expr *ValExpr = NumThreads;
  if (!NumThreads->isValueDependent() && !NumThreads->isTypeDependent() &&
      !NumThreads->containsUnexpandedParameterPack()) {
    SourceLocation NumThreadsLoc = NumThreads->getLocStart();
    ExprResult Val =
        PerformOpenMPImplicitIntegerConversion(NumThreadsLoc, NumThreads);
    if (Val.isInvalid())
      return nullptr;

    ValExpr = Val.get();

    // OpenMP [2.5, Restrictions]
    //  The num_threads expression must evaluate to a positive integer value.
    llvm::APSInt Result;
    if (ValExpr->isIntegerConstantExpr(Result, Context) && Result.isSigned() &&
        !Result.isStrictlyPositive()) {
      Diag(NumThreadsLoc, diag::err_omp_negative_expression_in_clause)
          << "num_threads" << NumThreads->getSourceRange();
      return nullptr;
    }
  }

  return new (Context)
      OMPNumThreadsClause(ValExpr, StartLoc, LParenLoc, EndLoc);
}

ExprResult Sema::VerifyPositiveIntegerConstantInClause(Expr *E,
                                                       OpenMPClauseKind CKind) {
  if (!E)
    return ExprError();
  if (E->isValueDependent() || E->isTypeDependent() ||
      E->isInstantiationDependent() || E->containsUnexpandedParameterPack())
    return E;
  llvm::APSInt Result;
  ExprResult ICE = VerifyIntegerConstantExpression(E, &Result);
  if (ICE.isInvalid())
    return ExprError();
  if (!Result.isStrictlyPositive()) {
    Diag(E->getExprLoc(), diag::err_omp_negative_expression_in_clause)
        << getOpenMPClauseName(CKind) << E->getSourceRange();
    return ExprError();
  }
  if (CKind == OMPC_aligned && !Result.isPowerOf2()) {
    Diag(E->getExprLoc(), diag::warn_omp_alignment_not_power_of_two)
        << E->getSourceRange();
    return ExprError();
  }
  return ICE;
}

OMPClause *Sema::ActOnOpenMPSafelenClause(Expr *Len, SourceLocation StartLoc,
                                          SourceLocation LParenLoc,
                                          SourceLocation EndLoc) {
  // OpenMP [2.8.1, simd construct, Description]
  // The parameter of the safelen clause must be a constant
  // positive integer expression.
  ExprResult Safelen = VerifyPositiveIntegerConstantInClause(Len, OMPC_safelen);
  if (Safelen.isInvalid())
    return nullptr;
  return new (Context)
      OMPSafelenClause(Safelen.get(), StartLoc, LParenLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPCollapseClause(Expr *NumForLoops,
                                           SourceLocation StartLoc,
                                           SourceLocation LParenLoc,
                                           SourceLocation EndLoc) {
  // OpenMP [2.7.1, loop construct, Description]
  // OpenMP [2.8.1, simd construct, Description]
  // OpenMP [2.9.6, distribute construct, Description]
  // The parameter of the collapse clause must be a constant
  // positive integer expression.
  ExprResult NumForLoopsResult =
      VerifyPositiveIntegerConstantInClause(NumForLoops, OMPC_collapse);
  if (NumForLoopsResult.isInvalid())
    return nullptr;
  return new (Context)
      OMPCollapseClause(NumForLoopsResult.get(), StartLoc, LParenLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPSimpleClause(
    OpenMPClauseKind Kind, unsigned Argument, SourceLocation ArgumentLoc,
    SourceLocation StartLoc, SourceLocation LParenLoc, SourceLocation EndLoc) {
  OMPClause *Res = nullptr;
  switch (Kind) {
  case OMPC_default:
    Res =
        ActOnOpenMPDefaultClause(static_cast<OpenMPDefaultClauseKind>(Argument),
                                 ArgumentLoc, StartLoc, LParenLoc, EndLoc);
    break;
  case OMPC_proc_bind:
    Res = ActOnOpenMPProcBindClause(
        static_cast<OpenMPProcBindClauseKind>(Argument), ArgumentLoc, StartLoc,
        LParenLoc, EndLoc);
    break;
  case OMPC_if:
  case OMPC_final:
  case OMPC_num_threads:
  case OMPC_safelen:
  case OMPC_collapse:
  case OMPC_schedule:
  case OMPC_private:
  case OMPC_firstprivate:
  case OMPC_lastprivate:
  case OMPC_shared:
  case OMPC_reduction:
  case OMPC_linear:
  case OMPC_aligned:
  case OMPC_copyin:
  case OMPC_copyprivate:
  case OMPC_ordered:
  case OMPC_nowait:
  case OMPC_untied:
  case OMPC_mergeable:
  case OMPC_threadprivate:
  case OMPC_flush:
  case OMPC_read:
  case OMPC_write:
  case OMPC_update:
  case OMPC_capture:
  case OMPC_seq_cst:
  case OMPC_unknown:
    llvm_unreachable("Clause is not allowed.");
  }
  return Res;
}

OMPClause *Sema::ActOnOpenMPDefaultClause(OpenMPDefaultClauseKind Kind,
                                          SourceLocation KindKwLoc,
                                          SourceLocation StartLoc,
                                          SourceLocation LParenLoc,
                                          SourceLocation EndLoc) {
  if (Kind == OMPC_DEFAULT_unknown) {
    std::string Values;
    static_assert(OMPC_DEFAULT_unknown > 0,
                  "OMPC_DEFAULT_unknown not greater than 0");
    std::string Sep(", ");
    for (unsigned i = 0; i < OMPC_DEFAULT_unknown; ++i) {
      Values += "'";
      Values += getOpenMPSimpleClauseTypeName(OMPC_default, i);
      Values += "'";
      switch (i) {
      case OMPC_DEFAULT_unknown - 2:
        Values += " or ";
        break;
      case OMPC_DEFAULT_unknown - 1:
        break;
      default:
        Values += Sep;
        break;
      }
    }
    Diag(KindKwLoc, diag::err_omp_unexpected_clause_value)
        << Values << getOpenMPClauseName(OMPC_default);
    return nullptr;
  }
  switch (Kind) {
  case OMPC_DEFAULT_none:
    DSAStack->setDefaultDSANone(KindKwLoc);
    break;
  case OMPC_DEFAULT_shared:
    DSAStack->setDefaultDSAShared(KindKwLoc);
    break;
  case OMPC_DEFAULT_unknown:
    llvm_unreachable("Clause kind is not allowed.");
    break;
  }
  return new (Context)
      OMPDefaultClause(Kind, KindKwLoc, StartLoc, LParenLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPProcBindClause(OpenMPProcBindClauseKind Kind,
                                           SourceLocation KindKwLoc,
                                           SourceLocation StartLoc,
                                           SourceLocation LParenLoc,
                                           SourceLocation EndLoc) {
  if (Kind == OMPC_PROC_BIND_unknown) {
    std::string Values;
    std::string Sep(", ");
    for (unsigned i = 0; i < OMPC_PROC_BIND_unknown; ++i) {
      Values += "'";
      Values += getOpenMPSimpleClauseTypeName(OMPC_proc_bind, i);
      Values += "'";
      switch (i) {
      case OMPC_PROC_BIND_unknown - 2:
        Values += " or ";
        break;
      case OMPC_PROC_BIND_unknown - 1:
        break;
      default:
        Values += Sep;
        break;
      }
    }
    Diag(KindKwLoc, diag::err_omp_unexpected_clause_value)
        << Values << getOpenMPClauseName(OMPC_proc_bind);
    return nullptr;
  }
  return new (Context)
      OMPProcBindClause(Kind, KindKwLoc, StartLoc, LParenLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPSingleExprWithArgClause(
    OpenMPClauseKind Kind, unsigned Argument, Expr *Expr,
    SourceLocation StartLoc, SourceLocation LParenLoc,
    SourceLocation ArgumentLoc, SourceLocation CommaLoc,
    SourceLocation EndLoc) {
  OMPClause *Res = nullptr;
  switch (Kind) {
  case OMPC_schedule:
    Res = ActOnOpenMPScheduleClause(
        static_cast<OpenMPScheduleClauseKind>(Argument), Expr, StartLoc,
        LParenLoc, ArgumentLoc, CommaLoc, EndLoc);
    break;
  case OMPC_if:
  case OMPC_final:
  case OMPC_num_threads:
  case OMPC_safelen:
  case OMPC_collapse:
  case OMPC_default:
  case OMPC_proc_bind:
  case OMPC_private:
  case OMPC_firstprivate:
  case OMPC_lastprivate:
  case OMPC_shared:
  case OMPC_reduction:
  case OMPC_linear:
  case OMPC_aligned:
  case OMPC_copyin:
  case OMPC_copyprivate:
  case OMPC_ordered:
  case OMPC_nowait:
  case OMPC_untied:
  case OMPC_mergeable:
  case OMPC_threadprivate:
  case OMPC_flush:
  case OMPC_read:
  case OMPC_write:
  case OMPC_update:
  case OMPC_capture:
  case OMPC_seq_cst:
  case OMPC_unknown:
    llvm_unreachable("Clause is not allowed.");
  }
  return Res;
}

OMPClause *Sema::ActOnOpenMPScheduleClause(
    OpenMPScheduleClauseKind Kind, Expr *ChunkSize, SourceLocation StartLoc,
    SourceLocation LParenLoc, SourceLocation KindLoc, SourceLocation CommaLoc,
    SourceLocation EndLoc) {
  if (Kind == OMPC_SCHEDULE_unknown) {
    std::string Values;
    std::string Sep(", ");
    for (unsigned i = 0; i < OMPC_SCHEDULE_unknown; ++i) {
      Values += "'";
      Values += getOpenMPSimpleClauseTypeName(OMPC_schedule, i);
      Values += "'";
      switch (i) {
      case OMPC_SCHEDULE_unknown - 2:
        Values += " or ";
        break;
      case OMPC_SCHEDULE_unknown - 1:
        break;
      default:
        Values += Sep;
        break;
      }
    }
    Diag(KindLoc, diag::err_omp_unexpected_clause_value)
        << Values << getOpenMPClauseName(OMPC_schedule);
    return nullptr;
  }
  Expr *ValExpr = ChunkSize;
  if (ChunkSize) {
    if (!ChunkSize->isValueDependent() && !ChunkSize->isTypeDependent() &&
        !ChunkSize->isInstantiationDependent() &&
        !ChunkSize->containsUnexpandedParameterPack()) {
      SourceLocation ChunkSizeLoc = ChunkSize->getLocStart();
      ExprResult Val =
          PerformOpenMPImplicitIntegerConversion(ChunkSizeLoc, ChunkSize);
      if (Val.isInvalid())
        return nullptr;

      ValExpr = Val.get();

      // OpenMP [2.7.1, Restrictions]
      //  chunk_size must be a loop invariant integer expression with a positive
      //  value.
      llvm::APSInt Result;
      if (ValExpr->isIntegerConstantExpr(Result, Context) &&
          Result.isSigned() && !Result.isStrictlyPositive()) {
        Diag(ChunkSizeLoc, diag::err_omp_negative_expression_in_clause)
            << "schedule" << ChunkSize->getSourceRange();
        return nullptr;
      }
    }
  }

  return new (Context) OMPScheduleClause(StartLoc, LParenLoc, KindLoc, CommaLoc,
                                         EndLoc, Kind, ValExpr);
}

OMPClause *Sema::ActOnOpenMPClause(OpenMPClauseKind Kind,
                                   SourceLocation StartLoc,
                                   SourceLocation EndLoc) {
  OMPClause *Res = nullptr;
  switch (Kind) {
  case OMPC_ordered:
    Res = ActOnOpenMPOrderedClause(StartLoc, EndLoc);
    break;
  case OMPC_nowait:
    Res = ActOnOpenMPNowaitClause(StartLoc, EndLoc);
    break;
  case OMPC_untied:
    Res = ActOnOpenMPUntiedClause(StartLoc, EndLoc);
    break;
  case OMPC_mergeable:
    Res = ActOnOpenMPMergeableClause(StartLoc, EndLoc);
    break;
  case OMPC_read:
    Res = ActOnOpenMPReadClause(StartLoc, EndLoc);
    break;
  case OMPC_write:
    Res = ActOnOpenMPWriteClause(StartLoc, EndLoc);
    break;
  case OMPC_update:
    Res = ActOnOpenMPUpdateClause(StartLoc, EndLoc);
    break;
  case OMPC_capture:
    Res = ActOnOpenMPCaptureClause(StartLoc, EndLoc);
    break;
  case OMPC_seq_cst:
    Res = ActOnOpenMPSeqCstClause(StartLoc, EndLoc);
    break;
  case OMPC_if:
  case OMPC_final:
  case OMPC_num_threads:
  case OMPC_safelen:
  case OMPC_collapse:
  case OMPC_schedule:
  case OMPC_private:
  case OMPC_firstprivate:
  case OMPC_lastprivate:
  case OMPC_shared:
  case OMPC_reduction:
  case OMPC_linear:
  case OMPC_aligned:
  case OMPC_copyin:
  case OMPC_copyprivate:
  case OMPC_default:
  case OMPC_proc_bind:
  case OMPC_threadprivate:
  case OMPC_flush:
  case OMPC_unknown:
    llvm_unreachable("Clause is not allowed.");
  }
  return Res;
}

OMPClause *Sema::ActOnOpenMPOrderedClause(SourceLocation StartLoc,
                                          SourceLocation EndLoc) {
  DSAStack->setOrderedRegion();
  return new (Context) OMPOrderedClause(StartLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPNowaitClause(SourceLocation StartLoc,
                                         SourceLocation EndLoc) {
  return new (Context) OMPNowaitClause(StartLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPUntiedClause(SourceLocation StartLoc,
                                         SourceLocation EndLoc) {
  return new (Context) OMPUntiedClause(StartLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPMergeableClause(SourceLocation StartLoc,
                                            SourceLocation EndLoc) {
  return new (Context) OMPMergeableClause(StartLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPReadClause(SourceLocation StartLoc,
                                       SourceLocation EndLoc) {
  return new (Context) OMPReadClause(StartLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPWriteClause(SourceLocation StartLoc,
                                        SourceLocation EndLoc) {
  return new (Context) OMPWriteClause(StartLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPUpdateClause(SourceLocation StartLoc,
                                         SourceLocation EndLoc) {
  return new (Context) OMPUpdateClause(StartLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPCaptureClause(SourceLocation StartLoc,
                                          SourceLocation EndLoc) {
  return new (Context) OMPCaptureClause(StartLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPSeqCstClause(SourceLocation StartLoc,
                                         SourceLocation EndLoc) {
  return new (Context) OMPSeqCstClause(StartLoc, EndLoc);
}

OMPClause *Sema::ActOnOpenMPVarListClause(
    OpenMPClauseKind Kind, ArrayRef<Expr *> VarList, Expr *TailExpr,
    SourceLocation StartLoc, SourceLocation LParenLoc, SourceLocation ColonLoc,
    SourceLocation EndLoc, CXXScopeSpec &ReductionIdScopeSpec,
    const DeclarationNameInfo &ReductionId) {
  OMPClause *Res = nullptr;
  switch (Kind) {
  case OMPC_private:
    Res = ActOnOpenMPPrivateClause(VarList, StartLoc, LParenLoc, EndLoc);
    break;
  case OMPC_firstprivate:
    Res = ActOnOpenMPFirstprivateClause(VarList, StartLoc, LParenLoc, EndLoc);
    break;
  case OMPC_lastprivate:
    Res = ActOnOpenMPLastprivateClause(VarList, StartLoc, LParenLoc, EndLoc);
    break;
  case OMPC_shared:
    Res = ActOnOpenMPSharedClause(VarList, StartLoc, LParenLoc, EndLoc);
    break;
  case OMPC_reduction:
    Res = ActOnOpenMPReductionClause(VarList, StartLoc, LParenLoc, ColonLoc,
                                     EndLoc, ReductionIdScopeSpec, ReductionId);
    break;
  case OMPC_linear:
    Res = ActOnOpenMPLinearClause(VarList, TailExpr, StartLoc, LParenLoc,
                                  ColonLoc, EndLoc);
    break;
  case OMPC_aligned:
    Res = ActOnOpenMPAlignedClause(VarList, TailExpr, StartLoc, LParenLoc,
                                   ColonLoc, EndLoc);
    break;
  case OMPC_copyin:
    Res = ActOnOpenMPCopyinClause(VarList, StartLoc, LParenLoc, EndLoc);
    break;
  case OMPC_copyprivate:
    Res = ActOnOpenMPCopyprivateClause(VarList, StartLoc, LParenLoc, EndLoc);
    break;
  case OMPC_flush:
    Res = ActOnOpenMPFlushClause(VarList, StartLoc, LParenLoc, EndLoc);
    break;
  case OMPC_if:
  case OMPC_final:
  case OMPC_num_threads:
  case OMPC_safelen:
  case OMPC_collapse:
  case OMPC_default:
  case OMPC_proc_bind:
  case OMPC_schedule:
  case OMPC_ordered:
  case OMPC_nowait:
  case OMPC_untied:
  case OMPC_mergeable:
  case OMPC_threadprivate:
  case OMPC_read:
  case OMPC_write:
  case OMPC_update:
  case OMPC_capture:
  case OMPC_seq_cst:
  case OMPC_unknown:
    llvm_unreachable("Clause is not allowed.");
  }
  return Res;
}

OMPClause *Sema::ActOnOpenMPPrivateClause(ArrayRef<Expr *> VarList,
                                          SourceLocation StartLoc,
                                          SourceLocation LParenLoc,
                                          SourceLocation EndLoc) {
  SmallVector<Expr *, 8> Vars;
  SmallVector<Expr *, 8> PrivateCopies;
  for (auto &RefExpr : VarList) {
    assert(RefExpr && "NULL expr in OpenMP private clause.");
    if (isa<DependentScopeDeclRefExpr>(RefExpr)) {
      // It will be analyzed later.
      Vars.push_back(RefExpr);
      PrivateCopies.push_back(nullptr);
      continue;
    }

    SourceLocation ELoc = RefExpr->getExprLoc();
    // OpenMP [2.1, C/C++]
    //  A list item is a variable name.
    // OpenMP  [2.9.3.3, Restrictions, p.1]
    //  A variable that is part of another variable (as an array or
    //  structure element) cannot appear in a private clause.
    DeclRefExpr *DE = dyn_cast_or_null<DeclRefExpr>(RefExpr);
    if (!DE || !isa<VarDecl>(DE->getDecl())) {
      Diag(ELoc, diag::err_omp_expected_var_name) << RefExpr->getSourceRange();
      continue;
    }
    Decl *D = DE->getDecl();
    VarDecl *VD = cast<VarDecl>(D);

    QualType Type = VD->getType();
    if (Type->isDependentType() || Type->isInstantiationDependentType()) {
      // It will be analyzed later.
      Vars.push_back(DE);
      PrivateCopies.push_back(nullptr);
      continue;
    }

    // OpenMP [2.9.3.3, Restrictions, C/C++, p.3]
    //  A variable that appears in a private clause must not have an incomplete
    //  type or a reference type.
    if (RequireCompleteType(ELoc, Type,
                            diag::err_omp_private_incomplete_type)) {
      continue;
    }
    if (Type->isReferenceType()) {
      Diag(ELoc, diag::err_omp_clause_ref_type_arg)
          << getOpenMPClauseName(OMPC_private) << Type;
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }

    // OpenMP [2.9.3.3, Restrictions, C/C++, p.1]
    //  A variable of class type (or array thereof) that appears in a private
    //  clause requires an accessible, unambiguous default constructor for the
    //  class type.
    while (Type->isArrayType()) {
      Type = cast<ArrayType>(Type.getTypePtr())->getElementType();
    }

    // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
    // in a Construct]
    //  Variables with the predetermined data-sharing attributes may not be
    //  listed in data-sharing attributes clauses, except for the cases
    //  listed below. For these exceptions only, listing a predetermined
    //  variable in a data-sharing attribute clause is allowed and overrides
    //  the variable's predetermined data-sharing attributes.
    DSAStackTy::DSAVarData DVar = DSAStack->getTopDSA(VD, false);
    if (DVar.CKind != OMPC_unknown && DVar.CKind != OMPC_private) {
      Diag(ELoc, diag::err_omp_wrong_dsa) << getOpenMPClauseName(DVar.CKind)
                                          << getOpenMPClauseName(OMPC_private);
      ReportOriginalDSA(*this, DSAStack, VD, DVar);
      continue;
    }

    // Generate helper private variable and initialize it with the default
    // value. The address of the original variable is replaced by the address of
    // the new private variable in CodeGen. This new variable is not added to
    // IdResolver, so the code in the OpenMP region uses original variable for
    // proper diagnostics.
    auto VDPrivate =
        VarDecl::Create(Context, CurContext, DE->getLocStart(),
                        DE->getExprLoc(), VD->getIdentifier(), VD->getType(),
                        VD->getTypeSourceInfo(), /*S*/ SC_Auto);
    ActOnUninitializedDecl(VDPrivate, /*TypeMayContainAuto*/ false);
    if (VDPrivate->isInvalidDecl())
      continue;
    CurContext->addDecl(VDPrivate);
    auto VDPrivateRefExpr =
        DeclRefExpr::Create(Context, /*QualifierLoc*/ NestedNameSpecifierLoc(),
                            /*TemplateKWLoc*/ SourceLocation(), VDPrivate,
                            /*RefersToEnclosingVariableOrCapture*/ false,
                            /*NameLoc*/ SourceLocation(), DE->getType(),
                            /*VK*/ VK_LValue);

    DSAStack->addDSA(VD, DE, OMPC_private);
    Vars.push_back(DE);
    PrivateCopies.push_back(VDPrivateRefExpr);
  }

  if (Vars.empty())
    return nullptr;

  return OMPPrivateClause::Create(Context, StartLoc, LParenLoc, EndLoc, Vars,
                                  PrivateCopies);
}

namespace {
class DiagsUninitializedSeveretyRAII {
private:
  DiagnosticsEngine &Diags;
  SourceLocation SavedLoc;
  bool IsIgnored;

public:
  DiagsUninitializedSeveretyRAII(DiagnosticsEngine &Diags, SourceLocation Loc,
                                 bool IsIgnored)
      : Diags(Diags), SavedLoc(Loc), IsIgnored(IsIgnored) {
    if (!IsIgnored) {
      Diags.setSeverity(/*Diag*/ diag::warn_uninit_self_reference_in_init,
                        /*Map*/ diag::Severity::Ignored, Loc);
    }
  }
  ~DiagsUninitializedSeveretyRAII() {
    if (!IsIgnored)
      Diags.popMappings(SavedLoc);
  }
};
}

OMPClause *Sema::ActOnOpenMPFirstprivateClause(ArrayRef<Expr *> VarList,
                                               SourceLocation StartLoc,
                                               SourceLocation LParenLoc,
                                               SourceLocation EndLoc) {
  SmallVector<Expr *, 8> Vars;
  SmallVector<Expr *, 8> PrivateCopies;
  SmallVector<Expr *, 8> Inits;
  bool IsImplicitClause =
      StartLoc.isInvalid() && LParenLoc.isInvalid() && EndLoc.isInvalid();
  auto ImplicitClauseLoc = DSAStack->getConstructLoc();

  for (auto &RefExpr : VarList) {
    assert(RefExpr && "NULL expr in OpenMP firstprivate clause.");
    if (isa<DependentScopeDeclRefExpr>(RefExpr)) {
      // It will be analyzed later.
      Vars.push_back(RefExpr);
      PrivateCopies.push_back(nullptr);
      Inits.push_back(nullptr);
      continue;
    }

    SourceLocation ELoc =
        IsImplicitClause ? ImplicitClauseLoc : RefExpr->getExprLoc();
    // OpenMP [2.1, C/C++]
    //  A list item is a variable name.
    // OpenMP  [2.9.3.3, Restrictions, p.1]
    //  A variable that is part of another variable (as an array or
    //  structure element) cannot appear in a private clause.
    DeclRefExpr *DE = dyn_cast_or_null<DeclRefExpr>(RefExpr);
    if (!DE || !isa<VarDecl>(DE->getDecl())) {
      Diag(ELoc, diag::err_omp_expected_var_name) << RefExpr->getSourceRange();
      continue;
    }
    Decl *D = DE->getDecl();
    VarDecl *VD = cast<VarDecl>(D);

    QualType Type = VD->getType();
    if (Type->isDependentType() || Type->isInstantiationDependentType()) {
      // It will be analyzed later.
      Vars.push_back(DE);
      PrivateCopies.push_back(nullptr);
      Inits.push_back(nullptr);
      continue;
    }

    // OpenMP [2.9.3.3, Restrictions, C/C++, p.3]
    //  A variable that appears in a private clause must not have an incomplete
    //  type or a reference type.
    if (RequireCompleteType(ELoc, Type,
                            diag::err_omp_firstprivate_incomplete_type)) {
      continue;
    }
    if (Type->isReferenceType()) {
      if (IsImplicitClause) {
        Diag(ImplicitClauseLoc,
             diag::err_omp_task_predetermined_firstprivate_ref_type_arg)
            << Type;
        Diag(RefExpr->getExprLoc(), diag::note_used_here);
      } else {
        Diag(ELoc, diag::err_omp_clause_ref_type_arg)
            << getOpenMPClauseName(OMPC_firstprivate) << Type;
      }
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }

    // OpenMP [2.9.3.4, Restrictions, C/C++, p.1]
    //  A variable of class type (or array thereof) that appears in a private
    //  clause requires an accessible, unambiguous copy constructor for the
    //  class type.
    Type = Context.getBaseElementType(Type);

    // If an implicit firstprivate variable found it was checked already.
    if (!IsImplicitClause) {
      DSAStackTy::DSAVarData DVar = DSAStack->getTopDSA(VD, false);
      Type = Type.getNonReferenceType().getCanonicalType();
      bool IsConstant = Type.isConstant(Context);
      Type = Context.getBaseElementType(Type);
      // OpenMP [2.4.13, Data-sharing Attribute Clauses]
      //  A list item that specifies a given variable may not appear in more
      // than one clause on the same directive, except that a variable may be
      //  specified in both firstprivate and lastprivate clauses.
      if (DVar.CKind != OMPC_unknown && DVar.CKind != OMPC_firstprivate &&
          DVar.CKind != OMPC_lastprivate && DVar.RefExpr) {
        Diag(ELoc, diag::err_omp_wrong_dsa)
            << getOpenMPClauseName(DVar.CKind)
            << getOpenMPClauseName(OMPC_firstprivate);
        ReportOriginalDSA(*this, DSAStack, VD, DVar);
        continue;
      }

      // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
      // in a Construct]
      //  Variables with the predetermined data-sharing attributes may not be
      //  listed in data-sharing attributes clauses, except for the cases
      //  listed below. For these exceptions only, listing a predetermined
      //  variable in a data-sharing attribute clause is allowed and overrides
      //  the variable's predetermined data-sharing attributes.
      // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
      // in a Construct, C/C++, p.2]
      //  Variables with const-qualified type having no mutable member may be
      //  listed in a firstprivate clause, even if they are static data members.
      if (!(IsConstant || VD->isStaticDataMember()) && !DVar.RefExpr &&
          DVar.CKind != OMPC_unknown && DVar.CKind != OMPC_shared) {
        Diag(ELoc, diag::err_omp_wrong_dsa)
            << getOpenMPClauseName(DVar.CKind)
            << getOpenMPClauseName(OMPC_firstprivate);
        ReportOriginalDSA(*this, DSAStack, VD, DVar);
        continue;
      }

      OpenMPDirectiveKind CurrDir = DSAStack->getCurrentDirective();
      // OpenMP [2.9.3.4, Restrictions, p.2]
      //  A list item that is private within a parallel region must not appear
      //  in a firstprivate clause on a worksharing construct if any of the
      //  worksharing regions arising from the worksharing construct ever bind
      //  to any of the parallel regions arising from the parallel construct.
      if (isOpenMPWorksharingDirective(CurrDir) &&
          !isOpenMPParallelDirective(CurrDir)) {
        DVar = DSAStack->getImplicitDSA(VD, true);
        if (DVar.CKind != OMPC_shared &&
            (isOpenMPParallelDirective(DVar.DKind) ||
             DVar.DKind == OMPD_unknown)) {
          Diag(ELoc, diag::err_omp_required_access)
              << getOpenMPClauseName(OMPC_firstprivate)
              << getOpenMPClauseName(OMPC_shared);
          ReportOriginalDSA(*this, DSAStack, VD, DVar);
          continue;
        }
      }
      // OpenMP [2.9.3.4, Restrictions, p.3]
      //  A list item that appears in a reduction clause of a parallel construct
      //  must not appear in a firstprivate clause on a worksharing or task
      //  construct if any of the worksharing or task regions arising from the
      //  worksharing or task construct ever bind to any of the parallel regions
      //  arising from the parallel construct.
      // OpenMP [2.9.3.4, Restrictions, p.4]
      //  A list item that appears in a reduction clause in worksharing
      //  construct must not appear in a firstprivate clause in a task construct
      //  encountered during execution of any of the worksharing regions arising
      //  from the worksharing construct.
      if (CurrDir == OMPD_task) {
        DVar =
            DSAStack->hasInnermostDSA(VD, MatchesAnyClause(OMPC_reduction),
                                      [](OpenMPDirectiveKind K) -> bool {
                                        return isOpenMPParallelDirective(K) ||
                                               isOpenMPWorksharingDirective(K);
                                      },
                                      false);
        if (DVar.CKind == OMPC_reduction &&
            (isOpenMPParallelDirective(DVar.DKind) ||
             isOpenMPWorksharingDirective(DVar.DKind))) {
          Diag(ELoc, diag::err_omp_parallel_reduction_in_task_firstprivate)
              << getOpenMPDirectiveName(DVar.DKind);
          ReportOriginalDSA(*this, DSAStack, VD, DVar);
          continue;
        }
      }
    }

    Type = Type.getUnqualifiedType();
    auto VDPrivate = VarDecl::Create(Context, CurContext, DE->getLocStart(),
                                     ELoc, VD->getIdentifier(), VD->getType(),
                                     VD->getTypeSourceInfo(), /*S*/ SC_Auto);
    // Generate helper private variable and initialize it with the value of the
    // original variable. The address of the original variable is replaced by
    // the address of the new private variable in the CodeGen. This new variable
    // is not added to IdResolver, so the code in the OpenMP region uses
    // original variable for proper diagnostics and variable capturing.
    Expr *VDInitRefExpr = nullptr;
    // For arrays generate initializer for single element and replace it by the
    // original array element in CodeGen.
    if (DE->getType()->isArrayType()) {
      auto VDInit = VarDecl::Create(Context, CurContext, DE->getLocStart(),
                                    ELoc, VD->getIdentifier(), Type,
                                    VD->getTypeSourceInfo(), /*S*/ SC_Auto);
      CurContext->addHiddenDecl(VDInit);
      VDInitRefExpr = DeclRefExpr::Create(
          Context, /*QualifierLoc*/ NestedNameSpecifierLoc(),
          /*TemplateKWLoc*/ SourceLocation(), VDInit,
          /*RefersToEnclosingVariableOrCapture*/ true, ELoc, Type,
          /*VK*/ VK_LValue);
      VDInit->setIsUsed();
      auto Init = DefaultLvalueConversion(VDInitRefExpr).get();
      InitializedEntity Entity = InitializedEntity::InitializeVariable(VDInit);
      InitializationKind Kind = InitializationKind::CreateCopy(ELoc, ELoc);

      InitializationSequence InitSeq(*this, Entity, Kind, Init);
      ExprResult Result = InitSeq.Perform(*this, Entity, Kind, Init);
      if (Result.isInvalid())
        VDPrivate->setInvalidDecl();
      else
        VDPrivate->setInit(Result.getAs<Expr>());
    } else {
      AddInitializerToDecl(
          VDPrivate,
          DefaultLvalueConversion(
              DeclRefExpr::Create(Context, NestedNameSpecifierLoc(),
                                  SourceLocation(), DE->getDecl(),
                                  /*RefersToEnclosingVariableOrCapture=*/true,
                                  DE->getExprLoc(), DE->getType(),
                                  /*VK=*/VK_LValue)).get(),
          /*DirectInit=*/false, /*TypeMayContainAuto=*/false);
    }
    if (VDPrivate->isInvalidDecl()) {
      if (IsImplicitClause) {
        Diag(DE->getExprLoc(),
             diag::note_omp_task_predetermined_firstprivate_here);
      }
      continue;
    }
    CurContext->addDecl(VDPrivate);
    auto VDPrivateRefExpr =
        DeclRefExpr::Create(Context, /*QualifierLoc*/ NestedNameSpecifierLoc(),
                            /*TemplateKWLoc*/ SourceLocation(), VDPrivate,
                            /*RefersToEnclosingVariableOrCapture*/ false,
                            DE->getLocStart(), DE->getType(),
                            /*VK*/ VK_LValue);
    DSAStack->addDSA(VD, DE, OMPC_firstprivate);
    Vars.push_back(DE);
    PrivateCopies.push_back(VDPrivateRefExpr);
    Inits.push_back(VDInitRefExpr);
  }

  if (Vars.empty())
    return nullptr;

  return OMPFirstprivateClause::Create(Context, StartLoc, LParenLoc, EndLoc,
                                       Vars, PrivateCopies, Inits);
}

OMPClause *Sema::ActOnOpenMPLastprivateClause(ArrayRef<Expr *> VarList,
                                              SourceLocation StartLoc,
                                              SourceLocation LParenLoc,
                                              SourceLocation EndLoc) {
  SmallVector<Expr *, 8> Vars;
  for (auto &RefExpr : VarList) {
    assert(RefExpr && "NULL expr in OpenMP lastprivate clause.");
    if (isa<DependentScopeDeclRefExpr>(RefExpr)) {
      // It will be analyzed later.
      Vars.push_back(RefExpr);
      continue;
    }

    SourceLocation ELoc = RefExpr->getExprLoc();
    // OpenMP [2.1, C/C++]
    //  A list item is a variable name.
    // OpenMP  [2.14.3.5, Restrictions, p.1]
    //  A variable that is part of another variable (as an array or structure
    //  element) cannot appear in a lastprivate clause.
    DeclRefExpr *DE = dyn_cast_or_null<DeclRefExpr>(RefExpr);
    if (!DE || !isa<VarDecl>(DE->getDecl())) {
      Diag(ELoc, diag::err_omp_expected_var_name) << RefExpr->getSourceRange();
      continue;
    }
    Decl *D = DE->getDecl();
    VarDecl *VD = cast<VarDecl>(D);

    QualType Type = VD->getType();
    if (Type->isDependentType() || Type->isInstantiationDependentType()) {
      // It will be analyzed later.
      Vars.push_back(DE);
      continue;
    }

    // OpenMP [2.14.3.5, Restrictions, C/C++, p.2]
    //  A variable that appears in a lastprivate clause must not have an
    //  incomplete type or a reference type.
    if (RequireCompleteType(ELoc, Type,
                            diag::err_omp_lastprivate_incomplete_type)) {
      continue;
    }
    if (Type->isReferenceType()) {
      Diag(ELoc, diag::err_omp_clause_ref_type_arg)
          << getOpenMPClauseName(OMPC_lastprivate) << Type;
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }

    // OpenMP [2.14.1.1, Data-sharing Attribute Rules for Variables Referenced
    // in a Construct]
    //  Variables with the predetermined data-sharing attributes may not be
    //  listed in data-sharing attributes clauses, except for the cases
    //  listed below.
    DSAStackTy::DSAVarData DVar = DSAStack->getTopDSA(VD, false);
    if (DVar.CKind != OMPC_unknown && DVar.CKind != OMPC_lastprivate &&
        DVar.CKind != OMPC_firstprivate &&
        (DVar.CKind != OMPC_private || DVar.RefExpr != nullptr)) {
      Diag(ELoc, diag::err_omp_wrong_dsa)
          << getOpenMPClauseName(DVar.CKind)
          << getOpenMPClauseName(OMPC_lastprivate);
      ReportOriginalDSA(*this, DSAStack, VD, DVar);
      continue;
    }

    OpenMPDirectiveKind CurrDir = DSAStack->getCurrentDirective();
    // OpenMP [2.14.3.5, Restrictions, p.2]
    // A list item that is private within a parallel region, or that appears in
    // the reduction clause of a parallel construct, must not appear in a
    // lastprivate clause on a worksharing construct if any of the corresponding
    // worksharing regions ever binds to any of the corresponding parallel
    // regions.
    if (isOpenMPWorksharingDirective(CurrDir) &&
        !isOpenMPParallelDirective(CurrDir)) {
      DVar = DSAStack->getImplicitDSA(VD, true);
      if (DVar.CKind != OMPC_shared) {
        Diag(ELoc, diag::err_omp_required_access)
            << getOpenMPClauseName(OMPC_lastprivate)
            << getOpenMPClauseName(OMPC_shared);
        ReportOriginalDSA(*this, DSAStack, VD, DVar);
        continue;
      }
    }
    // OpenMP [2.14.3.5, Restrictions, C++, p.1,2]
    //  A variable of class type (or array thereof) that appears in a
    //  lastprivate clause requires an accessible, unambiguous default
    //  constructor for the class type, unless the list item is also specified
    //  in a firstprivate clause.
    //  A variable of class type (or array thereof) that appears in a
    //  lastprivate clause requires an accessible, unambiguous copy assignment
    //  operator for the class type.
    while (Type.getNonReferenceType()->isArrayType())
      Type = cast<ArrayType>(Type.getNonReferenceType().getTypePtr())
                 ->getElementType();
    CXXRecordDecl *RD = getLangOpts().CPlusPlus
                            ? Type.getNonReferenceType()->getAsCXXRecordDecl()
                            : nullptr;
    // FIXME This code must be replaced by actual copying and destructing of the
    // lastprivate variable.
    if (RD) {
      CXXMethodDecl *MD = LookupCopyingAssignment(RD, 0, false, 0);
      DeclAccessPair FoundDecl = DeclAccessPair::make(MD, MD->getAccess());
      if (MD) {
        if (CheckMemberAccess(ELoc, RD, FoundDecl) == AR_inaccessible ||
            MD->isDeleted()) {
          Diag(ELoc, diag::err_omp_required_method)
              << getOpenMPClauseName(OMPC_lastprivate) << 2;
          bool IsDecl = VD->isThisDeclarationADefinition(Context) ==
                        VarDecl::DeclarationOnly;
          Diag(VD->getLocation(),
               IsDecl ? diag::note_previous_decl : diag::note_defined_here)
              << VD;
          Diag(RD->getLocation(), diag::note_previous_decl) << RD;
          continue;
        }
        MarkFunctionReferenced(ELoc, MD);
        DiagnoseUseOfDecl(MD, ELoc);
      }

      CXXDestructorDecl *DD = RD->getDestructor();
      if (DD) {
        PartialDiagnostic PD =
            PartialDiagnostic(PartialDiagnostic::NullDiagnostic());
        if (CheckDestructorAccess(ELoc, DD, PD) == AR_inaccessible ||
            DD->isDeleted()) {
          Diag(ELoc, diag::err_omp_required_method)
              << getOpenMPClauseName(OMPC_lastprivate) << 4;
          bool IsDecl = VD->isThisDeclarationADefinition(Context) ==
                        VarDecl::DeclarationOnly;
          Diag(VD->getLocation(),
               IsDecl ? diag::note_previous_decl : diag::note_defined_here)
              << VD;
          Diag(RD->getLocation(), diag::note_previous_decl) << RD;
          continue;
        }
        MarkFunctionReferenced(ELoc, DD);
        DiagnoseUseOfDecl(DD, ELoc);
      }
    }

    if (DVar.CKind != OMPC_firstprivate)
      DSAStack->addDSA(VD, DE, OMPC_lastprivate);
    Vars.push_back(DE);
  }

  if (Vars.empty())
    return nullptr;

  return OMPLastprivateClause::Create(Context, StartLoc, LParenLoc, EndLoc,
                                      Vars);
}

OMPClause *Sema::ActOnOpenMPSharedClause(ArrayRef<Expr *> VarList,
                                         SourceLocation StartLoc,
                                         SourceLocation LParenLoc,
                                         SourceLocation EndLoc) {
  SmallVector<Expr *, 8> Vars;
  for (auto &RefExpr : VarList) {
    assert(RefExpr && "NULL expr in OpenMP shared clause.");
    if (isa<DependentScopeDeclRefExpr>(RefExpr)) {
      // It will be analyzed later.
      Vars.push_back(RefExpr);
      continue;
    }

    SourceLocation ELoc = RefExpr->getExprLoc();
    // OpenMP [2.1, C/C++]
    //  A list item is a variable name.
    // OpenMP  [2.14.3.2, Restrictions, p.1]
    //  A variable that is part of another variable (as an array or structure
    //  element) cannot appear in a shared unless it is a static data member
    //  of a C++ class.
    DeclRefExpr *DE = dyn_cast<DeclRefExpr>(RefExpr);
    if (!DE || !isa<VarDecl>(DE->getDecl())) {
      Diag(ELoc, diag::err_omp_expected_var_name) << RefExpr->getSourceRange();
      continue;
    }
    Decl *D = DE->getDecl();
    VarDecl *VD = cast<VarDecl>(D);

    QualType Type = VD->getType();
    if (Type->isDependentType() || Type->isInstantiationDependentType()) {
      // It will be analyzed later.
      Vars.push_back(DE);
      continue;
    }

    // OpenMP [2.9.1.1, Data-sharing Attribute Rules for Variables Referenced
    // in a Construct]
    //  Variables with the predetermined data-sharing attributes may not be
    //  listed in data-sharing attributes clauses, except for the cases
    //  listed below. For these exceptions only, listing a predetermined
    //  variable in a data-sharing attribute clause is allowed and overrides
    //  the variable's predetermined data-sharing attributes.
    DSAStackTy::DSAVarData DVar = DSAStack->getTopDSA(VD, false);
    if (DVar.CKind != OMPC_unknown && DVar.CKind != OMPC_shared &&
        DVar.RefExpr) {
      Diag(ELoc, diag::err_omp_wrong_dsa) << getOpenMPClauseName(DVar.CKind)
                                          << getOpenMPClauseName(OMPC_shared);
      ReportOriginalDSA(*this, DSAStack, VD, DVar);
      continue;
    }

    DSAStack->addDSA(VD, DE, OMPC_shared);
    Vars.push_back(DE);
  }

  if (Vars.empty())
    return nullptr;

  return OMPSharedClause::Create(Context, StartLoc, LParenLoc, EndLoc, Vars);
}

namespace {
class DSARefChecker : public StmtVisitor<DSARefChecker, bool> {
  DSAStackTy *Stack;

public:
  bool VisitDeclRefExpr(DeclRefExpr *E) {
    if (VarDecl *VD = dyn_cast<VarDecl>(E->getDecl())) {
      DSAStackTy::DSAVarData DVar = Stack->getTopDSA(VD, false);
      if (DVar.CKind == OMPC_shared && !DVar.RefExpr)
        return false;
      if (DVar.CKind != OMPC_unknown)
        return true;
      DSAStackTy::DSAVarData DVarPrivate =
          Stack->hasDSA(VD, isOpenMPPrivate, MatchesAlways(), false);
      if (DVarPrivate.CKind != OMPC_unknown)
        return true;
      return false;
    }
    return false;
  }
  bool VisitStmt(Stmt *S) {
    for (auto Child : S->children()) {
      if (Child && Visit(Child))
        return true;
    }
    return false;
  }
  explicit DSARefChecker(DSAStackTy *S) : Stack(S) {}
};
} // namespace

OMPClause *Sema::ActOnOpenMPReductionClause(
    ArrayRef<Expr *> VarList, SourceLocation StartLoc, SourceLocation LParenLoc,
    SourceLocation ColonLoc, SourceLocation EndLoc,
    CXXScopeSpec &ReductionIdScopeSpec,
    const DeclarationNameInfo &ReductionId) {
  // TODO: Allow scope specification search when 'declare reduction' is
  // supported.
  assert(ReductionIdScopeSpec.isEmpty() &&
         "No support for scoped reduction identifiers yet.");

  auto DN = ReductionId.getName();
  auto OOK = DN.getCXXOverloadedOperator();
  BinaryOperatorKind BOK = BO_Comma;

  // OpenMP [2.14.3.6, reduction clause]
  // C
  // reduction-identifier is either an identifier or one of the following
  // operators: +, -, *,  &, |, ^, && and ||
  // C++
  // reduction-identifier is either an id-expression or one of the following
  // operators: +, -, *, &, |, ^, && and ||
  // FIXME: Only 'min' and 'max' identifiers are supported for now.
  switch (OOK) {
  case OO_Plus:
  case OO_Minus:
    BOK = BO_AddAssign;
    break;
  case OO_Star:
    BOK = BO_MulAssign;
    break;
  case OO_Amp:
    BOK = BO_AndAssign;
    break;
  case OO_Pipe:
    BOK = BO_OrAssign;
    break;
  case OO_Caret:
    BOK = BO_XorAssign;
    break;
  case OO_AmpAmp:
    BOK = BO_LAnd;
    break;
  case OO_PipePipe:
    BOK = BO_LOr;
    break;
  default:
    if (auto II = DN.getAsIdentifierInfo()) {
      if (II->isStr("max"))
        BOK = BO_GT;
      else if (II->isStr("min"))
        BOK = BO_LT;
    }
    break;
  }
  SourceRange ReductionIdRange;
  if (ReductionIdScopeSpec.isValid()) {
    ReductionIdRange.setBegin(ReductionIdScopeSpec.getBeginLoc());
  }
  ReductionIdRange.setEnd(ReductionId.getEndLoc());
  if (BOK == BO_Comma) {
    // Not allowed reduction identifier is found.
    Diag(ReductionId.getLocStart(), diag::err_omp_unknown_reduction_identifier)
        << ReductionIdRange;
    return nullptr;
  }

  SmallVector<Expr *, 8> Vars;
  for (auto RefExpr : VarList) {
    assert(RefExpr && "nullptr expr in OpenMP reduction clause.");
    if (isa<DependentScopeDeclRefExpr>(RefExpr)) {
      // It will be analyzed later.
      Vars.push_back(RefExpr);
      continue;
    }

    if (RefExpr->isTypeDependent() || RefExpr->isValueDependent() ||
        RefExpr->isInstantiationDependent() ||
        RefExpr->containsUnexpandedParameterPack()) {
      // It will be analyzed later.
      Vars.push_back(RefExpr);
      continue;
    }

    auto ELoc = RefExpr->getExprLoc();
    auto ERange = RefExpr->getSourceRange();
    // OpenMP [2.1, C/C++]
    //  A list item is a variable or array section, subject to the restrictions
    //  specified in Section 2.4 on page 42 and in each of the sections
    // describing clauses and directives for which a list appears.
    // OpenMP  [2.14.3.3, Restrictions, p.1]
    //  A variable that is part of another variable (as an array or
    //  structure element) cannot appear in a private clause.
    auto DE = dyn_cast<DeclRefExpr>(RefExpr);
    if (!DE || !isa<VarDecl>(DE->getDecl())) {
      Diag(ELoc, diag::err_omp_expected_var_name) << ERange;
      continue;
    }
    auto D = DE->getDecl();
    auto VD = cast<VarDecl>(D);
    auto Type = VD->getType();
    // OpenMP [2.9.3.3, Restrictions, C/C++, p.3]
    //  A variable that appears in a private clause must not have an incomplete
    //  type or a reference type.
    if (RequireCompleteType(ELoc, Type,
                            diag::err_omp_reduction_incomplete_type))
      continue;
    // OpenMP [2.14.3.6, reduction clause, Restrictions]
    // Arrays may not appear in a reduction clause.
    if (Type.getNonReferenceType()->isArrayType()) {
      Diag(ELoc, diag::err_omp_reduction_type_array) << Type << ERange;
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }
    // OpenMP [2.14.3.6, reduction clause, Restrictions]
    // A list item that appears in a reduction clause must not be
    // const-qualified.
    if (Type.getNonReferenceType().isConstant(Context)) {
      Diag(ELoc, diag::err_omp_const_variable)
          << getOpenMPClauseName(OMPC_reduction) << Type << ERange;
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }
    // OpenMP [2.9.3.6, Restrictions, C/C++, p.4]
    //  If a list-item is a reference type then it must bind to the same object
    //  for all threads of the team.
    VarDecl *VDDef = VD->getDefinition();
    if (Type->isReferenceType() && VDDef) {
      DSARefChecker Check(DSAStack);
      if (Check.Visit(VDDef->getInit())) {
        Diag(ELoc, diag::err_omp_reduction_ref_type_arg) << ERange;
        Diag(VDDef->getLocation(), diag::note_defined_here) << VDDef;
        continue;
      }
    }
    // OpenMP [2.14.3.6, reduction clause, Restrictions]
    // The type of a list item that appears in a reduction clause must be valid
    // for the reduction-identifier. For a max or min reduction in C, the type
    // of the list item must be an allowed arithmetic data type: char, int,
    // float, double, or _Bool, possibly modified with long, short, signed, or
    // unsigned. For a max or min reduction in C++, the type of the list item
    // must be an allowed arithmetic data type: char, wchar_t, int, float,
    // double, or bool, possibly modified with long, short, signed, or unsigned.
    if ((BOK == BO_GT || BOK == BO_LT) &&
        !(Type->isScalarType() ||
          (getLangOpts().CPlusPlus && Type->isArithmeticType()))) {
      Diag(ELoc, diag::err_omp_clause_not_arithmetic_type_arg)
          << getLangOpts().CPlusPlus;
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }
    if ((BOK == BO_OrAssign || BOK == BO_AndAssign || BOK == BO_XorAssign) &&
        !getLangOpts().CPlusPlus && Type->isFloatingType()) {
      Diag(ELoc, diag::err_omp_clause_floating_type_arg);
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }
    bool Suppress = getDiagnostics().getSuppressAllDiagnostics();
    getDiagnostics().setSuppressAllDiagnostics(true);
    ExprResult ReductionOp =
        BuildBinOp(DSAStack->getCurScope(), ReductionId.getLocStart(), BOK,
                   RefExpr, RefExpr);
    getDiagnostics().setSuppressAllDiagnostics(Suppress);
    if (ReductionOp.isInvalid()) {
      Diag(ELoc, diag::err_omp_reduction_id_not_compatible) << Type
                                                            << ReductionIdRange;
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }

    // OpenMP [2.14.1.1, Data-sharing Attribute Rules for Variables Referenced
    // in a Construct]
    //  Variables with the predetermined data-sharing attributes may not be
    //  listed in data-sharing attributes clauses, except for the cases
    //  listed below. For these exceptions only, listing a predetermined
    //  variable in a data-sharing attribute clause is allowed and overrides
    //  the variable's predetermined data-sharing attributes.
    // OpenMP [2.14.3.6, Restrictions, p.3]
    //  Any number of reduction clauses can be specified on the directive,
    //  but a list item can appear only once in the reduction clauses for that
    //  directive.
    DSAStackTy::DSAVarData DVar = DSAStack->getTopDSA(VD, false);
    if (DVar.CKind == OMPC_reduction) {
      Diag(ELoc, diag::err_omp_once_referenced)
          << getOpenMPClauseName(OMPC_reduction);
      if (DVar.RefExpr) {
        Diag(DVar.RefExpr->getExprLoc(), diag::note_omp_referenced);
      }
    } else if (DVar.CKind != OMPC_unknown) {
      Diag(ELoc, diag::err_omp_wrong_dsa)
          << getOpenMPClauseName(DVar.CKind)
          << getOpenMPClauseName(OMPC_reduction);
      ReportOriginalDSA(*this, DSAStack, VD, DVar);
      continue;
    }

    // OpenMP [2.14.3.6, Restrictions, p.1]
    //  A list item that appears in a reduction clause of a worksharing
    //  construct must be shared in the parallel regions to which any of the
    //  worksharing regions arising from the worksharing construct bind.
    OpenMPDirectiveKind CurrDir = DSAStack->getCurrentDirective();
    if (isOpenMPWorksharingDirective(CurrDir) &&
        !isOpenMPParallelDirective(CurrDir)) {
      DVar = DSAStack->getImplicitDSA(VD, true);
      if (DVar.CKind != OMPC_shared) {
        Diag(ELoc, diag::err_omp_required_access)
            << getOpenMPClauseName(OMPC_reduction)
            << getOpenMPClauseName(OMPC_shared);
        ReportOriginalDSA(*this, DSAStack, VD, DVar);
        continue;
      }
    }

    CXXRecordDecl *RD = getLangOpts().CPlusPlus
                            ? Type.getNonReferenceType()->getAsCXXRecordDecl()
                            : nullptr;
    // FIXME This code must be replaced by actual constructing/destructing of
    // the reduction variable.
    if (RD) {
      CXXConstructorDecl *CD = LookupDefaultConstructor(RD);
      PartialDiagnostic PD =
          PartialDiagnostic(PartialDiagnostic::NullDiagnostic());
      if (!CD ||
          CheckConstructorAccess(ELoc, CD,
                                 InitializedEntity::InitializeTemporary(Type),
                                 CD->getAccess(), PD) == AR_inaccessible ||
          CD->isDeleted()) {
        Diag(ELoc, diag::err_omp_required_method)
            << getOpenMPClauseName(OMPC_reduction) << 0;
        bool IsDecl = VD->isThisDeclarationADefinition(Context) ==
                      VarDecl::DeclarationOnly;
        Diag(VD->getLocation(),
             IsDecl ? diag::note_previous_decl : diag::note_defined_here)
            << VD;
        Diag(RD->getLocation(), diag::note_previous_decl) << RD;
        continue;
      }
      MarkFunctionReferenced(ELoc, CD);
      DiagnoseUseOfDecl(CD, ELoc);

      CXXDestructorDecl *DD = RD->getDestructor();
      if (DD) {
        if (CheckDestructorAccess(ELoc, DD, PD) == AR_inaccessible ||
            DD->isDeleted()) {
          Diag(ELoc, diag::err_omp_required_method)
              << getOpenMPClauseName(OMPC_reduction) << 4;
          bool IsDecl = VD->isThisDeclarationADefinition(Context) ==
                        VarDecl::DeclarationOnly;
          Diag(VD->getLocation(),
               IsDecl ? diag::note_previous_decl : diag::note_defined_here)
              << VD;
          Diag(RD->getLocation(), diag::note_previous_decl) << RD;
          continue;
        }
        MarkFunctionReferenced(ELoc, DD);
        DiagnoseUseOfDecl(DD, ELoc);
      }
    }

    DSAStack->addDSA(VD, DE, OMPC_reduction);
    Vars.push_back(DE);
  }

  if (Vars.empty())
    return nullptr;

  return OMPReductionClause::Create(
      Context, StartLoc, LParenLoc, ColonLoc, EndLoc, Vars,
      ReductionIdScopeSpec.getWithLocInContext(Context), ReductionId);
}

OMPClause *Sema::ActOnOpenMPLinearClause(ArrayRef<Expr *> VarList, Expr *Step,
                                         SourceLocation StartLoc,
                                         SourceLocation LParenLoc,
                                         SourceLocation ColonLoc,
                                         SourceLocation EndLoc) {
  SmallVector<Expr *, 8> Vars;
  for (auto &RefExpr : VarList) {
    assert(RefExpr && "NULL expr in OpenMP linear clause.");
    if (isa<DependentScopeDeclRefExpr>(RefExpr)) {
      // It will be analyzed later.
      Vars.push_back(RefExpr);
      continue;
    }

    // OpenMP [2.14.3.7, linear clause]
    // A list item that appears in a linear clause is subject to the private
    // clause semantics described in Section 2.14.3.3 on page 159 except as
    // noted. In addition, the value of the new list item on each iteration
    // of the associated loop(s) corresponds to the value of the original
    // list item before entering the construct plus the logical number of
    // the iteration times linear-step.

    SourceLocation ELoc = RefExpr->getExprLoc();
    // OpenMP [2.1, C/C++]
    //  A list item is a variable name.
    // OpenMP  [2.14.3.3, Restrictions, p.1]
    //  A variable that is part of another variable (as an array or
    //  structure element) cannot appear in a private clause.
    DeclRefExpr *DE = dyn_cast<DeclRefExpr>(RefExpr);
    if (!DE || !isa<VarDecl>(DE->getDecl())) {
      Diag(ELoc, diag::err_omp_expected_var_name) << RefExpr->getSourceRange();
      continue;
    }

    VarDecl *VD = cast<VarDecl>(DE->getDecl());

    // OpenMP [2.14.3.7, linear clause]
    //  A list-item cannot appear in more than one linear clause.
    //  A list-item that appears in a linear clause cannot appear in any
    //  other data-sharing attribute clause.
    DSAStackTy::DSAVarData DVar = DSAStack->getTopDSA(VD, false);
    if (DVar.RefExpr) {
      Diag(ELoc, diag::err_omp_wrong_dsa) << getOpenMPClauseName(DVar.CKind)
                                          << getOpenMPClauseName(OMPC_linear);
      ReportOriginalDSA(*this, DSAStack, VD, DVar);
      continue;
    }

    QualType QType = VD->getType();
    if (QType->isDependentType() || QType->isInstantiationDependentType()) {
      // It will be analyzed later.
      Vars.push_back(DE);
      continue;
    }

    // A variable must not have an incomplete type or a reference type.
    if (RequireCompleteType(ELoc, QType,
                            diag::err_omp_linear_incomplete_type)) {
      continue;
    }
    if (QType->isReferenceType()) {
      Diag(ELoc, diag::err_omp_clause_ref_type_arg)
          << getOpenMPClauseName(OMPC_linear) << QType;
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }

    // A list item must not be const-qualified.
    if (QType.isConstant(Context)) {
      Diag(ELoc, diag::err_omp_const_variable)
          << getOpenMPClauseName(OMPC_linear);
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }

    // A list item must be of integral or pointer type.
    QType = QType.getUnqualifiedType().getCanonicalType();
    const Type *Ty = QType.getTypePtrOrNull();
    if (!Ty || (!Ty->isDependentType() && !Ty->isIntegralType(Context) &&
                !Ty->isPointerType())) {
      Diag(ELoc, diag::err_omp_linear_expected_int_or_ptr) << QType;
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }

    DSAStack->addDSA(VD, DE, OMPC_linear);
    Vars.push_back(DE);
  }

  if (Vars.empty())
    return nullptr;

  Expr *StepExpr = Step;
  if (Step && !Step->isValueDependent() && !Step->isTypeDependent() &&
      !Step->isInstantiationDependent() &&
      !Step->containsUnexpandedParameterPack()) {
    SourceLocation StepLoc = Step->getLocStart();
    ExprResult Val = PerformOpenMPImplicitIntegerConversion(StepLoc, Step);
    if (Val.isInvalid())
      return nullptr;
    StepExpr = Val.get();

    // Warn about zero linear step (it would be probably better specified as
    // making corresponding variables 'const').
    llvm::APSInt Result;
    if (StepExpr->isIntegerConstantExpr(Result, Context) &&
        !Result.isNegative() && !Result.isStrictlyPositive())
      Diag(StepLoc, diag::warn_omp_linear_step_zero) << Vars[0]
                                                     << (Vars.size() > 1);
  }

  return OMPLinearClause::Create(Context, StartLoc, LParenLoc, ColonLoc, EndLoc,
                                 Vars, StepExpr);
}

OMPClause *Sema::ActOnOpenMPAlignedClause(
    ArrayRef<Expr *> VarList, Expr *Alignment, SourceLocation StartLoc,
    SourceLocation LParenLoc, SourceLocation ColonLoc, SourceLocation EndLoc) {

  SmallVector<Expr *, 8> Vars;
  for (auto &RefExpr : VarList) {
    assert(RefExpr && "NULL expr in OpenMP aligned clause.");
    if (isa<DependentScopeDeclRefExpr>(RefExpr)) {
      // It will be analyzed later.
      Vars.push_back(RefExpr);
      continue;
    }

    SourceLocation ELoc = RefExpr->getExprLoc();
    // OpenMP [2.1, C/C++]
    //  A list item is a variable name.
    DeclRefExpr *DE = dyn_cast<DeclRefExpr>(RefExpr);
    if (!DE || !isa<VarDecl>(DE->getDecl())) {
      Diag(ELoc, diag::err_omp_expected_var_name) << RefExpr->getSourceRange();
      continue;
    }

    VarDecl *VD = cast<VarDecl>(DE->getDecl());

    // OpenMP  [2.8.1, simd construct, Restrictions]
    // The type of list items appearing in the aligned clause must be
    // array, pointer, reference to array, or reference to pointer.
    QualType QType = DE->getType()
                         .getNonReferenceType()
                         .getUnqualifiedType()
                         .getCanonicalType();
    const Type *Ty = QType.getTypePtrOrNull();
    if (!Ty || (!Ty->isDependentType() && !Ty->isArrayType() &&
                !Ty->isPointerType())) {
      Diag(ELoc, diag::err_omp_aligned_expected_array_or_ptr)
          << QType << getLangOpts().CPlusPlus << RefExpr->getSourceRange();
      bool IsDecl =
          VD->isThisDeclarationADefinition(Context) == VarDecl::DeclarationOnly;
      Diag(VD->getLocation(),
           IsDecl ? diag::note_previous_decl : diag::note_defined_here)
          << VD;
      continue;
    }

    // OpenMP  [2.8.1, simd construct, Restrictions]
    // A list-item cannot appear in more than one aligned clause.
    if (DeclRefExpr *PrevRef = DSAStack->addUniqueAligned(VD, DE)) {
      Diag(ELoc, diag::err_omp_aligned_twice) << RefExpr->getSourceRange();
      Diag(PrevRef->getExprLoc(), diag::note_omp_explicit_dsa)
          << getOpenMPClauseName(OMPC_aligned);
      continue;
    }

    Vars.push_back(DE);
  }

  // OpenMP [2.8.1, simd construct, Description]
  // The parameter of the aligned clause, alignment, must be a constant
  // positive integer expression.
  // If no optional parameter is specified, implementation-defined default
  // alignments for SIMD instructions on the target platforms are assumed.
  if (Alignment != nullptr) {
    ExprResult AlignResult =
        VerifyPositiveIntegerConstantInClause(Alignment, OMPC_aligned);
    if (AlignResult.isInvalid())
      return nullptr;
    Alignment = AlignResult.get();
  }
  if (Vars.empty())
    return nullptr;

  return OMPAlignedClause::Create(Context, StartLoc, LParenLoc, ColonLoc,
                                  EndLoc, Vars, Alignment);
}

OMPClause *Sema::ActOnOpenMPCopyinClause(ArrayRef<Expr *> VarList,
                                         SourceLocation StartLoc,
                                         SourceLocation LParenLoc,
                                         SourceLocation EndLoc) {
  SmallVector<Expr *, 8> Vars;
  for (auto &RefExpr : VarList) {
    assert(RefExpr && "NULL expr in OpenMP copyin clause.");
    if (isa<DependentScopeDeclRefExpr>(RefExpr)) {
      // It will be analyzed later.
      Vars.push_back(RefExpr);
      continue;
    }

    SourceLocation ELoc = RefExpr->getExprLoc();
    // OpenMP [2.1, C/C++]
    //  A list item is a variable name.
    // OpenMP  [2.14.4.1, Restrictions, p.1]
    //  A list item that appears in a copyin clause must be threadprivate.
    DeclRefExpr *DE = dyn_cast<DeclRefExpr>(RefExpr);
    if (!DE || !isa<VarDecl>(DE->getDecl())) {
      Diag(ELoc, diag::err_omp_expected_var_name) << RefExpr->getSourceRange();
      continue;
    }

    Decl *D = DE->getDecl();
    VarDecl *VD = cast<VarDecl>(D);

    QualType Type = VD->getType();
    if (Type->isDependentType() || Type->isInstantiationDependentType()) {
      // It will be analyzed later.
      Vars.push_back(DE);
      continue;
    }

    // OpenMP [2.14.4.1, Restrictions, C/C++, p.1]
    //  A list item that appears in a copyin clause must be threadprivate.
    if (!DSAStack->isThreadPrivate(VD)) {
      Diag(ELoc, diag::err_omp_required_access)
          << getOpenMPClauseName(OMPC_copyin)
          << getOpenMPDirectiveName(OMPD_threadprivate);
      continue;
    }

    // OpenMP [2.14.4.1, Restrictions, C/C++, p.2]
    //  A variable of class type (or array thereof) that appears in a
    //  copyin clause requires an accessible, unambiguous copy assignment
    //  operator for the class type.
    Type = Context.getBaseElementType(Type);
    CXXRecordDecl *RD =
        getLangOpts().CPlusPlus ? Type->getAsCXXRecordDecl() : nullptr;
    // FIXME This code must be replaced by actual assignment of the
    // threadprivate variable.
    if (RD) {
      CXXMethodDecl *MD = LookupCopyingAssignment(RD, 0, false, 0);
      DeclAccessPair FoundDecl = DeclAccessPair::make(MD, MD->getAccess());
      if (MD) {
        if (CheckMemberAccess(ELoc, RD, FoundDecl) == AR_inaccessible ||
            MD->isDeleted()) {
          Diag(ELoc, diag::err_omp_required_method)
              << getOpenMPClauseName(OMPC_copyin) << 2;
          bool IsDecl = VD->isThisDeclarationADefinition(Context) ==
                        VarDecl::DeclarationOnly;
          Diag(VD->getLocation(),
               IsDecl ? diag::note_previous_decl : diag::note_defined_here)
              << VD;
          Diag(RD->getLocation(), diag::note_previous_decl) << RD;
          continue;
        }
        MarkFunctionReferenced(ELoc, MD);
        DiagnoseUseOfDecl(MD, ELoc);
      }
    }

    DSAStack->addDSA(VD, DE, OMPC_copyin);
    Vars.push_back(DE);
  }

  if (Vars.empty())
    return nullptr;

  return OMPCopyinClause::Create(Context, StartLoc, LParenLoc, EndLoc, Vars);
}

OMPClause *Sema::ActOnOpenMPCopyprivateClause(ArrayRef<Expr *> VarList,
                                              SourceLocation StartLoc,
                                              SourceLocation LParenLoc,
                                              SourceLocation EndLoc) {
  SmallVector<Expr *, 8> Vars;
  for (auto &RefExpr : VarList) {
    assert(RefExpr && "NULL expr in OpenMP copyprivate clause.");
    if (isa<DependentScopeDeclRefExpr>(RefExpr)) {
      // It will be analyzed later.
      Vars.push_back(RefExpr);
      continue;
    }

    SourceLocation ELoc = RefExpr->getExprLoc();
    // OpenMP [2.1, C/C++]
    //  A list item is a variable name.
    // OpenMP  [2.14.4.1, Restrictions, p.1]
    //  A list item that appears in a copyin clause must be threadprivate.
    DeclRefExpr *DE = dyn_cast<DeclRefExpr>(RefExpr);
    if (!DE || !isa<VarDecl>(DE->getDecl())) {
      Diag(ELoc, diag::err_omp_expected_var_name) << RefExpr->getSourceRange();
      continue;
    }

    Decl *D = DE->getDecl();
    VarDecl *VD = cast<VarDecl>(D);

    QualType Type = VD->getType();
    if (Type->isDependentType() || Type->isInstantiationDependentType()) {
      // It will be analyzed later.
      Vars.push_back(DE);
      continue;
    }

    // OpenMP [2.14.4.2, Restrictions, p.2]
    //  A list item that appears in a copyprivate clause may not appear in a
    //  private or firstprivate clause on the single construct.
    if (!DSAStack->isThreadPrivate(VD)) {
      auto DVar = DSAStack->getTopDSA(VD, false);
      if (DVar.CKind != OMPC_copyprivate && DVar.CKind != OMPC_unknown &&
          !(DVar.CKind == OMPC_private && !DVar.RefExpr)) {
        Diag(ELoc, diag::err_omp_wrong_dsa)
            << getOpenMPClauseName(DVar.CKind)
            << getOpenMPClauseName(OMPC_copyprivate);
        ReportOriginalDSA(*this, DSAStack, VD, DVar);
        continue;
      }

      // OpenMP [2.11.4.2, Restrictions, p.1]
      //  All list items that appear in a copyprivate clause must be either
      //  threadprivate or private in the enclosing context.
      if (DVar.CKind == OMPC_unknown) {
        DVar = DSAStack->getImplicitDSA(VD, false);
        if (DVar.CKind == OMPC_shared) {
          Diag(ELoc, diag::err_omp_required_access)
              << getOpenMPClauseName(OMPC_copyprivate)
              << "threadprivate or private in the enclosing context";
          ReportOriginalDSA(*this, DSAStack, VD, DVar);
          continue;
        }
      }
    }

    // OpenMP [2.14.4.1, Restrictions, C/C++, p.2]
    //  A variable of class type (or array thereof) that appears in a
    //  copyin clause requires an accessible, unambiguous copy assignment
    //  operator for the class type.
    Type = Context.getBaseElementType(Type);
    CXXRecordDecl *RD =
        getLangOpts().CPlusPlus ? Type->getAsCXXRecordDecl() : nullptr;
    // FIXME This code must be replaced by actual assignment of the
    // threadprivate variable.
    if (RD) {
      CXXMethodDecl *MD = LookupCopyingAssignment(RD, 0, false, 0);
      DeclAccessPair FoundDecl = DeclAccessPair::make(MD, MD->getAccess());
      if (MD) {
        if (CheckMemberAccess(ELoc, RD, FoundDecl) == AR_inaccessible ||
            MD->isDeleted()) {
          Diag(ELoc, diag::err_omp_required_method)
              << getOpenMPClauseName(OMPC_copyprivate) << 2;
          bool IsDecl = VD->isThisDeclarationADefinition(Context) ==
                        VarDecl::DeclarationOnly;
          Diag(VD->getLocation(),
               IsDecl ? diag::note_previous_decl : diag::note_defined_here)
              << VD;
          Diag(RD->getLocation(), diag::note_previous_decl) << RD;
          continue;
        }
        MarkFunctionReferenced(ELoc, MD);
        DiagnoseUseOfDecl(MD, ELoc);
      }
    }

    // No need to mark vars as copyprivate, they are already threadprivate or
    // implicitly private.
    Vars.push_back(DE);
  }

  if (Vars.empty())
    return nullptr;

  return OMPCopyprivateClause::Create(Context, StartLoc, LParenLoc, EndLoc, Vars);
}

OMPClause *Sema::ActOnOpenMPFlushClause(ArrayRef<Expr *> VarList,
                                        SourceLocation StartLoc,
                                        SourceLocation LParenLoc,
                                        SourceLocation EndLoc) {
  if (VarList.empty())
    return nullptr;

  return OMPFlushClause::Create(Context, StartLoc, LParenLoc, EndLoc, VarList);
}

