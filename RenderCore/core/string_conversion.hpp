#pragma once

#include <string>
#include <EASTL/vector.h>

[[nodiscard]] std::string to_string(const std::wstring& chonker);

[[nodiscard]] std::wstring to_wstring(const std::string& thinboi);

/**
 * \brief Splits a string based on a separator
 *
 * The sting views in the return value are made from the memory of the input string view. The caller is responsible for ensuring this
 * memory stays alive
 */
[[nodiscard]] eastl::vector<std::string_view> split_string_by_newline(std::string_view str);

/**
 * \brief Splits a string based on a separator
 *
 * The sting views in the return value are made from the memory of the input string view. The caller is responsible for ensuring this memory stays alive
 */
[[nodiscard]] eastl::vector<std::string_view> split_string(std::string_view str, char separator);
