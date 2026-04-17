#include "llm_prompt_templates.h"

#include <filesystem>

#include "filesystem.h"
#include "path_info.h"

namespace
{

constexpr const char *llm_prompt_dirname = "llm_prompts";

std::filesystem::path llm_prompt_override_dir_path()
{
    return ( PATH_INFO::config_dir_path() / llm_prompt_dirname ).get_unrelative_path();
}

std::filesystem::path bundled_llm_prompt_dir_path()
{
    return ( PATH_INFO::datadir_path() / llm_prompt_dirname ).get_unrelative_path();
}

void replace_all_in_place( std::string &text, const std::string &from, const std::string &to )
{
    if( from.empty() ) {
        return;
    }
    size_t pos = 0;
    while( ( pos = text.find( from, pos ) ) != std::string::npos ) {
        text.replace( pos, from.size(), to );
        pos += to.size();
    }
}

bool prompt_template_has_required_tokens( const std::string &templ,
        const std::initializer_list<std::string_view> &required_tokens )
{
    if( templ.find_first_not_of( " \t\r\n" ) == std::string::npos ) {
        return false;
    }
    for( const std::string_view token : required_tokens ) {
        if( templ.find( token ) == std::string::npos ) {
            return false;
        }
    }
    return true;
}

void seed_llm_prompt_override_files( const std::initializer_list<const char *> &filenames )
{
    if( filenames.size() == 0 ) {
        return;
    }

    const std::filesystem::path override_dir = llm_prompt_override_dir_path();
    if( !assure_dir_exist( override_dir ) ) {
        return;
    }

    const std::filesystem::path bundled_dir = bundled_llm_prompt_dir_path();
    for( const char *filename : filenames ) {
        const std::filesystem::path source = bundled_dir / filename;
        const std::filesystem::path dest = override_dir / filename;
        if( file_exist( source ) && !file_exist( dest ) ) {
            copy_file( source.string(), dest.string() );
        }
    }
}

} // namespace

namespace llm_prompt_templates
{

std::string render( std::string templ,
                    const std::initializer_list<std::pair<std::string, std::string>> &replacements )
{
    for( const std::pair<std::string, std::string> &entry : replacements ) {
        replace_all_in_place( templ, entry.first, entry.second );
    }
    return templ;
}

std::string load( const char *filename,
                  std::string_view fallback_template,
                  const std::initializer_list<std::string_view> &required_tokens,
                  const std::initializer_list<const char *> &seed_filenames )
{
    seed_llm_prompt_override_files( seed_filenames );

    for( const std::filesystem::path &path : { llm_prompt_override_dir_path() / filename,
                                               bundled_llm_prompt_dir_path() / filename } ) {
        if( !file_exist( path ) ) {
            continue;
        }
        const std::string templ = read_entire_file( path );
        if( prompt_template_has_required_tokens( templ, required_tokens ) ) {
            return templ;
        }
    }

    return std::string( fallback_template );
}

} // namespace llm_prompt_templates
