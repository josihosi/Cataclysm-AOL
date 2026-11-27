#ifndef CATA_TOOLS_CLANG_TIDY_PLUGIN_OVERMAPPARTYROSTERCHECK_H
#define CATA_TOOLS_CLANG_TIDY_PLUGIN_OVERMAPPARTYROSTERCHECK_H

#include <clang-tidy/ClangTidyCheck.h>

namespace clang::tidy::cata
{

class OvermapPartyRosterCheck : public ClangTidyCheck
{
    public:
        using ClangTidyCheck::ClangTidyCheck;
        void registerMatchers( ast_matchers::MatchFinder *Finder ) override;
        void check( const ast_matchers::MatchFinder::MatchResult &Result ) override;
};

} // namespace clang::tidy::cata

#endif // CATA_TOOLS_CLANG_TIDY_PLUGIN_OVERMAPPARTYROSTERCHECK_H
