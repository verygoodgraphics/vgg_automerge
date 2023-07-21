// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

constexpr unsigned char CONTINUATION_BIT = 1 << 7;
constexpr unsigned char SIGN_BIT = 1 << 6;

inline unsigned char low_bits_of_byte(unsigned char byte) {
    return byte & ~CONTINUATION_BIT;
}

inline unsigned char low_bits_of_u64(unsigned long long val) {
    unsigned long long byte = val & 0xFF;
    return low_bits_of_byte((unsigned char)byte);
}
