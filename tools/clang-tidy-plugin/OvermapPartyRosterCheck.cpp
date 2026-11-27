#include "OvermapPartyRosterCheck.h"

#include <clang/AST/Expr.h>
#include <clang/ASTMatchers/ASTMatchers.h>

using namespace clang::ast_matchers;

namespace clang::tidy::cata
{

void OvermapPartyRosterCheck::registerMatchers( MatchFinder *Finder )
{
    const auto source = isExpansionInFileMatching(
                            R"((^|[/\])(bandit_[a-z_]+|do_turn|game_io|mapbuffer|overmap_[a-z_]+|physical_navigation|migo_[a-z_]+|predator_[a-z_]+|zombie_rider_[a-z_]+|overmap-party-roster)\.(cpp|h)$)" );
    // Issuing missions can choose a pair. The consumers below are responsible
    // for the issued IDs and physical custody, across faction implementations.
    const auto consumer = functionDecl(
                              anyOf( matchesName(
                                         "(handoff|dematerializ|admission|boundary|crossing|arrival|settle|reconcil|restore|deserialize|serialize|travel|transit|transfer|withdraw|terminal_at_home)" ),
                                     hasName( "::migo_population::discover" ) ),
                              unless( matchesName(
                                          "(::select_routine_scout_pair|::routine_scout_policy|::plan_structural_bounty_outing|_for_test)$" ) ) );
    const auto rosterName = matchesName(
                                "(^|::)(members|member_ids|member_reads|raw_member_ids|casualties|casualty_ids|survivors|staged|placements|actors|actor_ids|rider_ids)$" );
    const auto roster = ignoringParenImpCasts( expr( anyOf(
            memberExpr( member( fieldDecl( rosterName ) ) ),
            declRefExpr( to( varDecl( rosterName ) ) ) ) ) );
    const auto rosterSize = ignoringParenImpCasts( cxxMemberCallExpr(
            callee( cxxMethodDecl( hasName( "size" ) ) ), on( roster ) ).bind( "size" ) );
    Finder->addMatcher( binaryOperator( source, hasAncestor( consumer ),
                                        hasAnyOperatorName( "==", "!=", ">", ">=", "<", "<=" ),
                                        hasEitherOperand( rosterSize ), hasEitherOperand( ignoringParenImpCasts(
                                                integerLiteral( anyOf( equals( 2 ), equals( 3 ) ) ).bind( "bound" ) ) )
                                      ).bind( "compare" ), this );

    const auto twoSlots = classTemplateSpecializationDecl( hasName( "::std::array" ),
        hasTemplateArgument( 1, equalsIntegralValue( "2" ) ) );
    const auto slotType = hasType( hasUnqualifiedDesugaredType(
                                       recordType( hasDeclaration( twoSlots ) ) ) );
    Finder->addMatcher( varDecl( source, rosterName, hasAncestor( consumer ),
                                 slotType
                               ).bind( "slots" ), this );
    Finder->addMatcher( fieldDecl( source, rosterName, slotType,
                                   hasParent( recordDecl( matchesName(
                                           "(operation|handoff|transit|admission|roster|rider_band_registry)" ) ) )
                                 ).bind( "slots" ), this );
}

void OvermapPartyRosterCheck::check( const MatchFinder::MatchResult &Result )
{
    if( const auto *slots = Result.Nodes.getNodeAs<DeclaratorDecl>( "slots" ) ) {
        diag( slots->getLocation(), "review this two-slot array in an overmap roster consumer" );
        return;
    }

    const auto *compare = Result.Nodes.getNodeAs<BinaryOperator>( "compare" );
    const auto *bound = Result.Nodes.getNodeAs<IntegerLiteral>( "bound" );
    const auto *size = Result.Nodes.getNodeAs<CXXMemberCallExpr>( "size" );
    const auto op = compare->getLHS()->IgnoreParenImpCasts() == size ?
                    compare->getOpcode() : BinaryOperator::reverseComparisonOp( compare->getOpcode() );
    const bool two = bound->getValue() == 2;
    if( ( two && ( op == BO_EQ || op == BO_NE || op == BO_GT || op == BO_LE ) ) ||
        ( !two && ( op == BO_GE || op == BO_LT ) ) ) {
        diag( compare->getOperatorLoc(),
              "review this fixed pair size in an overmap roster consumer" );
    }
}

} // namespace clang::tidy::cata
