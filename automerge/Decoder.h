// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once
#include <optional>
#include <memory>
#include <type_traits>
#include <stdexcept>

#include "type.h"

class Decoding {
public:
    template<typename T>
    static void decode(BinSlice& bytes, std::optional<T>& val) {
        using U = std::decay_t<T>;

        if constexpr (std::is_same_v<U, Action>) {
            decode_action(bytes, val);
        }
        // unsigned
        else if constexpr (std::is_same_v<U, u64>) {
            decode_u64(bytes, val);
        }
        else if constexpr (std::is_same_v<U, u32>) {
            decode_u32(bytes, val);
        }
        else if constexpr (std::is_same_v<U, u8>) {
            decode_u8(bytes, val);
        }
        else if constexpr (std::is_same_v<U, usize>) {
            decode_usize(bytes, val);
        }
        // signed
        else if constexpr (std::is_same_v<U, s64>) {
            decode_s64(bytes, val);
        }
        else if constexpr (std::is_same_v<U, s32>) {
            decode_s32(bytes, val);
        }
        // double
        else if constexpr (std::is_same_v<U, double>) {
            decode_double(bytes, val);
        }
        // float
        else if constexpr (std::is_same_v<U, float>) {
            decode_float(bytes, val);
        }
        else {
            throw("not supported type");
        }
    }

    static void decode(BinSlice& bytes, std::optional<std::vector<u8>>& val);

    static void decode(BinSlice& bytes, std::optional<std::string>& val);

    static void decode(BinSlice& bytes, std::optional<std::optional<std::string>>& val);

    static void decode(BinSlice& bytes, std::optional<ActorId>& val);

private:
    static void decode_u8(BinSlice& bytes, std::optional<u8>& val);

    static void decode_u64(BinSlice& bytes, std::optional<u64>& val);

    static void decode_s64(BinSlice& bytes, std::optional<s64>& val);

    static void decode_double(BinSlice& bytes, std::optional<double>& val);

    static void decode_float(BinSlice& bytes, std::optional<float>& val);

    static void decode_usize(BinSlice& bytes, std::optional<usize>& val);

    static void decode_u32(BinSlice& bytes, std::optional<u32>& val);

    static void decode_s32(BinSlice& bytes, std::optional<s32>& val);

    static void decode_action(BinSlice& bytes, std::optional<Action>& val);

    static void read_unsigned(BinSlice& bytes, u64& result);

    static void read_signed(BinSlice& bytes, s64& result);

    static void read(BinSlice& bytes, usize count, u8* buf);
};

struct Decoder {
public:
    usize offset = 0;
    usize last_read = 0;

    Decoder() = default;
    Decoder(std::vector<u8>&& data) : offset(0), last_read(0),
        _data(std::move(data)), data{ _data.cbegin(), _data.size() } {}
    Decoder(const BinSlice& data) : offset(0), last_read(0), _data(), data(data) {}

    template<class T>
    std::optional<T> read() {
        BinSlice buf = { data.first + offset, data.second - offset };
        usize init_len = buf.second;

        std::optional<T> res;
        Decoding::decode(buf, res);
        if (!res.has_value()) {
            //throw std::runtime_error("NoDecodedValue");
            return {};
        }

        usize delta = init_len - buf.second;
        if (delta == 0) {
            //throw std::runtime_error("BufferSizeDidNotChange");
            return {};
        }
        else {
            last_read = delta;
            offset += delta;
            return res;
        }
    }

    std::optional<BinSlice> read_bytes(usize index) {
        if (offset + index > data.second) {
            //throw std::runtime_error("TryingToReadPastEnd");
            return {};
        }

        last_read = index;
        offset += index;

        return std::make_pair(data.first + (offset - index), index);
    }

    bool done() {
        return offset >= data.second;
    }

private:
    std::vector<u8> _data;
    BinSlice data;
};

struct BooleanDecoder {
    Decoder decoder;
    bool last_value;
    usize count;

    BooleanDecoder() = default;
    BooleanDecoder(std::vector<u8>&& data) : decoder(std::move(data)), last_value(true), count(0) {}
    BooleanDecoder(const BinSlice& data) : decoder(data), last_value(true), count(0) {}

    std::optional<bool> next();
};

template<class T>
struct RleDecoder {
    Decoder decoder;
    std::optional<T> last_value;
    s64 count = 0;
    bool _literal = false;

    RleDecoder() = default;
    RleDecoder(std::vector<u8>&& data) : decoder(std::move(data)), last_value{}, count(0), _literal(false) {}
    RleDecoder(const BinSlice& data) : decoder(data), last_value{}, count(0), _literal(false) {}

    std::optional<std::optional<T>> next() {
        while (count == 0) {
            if (decoder.done()) {
                return std::optional<T>();
            }

            auto res = decoder.read<s64>();
            if (!res.has_value()) {
                // warning
                return {};
            }
            else if (*res > 0) {
                // normal run
                count = *res;
                last_value = decoder.read<T>();
                _literal = false;
            }
            else if (*res < 0) {
                // _literal run
                count = std::abs(*res);
                _literal = true;
            }
            else {
                // null run
                count = decoder.read<usize>().value();
                last_value.reset();
                _literal = false;
            }
        }

        count -= 1;
        if (_literal) {
            return decoder.read<T>();
        }
        else {
            return last_value;
        }
    }
};

struct DeltaDecoder {
    RleDecoder<s64> rle;
    u64 absolute_val;

    DeltaDecoder() = default;
    DeltaDecoder(std::vector<u8>&& bytes) : rle(std::move(bytes)), absolute_val(0) {}
    DeltaDecoder(const BinSlice& bytes) : rle(bytes), absolute_val(0) {}

    std::optional<std::optional<u64>> next();
};