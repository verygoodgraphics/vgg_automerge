// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include <cassert>

#include "Encoder.h"
#include "leb128.h"
#include "helper.h"

usize Encoder::encode(const std::string_view& val) {
    out_buf.reserve(out_buf.size() + LEB128_U64_MAX_BYTE_SIZE + val.size());

    usize head = encode(val.size());
    out_buf.insert(out_buf.end(), val.begin(), val.end());

    return head + val.size();
}

usize Encoder::encode(const std::string& val) {
    out_buf.reserve(out_buf.size() + LEB128_U64_MAX_BYTE_SIZE + val.size());

    usize head = encode(val.size());
    out_buf.insert(out_buf.end(), val.begin(), val.end());

    return head + val.size();
}

usize Encoder::encode(const std::vector<ActorId>& val, usize skip) {
    assert(skip <= val.size());

    out_buf.reserve(out_buf.size() + LEB128_U64_MAX_BYTE_SIZE + (val.size() - skip) * ACTOR_ID_SIZE);

    usize len = encode(val.size() - skip);
    for (auto iter = val.cbegin() + skip; iter != val.cend(); ++iter) {
        len += encode(*iter);
    }

    return len;
}

usize Encoder::encode(const ActorId& val) {
    out_buf.reserve(out_buf.size() + 1 + ACTOR_ID_SIZE);

    usize len = ACTOR_ID_SIZE;
    usize head = encode(len);
    out_buf.insert(out_buf.end(), std::begin(val.data), std::end(val.data));

    return head + len;
}

usize Encoder::encode(const std::vector<u8>& val) {
    out_buf.reserve(out_buf.size() + LEB128_U64_MAX_BYTE_SIZE + val.size());

    usize head = encode(val.size());
    vector_extend(out_buf, val);

    return head + val.size();
}

usize Encoder::encode(const BinSlice& val) {
    out_buf.reserve(out_buf.size() + LEB128_U64_MAX_BYTE_SIZE + val.second);

    usize head = encode(val.second);
    out_buf.insert(out_buf.end(), val.first, val.first + val.second);

    return head + val.second;
}

usize Encoder::encode(const std::vector<ChangeHash>& val) {
    out_buf.reserve(out_buf.size() + LEB128_U64_MAX_BYTE_SIZE + val.size() * HASH_SIZE);

    usize head = encode(val.size());
    usize body = 0;

    for (auto& hash : val) {
        std::copy(std::begin(hash.data), std::end(hash.data), std::back_inserter(out_buf));
        body += HASH_SIZE;
    }

    return head + body;
}

usize Encoder::write_unsigned(u64 val) {
    usize bytes_written = 0;

    u8 byte = 0;
    while (true) {
        byte = low_bits_of_u64(val);
        val >>= 7;
        if (val) {
            // More bytes to come, so set the continuation bit.
            byte |= CONTINUATION_BIT;
        }

        out_buf.push_back(byte);
        ++bytes_written;

        if (!val) {
            return bytes_written;
        }
    }
}

usize Encoder::write_signed(s64 val) {
    usize bytes_written = 0;

    u8 byte = 0;
    bool done = false;
    while (true) {
        byte = (u8)val;
        // Keep the sign bit for testing
        val >>= 6;
        done = (val == 0) || (val == -1);
        if (done) {
            byte = low_bits_of_byte(byte);
        }
        else {
            // Remove the sign bit
            val >>= 1;
            // More bytes to come, so set the continuation bit.
            byte |= CONTINUATION_BIT;
        }

        out_buf.push_back(byte);
        ++bytes_written;

        if (done) {
            return bytes_written;
        }
    }
}

/////////////////////////////////////////////////////////

usize ColData::encode_col_len(Encoder& encoder) const {
    usize len = 0;
    if (!data.empty()) {
        len += encoder.encode(col);
        len += encoder.encode(data.size());
    }
    return len;
}

void ColData::deflate() {
    assert(!has_been_deflated);
    has_been_deflated = true;

    if (data.size() > DEFLATE_MIN_SIZE) {
        col |= COLUMN_TYPE_DEFLATE;
        data = deflate_compress(make_bin_slice(data));
    }
}

/////////////////////////////////////////////////////////

void BooleanEncoder::append(bool value) {
    if (value == last) {
        ++count;
    }
    else {
        encoder.encode(count);
        last = value;
        count = 1;
    }
}

ColData BooleanEncoder::finish(u32 col) {
    if (count > 0) {
        encoder.encode(count);
    }
    return ColData{ col, std::move(buf), false };
}
