#include "string_conversion.hpp"

#include <Windows.h>

#include <sstream>

std::string to_string(const std::wstring& chonker) {
    int count = WideCharToMultiByte(CP_UTF8, 0, chonker.c_str(), chonker.length(), nullptr, 0, nullptr, nullptr);
    std::string str(count, 0);
    WideCharToMultiByte(CP_UTF8, 0, chonker.c_str(), -1, str.data(), count, nullptr, nullptr);
    return str;
}

std::wstring to_wstring(const std::string& thinboi) {
    int count = MultiByteToWideChar(CP_UTF8, 0, thinboi.c_str(), thinboi.length(), nullptr, 0);
    std::wstring wstr(count, 0);
    MultiByteToWideChar(CP_UTF8, 0, thinboi.c_str(), thinboi.length(), wstr.data(), count);
    return wstr;
}

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
