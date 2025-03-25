#include "string_conversion.hpp"

#include <utf8.h>

#include <sstream>

// static_assert(sizeof(wchar_t) == sizeof(char16_t), "wchar_t must be two bytes! Or all your strings will explode");

std::string to_string(const std::wstring& chonker) {
    return utf8::utf16to8(std::u16string_view{reinterpret_cast<const char16_t*>(chonker.data()), chonker.size()});
}

std::wstring to_wstring(const std::string& thinboi) {
    const auto utf16_string =  utf8::utf8to16(thinboi);
    return std::wstring{ reinterpret_cast<const wchar_t*>(utf16_string.data()), utf16_string.size() };
}

eastl::vector<std::string_view> split_string_by_newline(const std::string_view str) {
    return split_string(str, '\n');
}

eastl::vector<std::string_view> split_string(std::string_view str, const char separator) {
    auto next_token_begin_iterator = str.begin();
    auto output = eastl::vector<std::string_view>{};
    output.reserve(str.size() / 7); // Awkward estimate

    for(auto itr = str.begin(); itr != str.end(); ++itr) {
        auto look_ahead = itr;
        ++look_ahead;
        if(look_ahead == str.end()) {
            output.emplace_back(next_token_begin_iterator, look_ahead);
            break;
        }
        if(*look_ahead == separator) {
            output.emplace_back(next_token_begin_iterator, look_ahead);
            ++look_ahead;
            next_token_begin_iterator = look_ahead;
        }
    }

    return output;
}
