//===--- TypeCheckCompletionCallback.cpp ----------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "swift/IDE/TypeCheckCompletionCallback.h"
#include "swift/IDE/CompletionLookup.h"
#include "swift/Sema/CompletionContextFinder.h"
#include "swift/Sema/ConstraintSystem.h"
#include "swift/Sema/IDETypeChecking.h"

using namespace swift;
using namespace swift::ide;
using namespace swift::constraints;

void TypeCheckCompletionCallback::fallbackTypeCheck(DeclContext *DC) {
  assert(!GotCallback);

  CompletionContextFinder finder(DC);
  if (!finder.hasCompletionExpr())
    return;

  auto fallback = finder.getFallbackCompletionExpr();
  if (!fallback)
    return;

  SolutionApplicationTarget completionTarget(fallback->E, fallback->DC,
                                             CTP_Unused, Type(),
                                             /*isDiscared=*/true);
  typeCheckForCodeCompletion(completionTarget, /*needsPrecheck=*/true,
                             [&](const Solution &S) { sawSolution(S); });
}

// MARK: - Utility functions for subclasses of TypeCheckCompletionCallback

Type swift::ide::getTypeForCompletion(const constraints::Solution &S,
                                      ASTNode Node) {
  if (!S.hasType(Node)) {
    assert(false && "Expression wasn't type checked?");
    return nullptr;
  }

  auto &CS = S.getConstraintSystem();

  Type Result;

  // To aid code completion, we need to attempt to convert type placeholders
  // back into underlying generic parameters if possible, since type
  // of the code completion expression is used as "expected" (or contextual)
  // type so it's helpful to know what requirements it has to filter
  // the list of possible member candidates e.g.
  //
  // \code
  // func test<T: P>(_: [T]) {}
  //
  // test(42.#^MEMBERS^#)
  // \code
  //
  // It's impossible to resolve `T` in this case but code completion
  // expression should still have a type of `[T]` instead of `[<<hole>>]`
  // because it helps to produce correct contextual member list based on
  // a conformance requirement associated with generic parameter `T`.
  if (isExpr<CodeCompletionExpr>(Node)) {
    auto completionTy = S.getType(Node).transform([&](Type type) -> Type {
      if (auto *typeVar = type->getAs<TypeVariableType>())
        return S.getFixedType(typeVar);
      return type;
    });

    Result = S.simplifyType(completionTy.transform([&](Type type) {
      if (auto *placeholder = type->getAs<PlaceholderType>()) {
        if (auto *typeVar =
                placeholder->getOriginator().dyn_cast<TypeVariableType *>()) {
          if (auto *GP = typeVar->getImpl().getGenericParameter()) {
            // Code completion depends on generic parameter type being
            // represented in terms of `ArchetypeType` since it's easy
            // to extract protocol requirements from it.
            if (auto *GPD = GP->getDecl())
              return GPD->getInnermostDeclContext()->mapTypeIntoContext(GP);
          }
        }

        return Type(CS.getASTContext().TheUnresolvedType);
      }

      return type;
    }));
  } else {
    Result = S.getResolvedType(Node);
  }

  if (!Result || Result->is<UnresolvedType>()) {
    Result = CS.getContextualType(Node, /*forConstraint=*/false);
  }
  if (Result && Result->is<UnresolvedType>()) {
    Result = Type();
  }
  return Result;
}

/// If the code completion expression \p E occurs in a pattern matching
/// position, we have an AST that looks like this.
/// \code
/// (binary_expr implicit type='$T3'
///   (overloaded_decl_ref_expr function_ref=compound decls=[
///     Swift.(file).~=,
///     Swift.(file).Optional extension.~=])
///   (argument_list implicit
///     (argument
///       (code_completion_expr implicit type='$T1'))
///     (argument
///       (declref_expr implicit decl=swift_ide_test.(file).foo(x:).$match))))
/// \endcode
/// If the code completion expression occurs in such an AST, return the
/// declaration of the \c $match variable, otherwise return \c nullptr.
static VarDecl *getMatchVarIfInPatternMatch(Expr *E, ConstraintSystem &CS) {
  auto &Context = CS.getASTContext();

  auto *Binary = dyn_cast_or_null<BinaryExpr>(CS.getParentExpr(E));
  if (!Binary || !Binary->isImplicit() || Binary->getLHS() != E) {
    return nullptr;
  }

  auto CalledOperator = Binary->getFn();
  if (!isPatternMatchingOperator(CalledOperator)) {
    return nullptr;
  }

  auto MatchArg = dyn_cast_or_null<DeclRefExpr>(Binary->getRHS());
  if (!MatchArg || !MatchArg->isImplicit()) {
    return nullptr;
  }

  auto MatchVar = MatchArg->getDecl();
  if (MatchVar && MatchVar->isImplicit() &&
      MatchVar->getBaseName() == Context.Id_PatternMatchVar) {
    return dyn_cast<VarDecl>(MatchVar);
  } else {
    return nullptr;
  }
}

Type swift::ide::getPatternMatchType(const constraints::Solution &S, Expr *E) {
  if (auto MatchVar = getMatchVarIfInPatternMatch(E, S.getConstraintSystem())) {
    Type MatchVarType;
    // If the MatchVar has an explicit type, it's not part of the solution. But
    // we can look it up in the constraint system directly.
    if (auto T = S.getConstraintSystem().getVarType(MatchVar)) {
      MatchVarType = T;
    } else {
      MatchVarType = getTypeForCompletion(S, MatchVar);
    }
    if (MatchVarType) {
      return MatchVarType;
    }
  }
  return nullptr;
}

bool swift::ide::isImplicitSingleExpressionReturn(ConstraintSystem &CS,
                                                  Expr *CompletionExpr) {
  Expr *ParentExpr = CS.getParentExpr(CompletionExpr);
  if (!ParentExpr)
    return CS.getContextualTypePurpose(CompletionExpr) == CTP_ReturnSingleExpr;

  if (auto *ParentCE = dyn_cast<ClosureExpr>(ParentExpr)) {
    if (ParentCE->hasSingleExpressionBody() &&
        ParentCE->getSingleExpressionBody() == CompletionExpr) {
      ASTNode Last = ParentCE->getBody()->getLastElement();
      return !Last.isStmt(StmtKind::Return) || Last.isImplicit();
    }
  }
  return false;
}

bool swift::ide::isContextAsync(const constraints::Solution &S,
                                DeclContext *DC) {
  // We are in an async context if
  //  - the decl context is async
  if (S.getConstraintSystem().isAsynchronousContext(DC)) {
    return true;
  }

  //  - the decl context is sync but it's used in a context that expectes an
  //    async function. This happens if the code completion token is in a
  //    closure that doesn't contain any async calles. Thus the closure is
  //    type-checked as non-async, but it might get converted to an async
  //    closure based on its contextual type
  auto target = S.solutionApplicationTargets.find(dyn_cast<ClosureExpr>(DC));
  if (target != S.solutionApplicationTargets.end()) {
    if (auto ContextTy = target->second.getClosureContextualType()) {
      if (auto ContextFuncTy =
              S.simplifyType(ContextTy)->getAs<AnyFunctionType>()) {
        return ContextFuncTy->isAsync();
      }
    }
  }

  //  - we did not record any information about async-ness of the context in the
  //    solution, but the type information recorded AST declares the context as
  //    async.
  return canDeclContextHandleAsync(DC);
}

bool swift::ide::nullableTypesEqual(Type LHS, Type RHS) {
  if (LHS.isNull() && RHS.isNull()) {
    return true;
  } else if (LHS.isNull() || RHS.isNull()) {
    // One type is null but the other is not.
    return false;
  } else {
    return LHS->isEqual(RHS);
  }
}
