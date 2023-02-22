//
// Created by gold1 on 9/5/2022.
//

#include "percent_encoding.hpp"

char decode(char c1, char c2);

std::string decode_percent_encoding(std::string_view input) {
    auto output = std::string{};
    output.reserve(input.size());

    for (auto i = 0u ; i < input.size() ; i++) {
        if (input[i] == '%') {
            // decode
            const auto decoded_char = decode(input[i + 1], input[i + 2]);
            output.push_back(decoded_char);
            i += 2;

        } else {
            output.push_back(input[i]);
        }
    }

    output.shrink_to_fit();

    return output;
}

uint32_t hex_to_dec(char hex) {
    if (hex >= '0' && hex <= '9') {
        return hex - '0';
    } else if (hex >= 'A' && hex <= 'F') {
        return (hex - 'A') + 10;
    } else if (hex >= 'a' && hex <= 'f') {
        return (hex - 'a') + 10;
    }

    return -1;
}

char decode(char c1, char c2) {
    const auto high = hex_to_dec(c1);
    const auto low = hex_to_dec(c2);
    return static_cast<char>(high << 4 | low);
}
