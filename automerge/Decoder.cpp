// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include <limits>

#include "Decoder.h"
#include "helper.h"
#include "leb128.h"

void Decoding::decode_u8(BinSlice& bytes, std::optional<u8>& val) {
    try {
        u8 buffer[1] = { 0 };
        read(bytes, 1, buffer);
        val = buffer[0];
    }
    catch (std::exception) {
        val.reset();
    }
}

// unsigned
void Decoding::decode_u64(BinSlice& bytes, std::optional<u64>& val) {
    try {
        u64 result = 0;
        read_unsigned(bytes, result);
        val = result;
    }
    catch (std::exception) {
        val.reset();
    }
}

// signed
void Decoding::decode_s64(BinSlice& bytes, std::optional<s64>& val) {
    try {
        s64 result = 0;
        read_signed(bytes, result);
        val = result;
    }
    catch (std::exception) {
        val.reset();
    }
}

void Decoding::decode(BinSlice& bytes, std::optional<std::vector<u8>>& val) {
    std::optional<usize> len;
    decode_usize(bytes, len);
    if (!len.has_value()) {
        val.reset();
        return;
    }
    if (*len == 0) {
        val = std::vector<u8>();
        return;
    }

    u8* buffer = new u8[*len]();
    try {
        read(bytes, *len, buffer);
        val = std::vector<u8>(buffer, buffer + *len);
    }
    catch (std::exception) {
        val.reset();
    }
    delete[]buffer;
}

void Decoding::decode(BinSlice& bytes, std::optional<std::string>& val) {
    std::optional<std::vector<u8>> result;
    decode(bytes, result);

    if (!result.has_value()) {
        val.reset();
    }
    else {
        val = std::string(result->cbegin(), result->cend());
    }
}

void Decoding::decode(BinSlice& bytes, std::optional<std::string_view>& val) {
    std::optional<std::string> result;
    decode(bytes, result);

    if (!result.has_value()) {
        val.reset();
    }
    else {
        val = cache_string(*result);
    }
}

void Decoding::decode(BinSlice& bytes, std::optional<std::optional<std::string>>& val) {
    std::optional<std::vector<u8>> result;
    decode(bytes, result);

    if (!result.has_value()) {
        val.reset();
    }
    else if (result->empty()) {
        val = std::optional<std::string>();
    }
    else {
        val = std::optional<std::string>(std::string(result->cbegin(), result->cend()));
    }
}

void Decoding::decode(BinSlice& bytes, std::optional<std::optional<std::string_view>>& val) {
    std::optional<std::optional<std::string>> result;
    decode(bytes, result);

    if (!result.has_value()) {
        val.reset();
    }
    else if (!result->has_value()) {
        val = std::optional<std::string_view>();
    }
    else {
        val = std::optional<std::string_view>(cache_string(**result));
    }
}

void Decoding::decode_double(BinSlice& bytes, std::optional<double>& val) {
    try {
        double res = 0;
        constexpr auto len = sizeof(res);
        u8 buffer[len] = { 0 };
        read(bytes, len, buffer);

        int num = 1;
        if (*(char*)&num == 1) {
            // Little-Endian
            std::copy(std::begin(buffer), std::end(buffer), reinterpret_cast<u8*>(&res));
        }
        else {
            // Big-Endian
            std::copy(std::rbegin(buffer), std::rend(buffer), reinterpret_cast<u8*>(&res));
        }

        val = res;
    }
    catch (std::exception) {
        val.reset();
    }
}

void Decoding::decode_float(BinSlice& bytes, std::optional<float>& val) {
    try {
        float res = 0;
        constexpr auto len = sizeof(res);
        u8 buffer[len] = { 0 };
        read(bytes, len, buffer);

        int num = 1;
        if (*(char*)&num == 1) {
            // Little-Endian
            std::copy(std::begin(buffer), std::end(buffer), reinterpret_cast<u8*>(&res));
        }
        else {
            // Big-Endian
            std::copy(std::rbegin(buffer), std::rend(buffer), reinterpret_cast<u8*>(&res));
        }

        val = res;
    }
    catch (std::exception) {
        val.reset();
    }
}

void Decoding::decode_usize(BinSlice& bytes, std::optional<usize>& val) {
    try {
        u64 result = 0;
        read_unsigned(bytes, result);
        if (result > std::numeric_limits<usize>::max()) {
            val.reset();
        }
        else {
            val = (usize)result;
        }
    }
    catch (std::exception) {
        val.reset();
    }
}

void Decoding::decode_u32(BinSlice& bytes, std::optional<u32>& val) {
    try {
        u64 result = 0;
        read_unsigned(bytes, result);
        if (result > UINT32_MAX) {
            val.reset();
        }
        else {
            val = (u32)result;
        }
    }
    catch (std::exception) {
        val.reset();
    }
}

void Decoding::decode_s32(BinSlice& bytes, std::optional<s32>& val) {
    try {
        s64 result = 0;
        read_signed(bytes, result);
        if (result > INT32_MAX ||
            result < INT32_MIN) {
            val.reset();
        }
        else {
            val = (s32)result;
        }
    }
    catch (std::exception) {
        val.reset();
    }
}

void Decoding::decode(BinSlice& bytes, std::optional<ActorId>& val) {
    std::optional<std::vector<u8>> result;
    decode(bytes, result);

    if (!result.has_value()) {
        val.reset();
    }
    else {
        val = ActorId(*result);
    }
}

void Decoding::decode_action(BinSlice& bytes, std::optional<Action>& val) {
    try {
        u64 result = 0;
        read_unsigned(bytes, result);
        if (result >= (u64)Action::BUTT) {
            val.reset();
        }
        else {
            val = (Action)result;
        }
    }
    catch (std::exception) {
        val.reset();
    }
}

void Decoding::read_unsigned(BinSlice& bytes, u64& result) {
    result = 0;
    s32 shift = 0;

    u8 buf[1] = { 0 };
    u64 low_bits = 0;

    while (true) {
        read(bytes, 1, buf);

        if (shift == 63 && buf[0] != 0 && buf[0] != 1) {
            while ((buf[0] & CONTINUATION_BIT) != 0) {
                read(bytes, 1, buf);
            }
            throw std::overflow_error("decode u64");
        }

        low_bits = (u64)low_bits_of_byte(buf[0]);
        result |= low_bits << shift;

        if ((buf[0] & CONTINUATION_BIT) == 0) {
            return;
        }

        shift += 7;
    }
}

void Decoding::read_signed(BinSlice& bytes, s64& result) {
    result = 0;
    s32 shift = 0;
    s32 size = 64;
    u8 byte = 0;

    u8 buf[1] = { 0 };
    s64 low_bits = 0;

    while (true) {
        read(bytes, 1, buf);

        byte = buf[0];
        if (shift == 63 && byte != 0 && byte != 0x7f) {
            while ((buf[0] & CONTINUATION_BIT) != 0) {
                read(bytes, 1, buf);
            }
            throw std::overflow_error("decode s64");
        }

        low_bits = (s64)low_bits_of_byte(byte);
        result |= low_bits << shift;
        shift += 7;

        if ((byte & CONTINUATION_BIT) == 0) {
            break;
        }
    }

    if (shift < size && (SIGN_BIT & byte) == SIGN_BIT) {
        // Sign extend the result.
        result |= UINT64_MAX << shift;
    }
}

void Decoding::read(BinSlice& bytes, usize count, u8* buf) {
    if (count > bytes.second) {
        throw std::out_of_range("not enough bytes to read");
    }

    std::copy(bytes.first, bytes.first + count, buf);
    bytes.first += count;
    bytes.second -= count;
}

/////////////////////////////////////////////////////////

std::optional<bool> BooleanDecoder::next() {
    while (count == 0) {
        if (decoder.done() && count == 0) {
            return { false };
        }

        auto res = decoder.read<usize>();
        if (!res.has_value()) {
            count = 0;
        }
        else {
            count = *res;
        }

        last_value = !last_value;
    }
    count -= 1;

    return { last_value };
}

std::optional<std::optional<u64>> DeltaDecoder::next() {
    auto delta = rle.next();
    if (!delta.has_value()) {
        return {};
    }
    if (!(*delta).has_value()) {
        return std::optional<u64>();
    }

    if (**delta < 0) {
        absolute_val -= std::abs(**delta);
    }
    else {
        absolute_val += **delta;
    }

    return { absolute_val };
}