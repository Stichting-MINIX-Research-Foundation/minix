//===--- EvaluatedExprVisitor.h - Evaluated expression visitor --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the EvaluatedExprVisitor class template, which visits
//  the potentially-evaluated subexpressions of a potentially-evaluated
//  expression.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_AST_EVALUATEDEXPRVISITOR_H
#define LLVM_CLANG_AST_EVALUATEDEXPRVISITOR_H

#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/StmtVisitor.h"

namespace clang {
  
class ASTContext;
  
/// \brief Given a potentially-evaluated expression, this visitor visits all
/// of its potentially-evaluated subexpressions, recursively.
template<typename ImplClass>
class EvaluatedExprVisitor : public StmtVisitor<ImplClass> {
  ASTContext &Context;
  
public:
  explicit EvaluatedExprVisitor(ASTContext &Context) : Context(Context) { }
  
  // Expressions that have no potentially-evaluated subexpressions (but may have
  // other sub-expressions).
  void VisitDeclRefExpr(DeclRefExpr *E) { }
  void VisitOffsetOfExpr(OffsetOfExpr *E) { }
  void VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *E) { }
  void VisitExpressionTraitExpr(ExpressionTraitExpr *E) { }
  void VisitBlockExpr(BlockExpr *E) { }
  void VisitCXXUuidofExpr(CXXUuidofExpr *E) { }  
  void VisitCXXNoexceptExpr(CXXNoexceptExpr *E) { }
  
  void VisitMemberExpr(MemberExpr *E) {
    // Only the base matters.
    return this->Visit(E->getBase());
  }
  
  void VisitChooseExpr(ChooseExpr *E) {
    // Don't visit either child expression if the condition is dependent.
    if (E->getCond()->isValueDependent())
      return;
    // Only the selected subexpression matters; the other one is not evaluated.
    return this->Visit(E->getChosenSubExpr());
  }

  void VisitGenericSelectionExpr(GenericSelectionExpr *E) {
    // The controlling expression of a generic selection is not evaluated.

    // Don't visit either child expression if the condition is type-dependent.
    if (E->isResultDependent())
      return;
    // Only the selected subexpression matters; the other subexpressions and the
    // controlling expression are not evaluated.
    return this->Visit(E->getResultExpr());
  }

  void VisitDesignatedInitExpr(DesignatedInitExpr *E) {
    // Only the actual initializer matters; the designators are all constant
    // expressions.
    return this->Visit(E->getInit());
  }

  void VisitCXXTypeidExpr(CXXTypeidExpr *E) {
    if (E->isPotentiallyEvaluated())
      return this->Visit(E->getExprOperand());
  }

  void VisitCallExpr(CallExpr *CE) {
    if (!CE->isUnevaluatedBuiltinCall(Context))
      return static_cast<ImplClass*>(this)->VisitExpr(CE);
  }

  void VisitLambdaExpr(LambdaExpr *LE) {
    // Only visit the capture initializers, and not the body.
    for (LambdaExpr::capture_init_iterator I = LE->capture_init_begin(),
                                           E = LE->capture_init_end();
         I != E; ++I)
      if (*I)
        this->Visit(*I);
  }

  /// \brief The basis case walks all of the children of the statement or
  /// expression, assuming they are all potentially evaluated.
  void VisitStmt(Stmt *S) {
    for (Stmt::child_range C = S->children(); C; ++C)
      if (*C)
        this->Visit(*C);
  }
};

}

#endif // LLVM_CLANG_AST_EVALUATEDEXPRVISITOR_H
