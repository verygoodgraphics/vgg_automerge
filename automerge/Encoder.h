// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once
#include <optional>
#include <type_traits>

#include "type.h"

const usize DEFLATE_MIN_SIZE = 256;
const u32 COLUMN_TYPE_DEFLATE = 8;

class Encoder {
public:
    Encoder(std::vector<u8>& _buf) : out_buf(_buf) {}

    template<typename T>
    usize encode(T val) {
        using U = std::decay_t<T>;

        // unsigned
        if constexpr (std::is_same_v<U, u64> || std::is_same_v<U, u32> || std::is_same_v<U, usize>) {
            return write_unsigned((u64)val);
        }
        // signed
        else if constexpr (std::is_same_v<U, s64> || std::is_same_v<U, s32>) {
            return write_signed((s64)val);
        }
        else if constexpr (std::is_same_v<U, double> || std::is_same_v<U, float>) {
            auto* p = reinterpret_cast<const u8*>(&val);
            auto len = sizeof(val);
            int num = 1;
            if (*(char*)&num == 1) {
                // Little-Endian
                out_buf.insert(out_buf.end(), p, p + len);
            }
            else {
                // Big-Endian
                out_buf.insert(out_buf.end(), std::make_reverse_iterator(p + len), std::make_reverse_iterator(p));
            }

            return len;
        }
        else {
            throw("not supported type");
        }
    }

    usize encode(const std::string& val);

    usize encode(const std::optional<std::string>& val) {
        if (val) {
            return encode(*val);
        }
        else {
            return encode((u64)0);
        }
    }

    usize encode(Action val) {
        return encode((u64)val);
    }

    usize encode(const std::vector<ActorId>& val, usize skip = 0);

    usize encode(const ActorId& val);

    usize encode_with_acotrs(const ActorId& val, const std::vector<ActorId>& actors) {
        return encode(val.actor_index(actors));
    }

    usize encode(const std::vector<u8>& val);

    usize encode(const BinSlice& val);

    usize encode(const std::vector<ChangeHash>& val);

private:
    std::vector<u8>& out_buf;

    usize write_unsigned(u64 val);

    usize write_signed(s64 val);
};

struct ColData {
    u32 col = 0;
    std::vector<u8> data;
    bool has_been_deflated = false;

    bool operator<(const ColData& other) const {
        return (col < other.col);
    }

    usize encode_col_len(Encoder& encoder) const;

    void deflate();
};

template <class T>
struct RleState {
    enum {
        EMPTY,
        NULL_RUN,       // usize
        LITERAL_RUN,    // T, Vec<T>
        LONE_VAL,       // T
        RUN             // T, usize
    } tag = EMPTY;
    usize size = 0;
    T value = {};
    std::vector<T> vec_value = {};
};

template <class T>
struct RleEncoder {
    std::vector<u8> buf;
    RleState<T> state;
    Encoder encoder = Encoder(buf);

    ColData finish(u32 col) {
        RleState<T> old_state = take_state();
        switch (old_state.tag) {
        case RleState<T>::NULL_RUN:
            if (!buf.empty()) {
                flush_null_run(old_state.size);
            }
            break;
        case RleState<T>::LONE_VAL:
            flush_lit_run({ std::move(old_state.value) });
            break;
        case RleState<T>::RUN:
            flush_run(old_state.value, old_state.size);
            break;
        case RleState<T>::LITERAL_RUN:
            old_state.vec_value.push_back(std::move(old_state.value));
            flush_lit_run(std::move(old_state.vec_value));
            break;
        case RleState<T>::EMPTY:
            break;
        default:
            break;
        }

        return ColData{ col, std::move(buf), false };
    }

    void flush_run(const T& val, usize len) {
        encoder.encode((s64)len);
        encoder.encode(val);
    }

    void flush_null_run(usize len) {
        encoder.encode((s64)0);
        encoder.encode(len);
    }

    void flush_lit_run(std::vector<T>&& run) {
        encoder.encode(-((s64)run.size()));
        for (auto& val : run) {
            encoder.encode(val);
        }
    }

    RleState<T> take_state() {
        RleState<T> state;
        std::swap(this->state, state);
        return state;
    }

    void append_null() {
        RleState<T> old_state = take_state();
        switch (old_state.tag) {
        case RleState<T>::EMPTY:
            state = RleState<T>{ RleState<T>::NULL_RUN, 1, {}, {} };
            break;
        case RleState<T>::NULL_RUN:
            old_state.size += 1;
            state = old_state;
            break;
        case RleState<T>::LONE_VAL:
            flush_lit_run({ std::move(old_state.value) });
            state = RleState<T>{ RleState<T>::NULL_RUN, 1, {}, {} };
            break;
        case RleState<T>::RUN:
            flush_run(old_state.value, old_state.size);
            state = RleState<T>{ RleState<T>::NULL_RUN, 1, {}, {} };
            break;
        case RleState<T>::LITERAL_RUN:
            old_state.vec_value.push_back(std::move(old_state.value));
            flush_lit_run(std::move(old_state.vec_value));
            state = RleState<T>{ RleState<T>::NULL_RUN, 1, {}, {} };
            break;
        default:
            break;
        }
    }

    void append_value(T&& value) {
        RleState<T> old_state = take_state();
        switch (old_state.tag) {
        case RleState<T>::EMPTY:
            state = RleState<T>{ RleState<T>::LONE_VAL, 0, std::move(value), {} };
            break;
        case RleState<T>::LONE_VAL:
            if (old_state.value == value) {
                state = RleState<T>{ RleState<T>::RUN, 2, std::move(value), {} };
            }
            else {
                std::vector<T> v;
                v.reserve(2);
                v.push_back(std::move(old_state.value));
                state = RleState<T>{ RleState<T>::LITERAL_RUN, 0, std::move(value), std::move(v) };
            }
            break;
        case RleState<T>::RUN:
            if (old_state.value == value) {
                old_state.size += 1;
                state = old_state;
            }
            else {
                flush_run(old_state.value, old_state.size);
                state = RleState<T>{ RleState<T>::LONE_VAL, 0, std::move(value), {} };
            }
            break;
        case RleState<T>::LITERAL_RUN:
            if (old_state.value == value) {
                flush_lit_run(std::move(old_state.vec_value));
                state = RleState<T>{ RleState<T>::RUN, 2, std::move(value), {} };
            }
            else {
                old_state.vec_value.push_back(std::move(old_state.value));
                old_state.value = std::move(value);
                state = old_state;
            }
            break;
        case RleState<T>::NULL_RUN:
            flush_null_run(old_state.size);
            state = RleState<T>{ RleState<T>::LONE_VAL, 0, std::move(value), {} };
            break;
        default:
            break;
        }
    }
};

struct  BooleanEncoder {
    std::vector<u8> buf;
    bool last = false;
    usize count = 0;
    Encoder encoder = Encoder(buf);

    void append(bool value);

    ColData finish(u32 col);
};

struct DeltaEncoder {
    RleEncoder<s64> rle;
    u64 absolute_value = 0;

    void append_value(u64 value) {
        rle.append_value((s64)value - (s64)absolute_value);
        absolute_value = value;
    }

    void append_null() {
        rle.append_null();
    }

    ColData finish(u32 col) {
        return rle.finish(col);
    }
};