#include "string_conversion.hpp"

#include <utf8.h>

#include <sstream>

// std::string to_string(const std::u16string& chonker) {
//     return utf8::utf16to8(chonker);
// }
//
// std::u16string to_wstring(const std::string& thinboi) {
//     return utf8::utf8to16(thinboi);
// }

std::vector<std::string_view> split_string_by_newline(const std::string_view str) {
    return split_string(str, '\n');
}

std::vector<std::string_view> split_string(std::string_view str, const char separator) {
    auto next_token_begin_iterator = str.begin();
    auto output = std::vector<std::string_view>{};
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
