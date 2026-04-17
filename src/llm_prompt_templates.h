#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace llm_prompt_templates
{

std::string render( std::string templ,
                    const std::initializer_list<std::pair<std::string, std::string>> &replacements );

std::string load( const char *filename,
                  std::string_view fallback_template,
                  const std::initializer_list<std::string_view> &required_tokens,
                  const std::initializer_list<const char *> &seed_filenames = {} );

} // namespace llm_prompt_templates
