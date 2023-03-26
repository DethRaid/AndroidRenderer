#pragma once

/**
 * Helper class for visitors, allows specifying a separate callable for each type in a variant
 */
template<class... Ts> struct Visitor : Ts... { using Ts::operator()...; };
