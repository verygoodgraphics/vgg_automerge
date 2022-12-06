#include "State.h"
#include "../Encoder.h"
#include "../Decoder.h"

std::vector<u8> State::encode() const {
    std::vector<u8> buf;
    buf.reserve(SYNC_STATE_TYPE);

    Encoder encoder(buf);
    encoder.encode(shared_heads);

    return buf;
}

std::optional<State> State::decode(const BinSlice& bytes) {
    Decoder decoder(bytes);

    auto record_type = decoder.read<u8>();
    if (!record_type) {
        return {};
    }
    if (*record_type != SYNC_STATE_TYPE) {
        // throw WrongType
        return {};
    }

    auto shared_heads = decode_hashes(decoder);
    if (!shared_heads) {
        return {};
    }

    return State{
        std::move(*shared_heads),
        {},
        {},
        {},
        std::vector<Have>(),
        {}
    };
}

std::optional<std::vector<ChangeHash>> decode_hashes(Decoder& decoder) {
    std::optional<u32> len = decoder.read<u32>();
    if (!len) {
        return {};
    }

    auto hashes = std::vector<ChangeHash>();
    if (*len == 0) {
        return hashes;
    }
    hashes.reserve(*len);

    try {
        for (int i = 0; i < *len; ++i) {
            auto hash_bytes = decoder.read_bytes(HASH_SIZE);
            if (!hash_bytes) {
                return {};
            }
            
            hashes.push_back(ChangeHash(*hash_bytes));
        }
    }
    catch (std::exception) {
        return {};
    }

    return hashes;
}