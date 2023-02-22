//
// Created by gold1 on 9/5/2022.
//

#ifndef SAHRENDERER_PERCENT_ENCODING_HPP
#define SAHRENDERER_PERCENT_ENCODING_HPP

#include <string>
#include <string_view>

/**
 * Decodes a stinky hex-encoded string into a useful string
 * @param input Hex-encoded string
 * @return Useful string
 */
std::string decode_percent_encoding(std::string_view input);

#endif //SAHRENDERER_PERCENT_ENCODING_HPP
