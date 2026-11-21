#include "ObservationCostCheck.h"

#include <clang/AST/Expr.h>
#include <clang/ASTMatchers/ASTMatchers.h>

using namespace clang::ast_matchers;

namespace clang::tidy::cata
{

void StalkerLoopReaderCheck::registerMatchers( MatchFinder *Finder )
{
    const auto allMonsters = cxxMemberCallExpr( callee( cxxMethodDecl(
            hasName( "::game::all_monsters" ) ) ) );
    const auto monsterLoop = cxxForRangeStmt( hasRangeInit(
            expr( anyOf( allMonsters, hasDescendant( allMonsters ) ) ) ) );
    Finder->addMatcher( callExpr( callee( functionDecl(
            hasName( "::writhing_stalker::can_observe_actor" ) ) ),
                                  argumentCountIs( 2 ), hasAncestor( monsterLoop ) ).bind( "observation" ), this );
}

void StalkerLoopReaderCheck::check( const MatchFinder::MatchResult &Result )
{
    const auto *call = Result.Nodes.getNodeAs<CallExpr>( "observation" );
    diag( call->getExprLoc(), "this observation creates a site_reader inside the monster loop. "
                              "Consider sharing a reader while the lighting and map are stable" );
}

void HordeLightSampleOrderCheck::registerMatchers( MatchFinder *Finder )
{
    const auto visibilityCall = cxxOperatorCallExpr( hasOverloadedOperatorName( "()" ),
        hasArgument( 0, ignoringParenImpCasts( declRefExpr(
                to( parmVarDecl( hasName( "visible" ) ) ) ) ) ) ).bind( "visibility" );
    const auto storedSample = memberExpr( member( fieldDecl(
            hasName( "::horde_entity::light_sample_id" ) ) ) );
    const auto incomingSample = declRefExpr( to( parmVarDecl( hasName( "sample_id" ) ) ) );
    const auto sameSample = binaryOperation( hasOperatorName( "==" ),
        hasOperands( ignoringParenImpCasts( storedSample ), ignoringParenImpCasts( incomingSample ) ) );
    Finder->addMatcher( binaryOperator( hasOperatorName( "||" ),
                                        hasLHS( expr( anyOf( visibilityCall, hasDescendant( visibilityCall ) ) ) ),
                                        hasRHS( ignoringParenImpCasts( sameSample ) ), hasAncestor( cxxMethodDecl(
                                                hasName( "::horde_map::attract_entities_to_light" ) ) ) ), this );
}

void HordeLightSampleOrderCheck::check( const MatchFinder::MatchResult &Result )
{
    const auto *call = Result.Nodes.getNodeAs<CallExpr>( "visibility" );
    diag( call->getExprLoc(), "visibility is evaluated before the duplicate light sample check. "
                              "Check the sample ID first to skip the sight test for known samples" );
}

} // namespace clang::tidy::cata
