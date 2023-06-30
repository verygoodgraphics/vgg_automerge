// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <variant>
//#include <exception>
//#include <stdexcept>

#include "type.h"

using ActorIdPair = std::pair<u64, ActorId>;

struct  AutomergeError {
    enum {
        NotAnObject,
        InvalidObjIdFormat, //(String),
        InvalidObjId, //(String),
        Encoding,
        Decoding,
        EmptyStringKey,
        InvalidSeq, //(u64),
        InvalidIndex, //(usize),
        DuplicateSeqNumber, //(u64, ActorId),
        InvalidHash, //(ChangeHash),
        MissingHash, //(ChangeHash),
        MissingCounter,
        Fail,
    } tag = NotAnObject;
    std::variant<std::string, u64, ActorIdPair, ChangeHash> data = {};

private:
};