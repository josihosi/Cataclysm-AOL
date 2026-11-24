#ifndef CATA_TOOLS_CLANG_TIDY_PLUGIN_OBSERVATIONCOSTCHECK_H
#define CATA_TOOLS_CLANG_TIDY_PLUGIN_OBSERVATIONCOSTCHECK_H

#include <clang-tidy/ClangTidyCheck.h>

namespace clang::tidy::cata
{

class TinymapLoadCheck : public ClangTidyCheck
{
    public:
        using ClangTidyCheck::ClangTidyCheck;
        void registerMatchers( ast_matchers::MatchFinder *Finder ) override;
        void check( const ast_matchers::MatchFinder::MatchResult &Result ) override;
};

class ProspectiveLookoutCheck : public ClangTidyCheck
{
    public:
        using ClangTidyCheck::ClangTidyCheck;
        void registerMatchers( ast_matchers::MatchFinder *Finder ) override;
        void check( const ast_matchers::MatchFinder::MatchResult &Result ) override;
};

class StalkerLoopReaderCheck : public ClangTidyCheck
{
    public:
        using ClangTidyCheck::ClangTidyCheck;
        void registerMatchers( ast_matchers::MatchFinder *Finder ) override;
        void check( const ast_matchers::MatchFinder::MatchResult &Result ) override;
};

class HordeLightSampleOrderCheck : public ClangTidyCheck
{
    public:
        using ClangTidyCheck::ClangTidyCheck;
        void registerMatchers( ast_matchers::MatchFinder *Finder ) override;
        void check( const ast_matchers::MatchFinder::MatchResult &Result ) override;
};

} // namespace clang::tidy::cata

#endif // CATA_TOOLS_CLANG_TIDY_PLUGIN_OBSERVATIONCOSTCHECK_H
