// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "Automerge.h"
#include <sstream>
#include <list>
#include <charconv>

#include "query/Len.h"
#include "query/QueryProp.h"
#include "query/Nth.h"

const ActorId& Automerge::get_actor() const {
    if (!actor.isUsed) {
        return actor.unused;
    }
    else {
        return ops.m.actors.get(actor.cached);
    }
}

usize Automerge::get_actor_index() {
    if (actor.isUsed) {
        return actor.cached;
    }

    usize index = ops.m.cache_actor(std::move(actor.unused));
    actor.isUsed = true;
    actor.cached = index;

    return index;
}

TransactionInner Automerge::transaction_inner() {
    usize actor = get_actor_index();
    u64 seq = 0;
    try {
        auto& state = states.at(actor);
        seq = state.size() + 1;
    }
    catch (std::out_of_range&) {
        seq = 1;
    }
    auto deps = get_heads();
    if (seq > 1) {
        auto last_hash = get_hash(actor, seq - 1);
        bool found = false;
        for (auto& d : deps) {
            if (d == last_hash) {
                found = true;
            }
        }
        if (!found) {
            deps.push_back(std::move(last_hash));
        }
    }

    return TransactionInner{
        actor,
        seq,
        max_op + 1,
        0,
        {},
        {},
        {},
        deps,
        {}
    };
}

std::pair<std::vector<ExId>, ChangeHash> Automerge::transact_with(CommitOptionsFunc c, TransactionFunc f) {
    auto tx = transaction();
    auto result = f(tx);

    auto hash = tx.commit_with(c(result));

    return { std::move(result), std::move(hash) };
}

Keys Automerge::keys(const ExId& obj) const {
    try {
        auto [object, _] = exid_to_obj(obj);
        return Keys(this, ops.keys(object));
    }
    catch (std::exception&) {
        return Keys(this, std::nullopt);
    }
}

usize Automerge::length(const ExId& obj) const {
    try {
        auto [inner_obj, obj_type] = exid_to_obj(obj);

        switch (obj_type) {
            case ObjType::Map:
            case ObjType::Table:
                return keys(obj).count();
            case ObjType::List:
            case ObjType::Text: {
                auto q = Len();
                return static_cast<Len&>(ops.search(inner_obj, q)).len;
            }
            default:
                return 0;
        }
    }
    catch (std::exception&) {
        return 0;
    }
}

usize Automerge::length_at(const ExId& obj, const std::vector<ChangeHash>& heads) const {
    try {
        auto [inner_obj, obj_type] = exid_to_obj(obj);
        auto clock = clock_at(heads);

        switch (obj_type) {
            case ObjType::Map:
            case ObjType::Table:
                // TODO: query::KeyAt
                break;
            case ObjType::List:
            case ObjType::Text:
                // TODO: query::LenAt
                break;
            default:
                break;
        }
    }
    catch (std::exception&) {
        return 0;
    }

    return 0;
}

std::pair<ObjId, ObjType> Automerge::exid_to_obj(const ExId& id) const {
    if (id.isRoot) {
        return { ROOT, ObjType::Map };
    }

    auto& [ctr, actor, idx] = id.id;

    // do a direct get here b/c this could be foriegn and not be within the array
    // bounds
    ObjId obj;
    if (ops.m.actors._cache.size() > idx &&
        ops.m.actors._cache[idx] == actor) {
        obj = ObjId{ ctr, idx };
    }
    else {
        auto res = ops.m.actors.lookup(actor);
        if (!res.has_value()) {
            throw AutomergeError{ AutomergeError::Fail, (u64)0 };
        }
        obj = ObjId{ ctr, *res };
    }

    auto obj_type = ops.object_type(obj);
    if (obj_type) {
        return { obj, *obj_type };
    }
    else {
        throw AutomergeError{ AutomergeError::NotAnObject, (u64)0 };
    }
}

ExId Automerge::id_to_exid(const OpId& id) const {
    return ops.id_to_exid(id);
}

std::optional<ValuePair> Automerge::get(const ExId& obj, Prop&& prop) const {
    auto all = get_all(obj, std::move(prop));
    if (all.empty()) {
        return {};
    }
    else {
        return all.back();
    }
}

std::vector<ValuePair> Automerge::get_all(const ExId& obj, Prop&& prop) const {
    auto map = [&](const std::vector<const Op*>& ops) -> std::vector<ValuePair> {
        std::vector<ValuePair> result;
        result.reserve(ops.size());
        std::transform(ops.begin(), ops.end(), std::back_inserter(result), [&](const Op* op) {
            return std::make_pair(this->id_to_exid(op->id), op->value());
            });

        std::stable_sort(result.begin(), result.end(), [](const ValuePair& left, const ValuePair& right) {
            return std::get<ExId>(right).cmp(std::get<ExId>(left)) < 0;
            });

        return result;
    };

    ObjId object = std::get<ObjId>(exid_to_obj(obj));
    if (prop.tag == Prop::Map) {
        auto prop_cached = this->ops.m.props.lookup(std::get<std::string>(prop.data));
        if (!prop_cached.has_value()) {
            return {};
        }

        auto q = QueryProp(*prop_cached);
        auto& ops = static_cast<QueryProp&>(this->ops.search(object, q)).ops;
        return map(ops);
    }
    else {
        auto q = Nth(std::get<usize>(prop.data));
        auto& ops = static_cast<Nth&>(this->ops.search(object, q)).ops;
        return map(ops);
    }
}

Automerge Automerge::load_with(const BinSlice& data, OpObserver* options) {
    auto changes = load_document(data);
    Automerge doc;
    doc.apply_changes_with(std::move(changes), options);

    return doc;
}

bool Automerge::duplicate_seq(const Change& change) const {
    bool dup = false;
    auto actor_index = ops.m.actors.lookup(change.actor_id());
    if (actor_index) {
        auto state = states.find(*actor_index);
        if (state != states.end()) {
            dup = (state->second.size() >= change.seq);
        }
    }
    return dup;
}

void Automerge::apply_changes_with(std::vector<Change>&& changes, OpObserver* options) {
    for (auto& c : changes) {
        if (histroy_index.count(c.hash)) {
            continue;
        }

        if (duplicate_seq(c)) {
            throw AutomergeError{ AutomergeError::DuplicateSeqNumber, ActorIdPair{ c.seq, c.actor_id() } };
        }

        if (is_causally_ready(c)) {
            apply_change(std::move(c), options);
        }
        else {
            queue.push_back(std::move(c));
        }
    }

    while (true) {
        auto c = pop_next_causally_ready_change();
        if (!c.has_value()) {
            break;
        }

        if (!histroy_index.count(c->hash)) {
            apply_change(std::move(*c), options);
        }
    }

    // TODO: optimize to_json
    json_doc = *this;
}

void Automerge::apply_change(Change&& change, OpObserver* observer) {
    auto ops = imports_ops(change);
    update_history(std::move(change), ops.size());

    for (auto& op : ops) {
        this->ops.insert_op(op.first, std::move(op.second));
    }
}

bool Automerge::is_causally_ready(const Change& change) const {
    return std::all_of(change.deps.cbegin(), change.deps.cend(), [&](const ChangeHash& d) {
        return histroy_index.count(d);
        });
}

std::optional<Change> Automerge::pop_next_causally_ready_change() {
    usize index = 0;
    while (index < queue.size()) {
        if (is_causally_ready(queue[index])) {
            auto res = Change(std::move(queue[index]));
            if (index == queue.size() - 1) {
                queue.pop_back();
            }
            else {
                queue[index] = vector_pop(queue);
            }
            return { std::move(res) };
        }
        ++index;
    }

    return {};
}

std::vector<std::pair<ObjId, Op>> Automerge::imports_ops(const Change& change) {
    std::vector<std::pair<ObjId, Op>> res;
    auto op_iter = change.iter_ops();

    usize i = 0;
    std::optional<OldOp> c;
    while ((c = op_iter.next())) {
        usize actor = ops.m.cache_actor(ActorId(change.actor_id()));
        OpId id{ change.start_op + i, std::move(actor) };

        ObjId obj;
        if (c->obj.isRoot) {
            obj = ROOT;
        }
        else {
            obj.counter = c->obj.id.counter;
            obj.actor = ops.m.cache_actor(std::move(c->obj.id.actor));
        }

        auto pred = ops.m.import_opids(std::move(c->pred));

        Key key;
        if (c->key.tag == OldKey::MAP) {
            key = Key{ Key::Map, ops.m.cache_prop(std::get<std::string_view>(std::move(c->key.data))) };
        }
        else {
            auto& elem_id = std::get<OldElementId>(c->key.data);
            if (elem_id.isHead) {
                key = Key{ Key::Seq, HEAD };
            }
            else {
                key = Key{ Key::Seq, ElemId{ elem_id.id.counter, ops.m.cache_actor(std::move(elem_id.id.actor)) } };
            }
        }

        res.emplace_back(
            std::move(obj),
            Op{
                std::move(id),
                std::move(c->action),
                std::move(key),
                {},
                std::move(pred),
                c->insert
            });

        ++i;
    }

    return res;
}

std::vector<ChangeHash> Automerge::merge(Automerge& other) {
    return merge_with(other, nullptr);
}

std::vector<ChangeHash> Automerge::merge(Automerge&& other) {
    return merge_with(other, nullptr);
}

std::vector<ChangeHash> Automerge::merge_with(Automerge& other, OpObserver* options) {
    auto changes_ptr = get_changes_added(other);
    std::vector<Change> changes;
    changes.reserve(changes_ptr.size());
    std::transform(changes_ptr.cbegin(), changes_ptr.cend(), std::back_inserter(changes),
        [](const Change* change_ptr) { return Change(*change_ptr); });

    apply_changes_with(std::move(changes), options);

    return get_heads();
}

std::vector<u8> Automerge::save() {
    auto bytes = encode_document(get_heads(), histroy, ops.iter(), ops.m.actors, ops.m.props._cache);
    saved = get_heads();

    return bytes;
}

void Automerge::filter_changes(const std::vector<ChangeHash>& _heads, std::set<ChangeHash>& changes) const {
    std::vector<ChangeHash> heads;
    heads.reserve(_heads.size());
    for (auto& hash : _heads) {
        if (histroy_index.count(hash)) {
            heads.push_back(hash);
        }
    }

    change_graph.remove_ancestors(changes, heads);
}

std::vector<ChangeHash> Automerge::get_missing_deps(const std::vector<ChangeHash>& heads) const {
    std::unordered_set<ChangeHash> in_queue;
    for (auto& change : queue) {
        in_queue.insert(change.hash);
    }

    // TODO: optimise, use pointer in set
    std::unordered_set<ChangeHash> missing;

    for (auto& change : queue) {
        for (auto& head : change.deps) {
            if (!histroy_index.count(head)) {
                missing.insert(head);
            }
        }
    }

    for (auto& head : heads) {
        if (!histroy_index.count(head)) {
            missing.insert(head);
        }
    }

    std::vector<ChangeHash> missing_deps;
    missing_deps.reserve(missing.size());
    for (auto iter = missing.begin(); iter != missing.end();) {
        if (!in_queue.count(*iter)) {
            // need iter++ as iter will be invalid after extract()
            missing_deps.push_back(std::move(missing.extract(iter++).value()));
        }
        else {
            ++iter;
        }
    }
    std::sort(missing_deps.begin(), missing_deps.end());

    return missing_deps;
}

// Get the changes since `have_deps` in this document using a clock internally.
std::vector<const Change*> Automerge::get_changes_clock(const std::vector<ChangeHash>& have_deps) const {
    // get the clock for the given deps
    auto clock = clock_at(have_deps);

    // get the documents current clock

    std::vector<usize> change_indexes;
    // walk the state from the given deps clock and add them into the vec
    for (auto& [actor_index, actor_changes] : states) {
        auto clock_data = clock.get_for_actor(actor_index);
        if (clock_data) {
            // find the change in this actors sequence of changes that corresponds to the max_op
            // recorded for them in the clock
            change_indexes.insert(change_indexes.end(), actor_changes.begin() + clock_data->seq, actor_changes.end());
        }
        else {
            vector_extend(change_indexes, actor_changes);
        }
    }

    // ensure the changes are still in sorted order
    std::sort(change_indexes.begin(), change_indexes.end());

    std::vector<const Change*> res;
    res.reserve(change_indexes.size());
    for (auto i : change_indexes) {
        res.push_back(&histroy[i]);
    }

    return res;
}

std::optional<Change> Automerge::get_last_local_change() const {
    for (auto iter = histroy.rbegin(); iter != histroy.rend(); ++iter) {
        if (iter->actor_id() == get_actor()) {
            return *iter;
        }
    }

    return {};
}

Clock Automerge::clock_at(const std::vector<ChangeHash>& heads) const {
    return change_graph.clock_for_heads(heads);
}

std::optional<const Change*> Automerge::get_change_by_hash(const ChangeHash& hash) const {
    try {
        auto index = histroy_index.at(hash);
        auto& change = histroy.at(index);
        return { &change };
    }
    catch (std::out_of_range&) {
        return {};
    }
}

std::vector<const Change*> Automerge::get_changes_added(const Automerge& other) const {
    // Depth-first traversal from the heads through the dependency graph,
    // until we reach a change that is already present in other
    auto stack = other.get_heads();
    std::unordered_set<ChangeHash> seen_hashes;
    std::vector<ChangeHash> added_change_hashes;

    while (!stack.empty()) {
        auto hash = vector_pop(stack);
        if (!seen_hashes.count(hash) && !get_change_by_hash(hash)) {
            seen_hashes.insert(hash);
            added_change_hashes.push_back(hash);
            auto change_opt = other.get_change_by_hash(hash);
            if (change_opt) {
                vector_extend(stack, (*change_opt)->deps);
            }
        }
    }

    // Return those changes in the reverse of the order in which the depth-first search
    // found them. This is not necessarily a topological sort, but should usually be close.
    std::reverse(added_change_hashes.begin(), added_change_hashes.end());

    std::vector<const Change*> res;
    for (auto& hash : added_change_hashes) {
        auto change_opt = other.get_change_by_hash(hash);
        if (change_opt) {
            res.push_back(*change_opt);
        }
    }

    return res;
}

std::vector<ChangeHash> Automerge::get_heads() const {
    std::vector<ChangeHash> heads;
    heads.insert(heads.end(), deps.cbegin(), deps.cend());
    std::sort(heads.begin(), heads.end());

    return heads;
}

ChangeHash Automerge::get_hash(usize actor, u64 seq) const {
    try {
        auto& state = states.at(actor);
        auto& index = state.at(seq - 1);
        auto& change = histroy.at(index);
        return change.hash;
    }
    catch (std::out_of_range&) {
        throw AutomergeError{ AutomergeError::InvalidSeq, seq };
    }
}

usize Automerge::update_history(Change&& change, usize num_pos) {
    max_op = std::max(max_op, change.start_op + num_pos - 1);

    update_deps(change);

    usize histroy_index = histroy.size();

    usize actor_index = ops.m.cache_actor(ActorId(change.actor_id()));
    states[actor_index].push_back(histroy_index);

    this->histroy_index.insert({ change.hash, histroy_index });
    change_graph.add_change(change, actor_index);

    histroy.push_back(std::move(change));

    return histroy_index;
}

void Automerge::update_deps(const Change& change) {
    for (auto& d : change.deps) {
        deps.erase(d);
    }
    deps.insert(change.hash);
}

std::pair<ExId, ObjType> Automerge::import(const std::string_view& s) const {
    if (s == "_root") {
        return { ExId(), ObjType::Map };
    }

    auto n = s.find('@');
    if (n == std::string::npos) {
        throw AutomergeError{ AutomergeError::InvalidObjIdFormat, std::string(s) };
    }

    u64 counter = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + n, counter);
    if (ec != std::errc()) {
        throw AutomergeError{ AutomergeError::InvalidObjIdFormat, std::string(s) };
    }

    auto actor = ActorId(std::string_view(s.data() + (n + 1), s.size() - (n + 1)));
    auto actor_index = ops.m.actors.lookup(actor);
    if (!actor_index.has_value()) {
        throw AutomergeError{ AutomergeError::InvalidObjId, std::string(s) };
    }

    auto obj = ExId(counter, ops.m.actors[*actor_index], *actor_index);
    auto obj_type = object_type(obj);

    return { obj, obj_type };
}

// throw AutomergeError
std::pair<ExId, ObjType> Automerge::import_object(const std::string& obj_str) const {
    // valid formats are
    // 123@aabbcc
    // 123@aabccc/prop1/prop2/prop3
    // /prop1/prop2/prop3

    auto index = obj_str.find('/');

    if (index == std::string::npos) {
        return import(obj_str);
    }

    auto parent_id = ExId();
    auto parent_type = ObjType::Map;

    if (index != 0) {
        std::tie(parent_id, parent_type) = import({ obj_str.c_str(), index });
    }

    return import_path(std::move(parent_id), parent_type, { obj_str.c_str() + index, obj_str.length() - index });
}

std::string Automerge::to_string(Export&& id) const {
    switch (id.tag) {
    case Export::Id: {
        auto& op = std::get<OpId>(id.data);
        auto str = std::to_string(op.counter) + '@';
        str.append(ops.m.actors[op.actor].to_hex());
        return str;
    }
    case Export::Prop:
        return std::string(ops.m.props[std::get<usize>(id.data)]);
    case Export::Special:
        return std::get<std::string>(id.data);
    default:
        return {};
    }
}

/////////////////////////////////////////////////////////

std::optional<SyncMessage> Automerge::generate_sync_message(State& sync_state) const {
    auto our_heads = get_heads();

    auto our_need = get_missing_deps(sync_state.their_heads.value_or(std::vector<ChangeHash>()));
    
    // TODO: optimise, use pointer in set
    std::unordered_set<ChangeHash> their_heads_set;
    if (sync_state.their_heads) {
        for (auto& head : *sync_state.their_heads) {
            their_heads_set.insert(head);
        }
    }

    std::vector<Have> our_have;
    if (std::all_of(our_need.cbegin(), our_need.cend(), [&](const ChangeHash& hash) {
        return their_heads_set.count(hash);
        })) {
        our_have.push_back(make_bloom_filter(std::vector(sync_state.shared_heads)));
    }

    if (sync_state.their_have.has_value() && !sync_state.their_have->empty()) {
        auto& last_sync = sync_state.their_have->front().last_sync;
        if (!std::all_of(last_sync.cbegin(), last_sync.cend(), [&](const ChangeHash& hash) {
            return get_change_by_hash(hash).has_value();
            })) {
            return SyncMessage{
                std::move(our_heads),
                {},
                { Have() },
                {}
            };
        }
    }

    std::vector<const Change*> changes_to_send_p;
    if (sync_state.their_have && sync_state.their_need) {
        changes_to_send_p = get_changes_to_send(*sync_state.their_have, *sync_state.their_need);
    }

    bool heads_unchanged = sync_state.last_sent_heads == our_heads;

    bool heads_equal = false;
    if (sync_state.their_heads) {
        heads_equal = *sync_state.their_heads == our_heads;
    }

    // deduplicate the changes to send with those we have already sent and clone it now
    std::vector<Change> changes_to_send;
    for (auto& change : changes_to_send_p) {
        if (!sync_state.sent_hashes.count(change->hash)) {
            changes_to_send.push_back(*change);
        }
    }

    if (heads_unchanged) {
        if (heads_equal&& changes_to_send.empty()) {
            return {};
        }
        if (sync_state.in_flight) {
            return {};
        }
    }

    sync_state.last_sent_heads = our_heads;
    for (auto& c : changes_to_send) {
        sync_state.sent_hashes.insert(c.hash);
    }

    sync_state.in_flight = true;

    return SyncMessage{
        std::move(our_heads),
        std::move(our_need),
        std::move(our_have),
        std::move(changes_to_send)
    };
}

void Automerge::receive_sync_message_with(State& sync_state, SyncMessage&& message, OpObserver* options) {
    auto before_heads = get_heads();

    auto& [message_heads, message_need, message_have, message_changes] = message;

    bool change_is_empty = message_changes.empty();
    if (!change_is_empty) {
        apply_changes_with(std::move(message_changes), options);
        auto new_heads = get_heads();
        sync_state.shared_heads = advance_heads(
            std::unordered_set<ChangeHash>(before_heads.cbegin(), before_heads.cend()),
            std::unordered_set<ChangeHash>(std::make_move_iterator(new_heads.begin()), std::make_move_iterator(new_heads.end())),
            std::move(sync_state.shared_heads)
        );
    }

    // trim down the sent hashes to those that we know they haven't seen
    filter_changes(message_heads, sync_state.sent_hashes);

    if (change_is_empty && (message_heads == before_heads)) {
        sync_state.last_sent_heads = message_heads;
    }

    if (sync_state.sent_hashes.empty()) {
        sync_state.in_flight = false;
    }

    std::vector<const ChangeHash*> known_heads;
    known_heads.reserve(message_heads.size());
    for (auto& head : message_heads) {
        if (get_change_by_hash(head).has_value()) {
            known_heads.push_back(&head);
        }
    }
    if (known_heads.size() == message_heads.size()) {
        sync_state.shared_heads = message_heads;
        sync_state.in_flight = false;
        // If the remote peer has lost all its data, reset our state to perform a full resync
        if (message_heads.empty()) {
            sync_state.last_sent_heads.clear();
            sync_state.sent_hashes.clear();
        }
    }
    else {
        auto& shared = sync_state.shared_heads;
        shared.reserve(shared.size() + known_heads.size());
        for (auto head : known_heads) {
            shared.push_back(*head);
        }

        std::sort(shared.begin(), shared.end());

        auto last = std::unique(shared.begin(), shared.end());
        shared.erase(last, shared.end());
    }

    sync_state.their_have = std::move(message_have);
    sync_state.their_heads = std::move(message_heads);
    sync_state.their_need = std::move(message_need);

    return;
}

Have Automerge::make_bloom_filter(std::vector<ChangeHash>&& last_sync) const {
    auto new_changes = get_changes(last_sync);
    
    std::vector<const ChangeHash*> hashes;
    hashes.reserve(new_changes.size());
    for (auto change : new_changes) {
        hashes.push_back(&change->hash);
    }

    return Have {
        std::move(last_sync),
        BloomFilter(std::move(hashes))
    };
}

std::vector<const Change*> Automerge::get_changes_to_send(const std::vector<Have>& have, const std::vector<ChangeHash>& need) const {
    std::vector<const Change*> changes_to_send;

    if (have.empty()) {
        for (auto& hash : need) {
            auto change = get_change_by_hash(hash);
            if (change) {
                changes_to_send.push_back(*change);
            }
        }

        return changes_to_send;
    }

    // TODO: optimise, use pointer in set
    std::unordered_set<ChangeHash> last_sync_hashes_set;
    std::vector<const BloomFilter*> bloom_filters;
    bloom_filters.reserve(have.size());

    for (auto& h : have) {
        auto& [last_sync, bloom] = h;
        last_sync_hashes_set.insert(last_sync.cbegin(), last_sync.cend());
        bloom_filters.push_back(&bloom);
    }

    std::vector<ChangeHash> last_sync_hashes;
    last_sync_hashes.reserve(last_sync_hashes_set.size());
    for (auto iter = last_sync_hashes_set.begin(); iter != last_sync_hashes_set.end(); ) {
        last_sync_hashes.push_back(std::move(last_sync_hashes_set.extract(iter++).value()));
    }

    auto changes = get_changes(last_sync_hashes);

    std::unordered_set<ChangeHash> change_hashes;
    change_hashes.reserve(changes.size());
    std::unordered_map<ChangeHash, std::vector<ChangeHash>> dependents;
    std::unordered_set<ChangeHash> hashes_to_send;

    for (auto change : changes) {
        change_hashes.insert(change->hash);

        for (auto& dep : change->deps) {
            dependents[dep].push_back(change->hash);
        }

        if (std::all_of(bloom_filters.cbegin(), bloom_filters.cend(), [&](const BloomFilter* bloom) {
            return !bloom->contains_hash(change->hash);
            })) {
            hashes_to_send.insert(change->hash);
        }
    }

    std::vector<ChangeHash> stack(hashes_to_send.cbegin(), hashes_to_send.cend());
    while (!stack.empty()) {
        auto hash = vector_pop(stack);

        auto deps = dependents.find(hash);
        if (deps != dependents.end()) {
            for (auto& dep : deps->second) {
                if (hashes_to_send.insert(dep).second) {
                    stack.push_back(std::move(dep));
                }
            }
        }
    }

    for (auto& hash : need) {
        hashes_to_send.insert(hash);
        if (!change_hashes.count(hash)) {
            auto change = get_change_by_hash(hash);
            if (change) {
                changes_to_send.push_back(*change);
            }
        }
    }

    for (auto& change : changes) {
        if (hashes_to_send.count(change->hash)) {
            changes_to_send.push_back(change);
        }
    }

    return changes_to_send;
}

/////////////////////////////////////////////////////////

static json map_to_json(const Automerge& doc, const ExId& obj);
static json list_to_json(const Automerge& doc, const ExId& obj);
static Automerge json_to_automerge(const json& value);

void to_json(json& j, const Automerge& doc) {
    j = map_to_json(doc, ExId());
}

void from_json(const json& j, Automerge& doc) {
    doc.merge(json_to_automerge(j));
}

/////////////////////////////////////////////////////////

static json scalar_to_json(ScalarValue& value) {
    switch (value.tag) {
    case ScalarValue::Bytes:
        return json::binary(std::get<std::vector<u8>>(value.data));
    case ScalarValue::Str:
        return json(std::get<std::string>(value.data));
    case ScalarValue::Int:
    case ScalarValue::Timestamp:
        return json(std::get<s64>(value.data));
    case ScalarValue::Uint:
        return json(std::get<u64>(value.data));
    case ScalarValue::F64:
        return json(std::get<double>(value.data));
    case ScalarValue::Counter:
        return json(std::get<Counter>(value.data).current);
    case ScalarValue::Boolean:
        return json(std::get<bool>(value.data));
    case ScalarValue::Null: 
    default:
        return json();
    }
}

json map_to_json(const Automerge& doc, const ExId& obj) {
    try {
        auto keys = doc.keys(obj);
        json map = json::object();

        std::optional<std::string> key;
        while ((key = keys.next())) {
            auto val = doc.get(obj, Prop(std::string(*key)));
            if (!val.has_value()) {
                continue;
            }
            auto& value = val->second;

            if (value.tag == Value::SCALAR) {
                map.push_back(json::object_t::value_type(*key, scalar_to_json(std::get<ScalarValue>(value.data))));
                continue;
            }

            auto& obj_type = std::get<ObjType>(value.data);
            if (obj_type == ObjType::Map || obj_type == ObjType::Table) {
                map.push_back(json::object_t::value_type(*key, map_to_json(doc, val->first)));
                continue;
            }

            if (obj_type == ObjType::List) {
                map.push_back(json::object_t::value_type(*key, list_to_json(doc, val->first)));
                continue;
            }

            if (obj_type == ObjType::Text) {
                map.push_back(json::object_t::value_type(*key, doc.text(val->first)));
                continue;
            }
        }

        return map;
    }
    catch (std::exception&) {
        return json();
    }
}

json list_to_json(const Automerge& doc, const ExId& obj) {
    try {
        auto len = doc.length(obj);
        json list = json::array();

        for (usize i = 0; i < len; ++i) {
            auto val = doc.get(obj, Prop(i));
            if (!val.has_value()) {
                continue;
            }
            auto& value = val->second;

            if (value.tag == Value::SCALAR) {
                list.push_back(scalar_to_json(std::get<ScalarValue>(value.data)));
                continue;
            }

            auto& obj_type = std::get<ObjType>(value.data);
            if (obj_type == ObjType::Map || obj_type == ObjType::Table) {
                list.push_back(map_to_json(doc, val->first));
                continue;
            }

            if (obj_type == ObjType::List) {
                list.push_back(list_to_json(doc, val->first));
                continue;
            }

            if (obj_type == ObjType::Text) {
                list.push_back(doc.text(val->first));
                continue;
            }
        }

        return list;
    }
    catch (std::exception&) {
        return json();
    }
}

/////////////////////////////////////////////////////////

// TODO: use cache to improve performance
JsonPathParsed Automerge::json_pointer_parse(ExId&& prefix_id, ObjType prefix_type, const json::json_pointer& path) const {
    if (path.empty()) {
        // not care parent's PropPair
        return JsonPathParsed{ JsonPathParsed::ExistedPath,
            std::make_tuple(std::move(prefix_id), Value{Value::OBJECT, prefix_type}, std::make_pair(ExId(), Prop())) };
    }

    // parse the parent path recursively
    auto parent_path = path.parent_pointer();
    auto parent_obj = json_pointer_parse(std::move(prefix_id), prefix_type, parent_path);

    // parent path should be valid and existed
    if (parent_obj.tag == JsonPathParsed::Invalid) {
        // TODO: err: "invalid path"
        return parent_obj;
    }
    if (parent_obj.tag == JsonPathParsed::NewPath) {
        // err: "parent path not found"
        return JsonPathParsed{ JsonPathParsed::Invalid, parent_path.to_string() };
    }

    // parent path should be an object
    auto& [parent_id, parent_value, parent_prop] = std::get<ValueTuple>(parent_obj.data);
    if (parent_value.tag == Value::SCALAR) {
        // err: "not an object"
        return JsonPathParsed{ JsonPathParsed::Invalid, parent_path.to_string() };
    }

    // get the item's prop from the path
    Prop prop;
    auto& parent_obj_type = std::get<ObjType>(parent_value.data);
    if (parent_obj_type == ObjType::Map) {
        prop = Prop(std::string(path.back()));
    }
    else if (parent_obj_type == ObjType::List) {
        try {
            prop = Prop(std::stol(path.back()));
        }
        catch (std::exception&) {
            // err: "not a path of an array"
            return JsonPathParsed{ JsonPathParsed::Invalid, path.to_string() };
        }
    }
    else {
        // TODO: Table and Text not supported yet
        return JsonPathParsed{ JsonPathParsed::Invalid, parent_path.to_string() };
    }

    // find the item on parent path by prop
    auto item = get(parent_id, Prop(prop));

    // return parsed result
    auto prop_pair = std::make_pair(std::move(parent_id), std::move(prop));
    if (!item.has_value()) {
        return JsonPathParsed{ JsonPathParsed::NewPath, std::move(prop_pair) };
    }
    else {
        return JsonPathParsed{ JsonPathParsed::ExistedPath,
            std::make_tuple(std::move(item->first), std::move(item->second), std::move(prop_pair)) };
    }
}

// interop.rs: import_prop. not used now
/*
static std::optional<Prop> json_to_prop(const json& p) {
    if (p.is_string()) {
        return Prop(p.get<std::string>());
    }

    if (p.is_number_float()) {
        return Prop((usize)p.get<double>());
    }

    // err: "prop must me a string or number"
    return {};
}
*/

// interop.rs: import_scalar
static std::optional<ScalarValue> json_to_scalar(const json& value, const std::optional<std::string>& datatype) {
    // ignore datatype

    if (value.is_null()) {
        return ScalarValue{ ScalarValue::Null, {} };
    }

    if (value.is_boolean()) {
        return ScalarValue{ ScalarValue::Boolean, value.get<bool>() };
    }

    if (value.is_string()) {
        return ScalarValue{ ScalarValue::Str, value.get<std::string>() };
    }

    if (value.is_number()) {
        if (value.is_number_float()) {
            return ScalarValue{ ScalarValue::F64, value.get<double>() };
        }

        return ScalarValue{ ScalarValue::Int, value.get<s64>() };
    }

    if (value.is_binary()) {
        return ScalarValue{ ScalarValue::Bytes, value.get<std::vector<u8>>() };
    }

    return {};
}

// interop.rs: import_obj
static auto json_to_object(const json& value, const std::optional<std::string>& datatype)
-> std::optional<std::pair<ObjType, std::list<std::pair<Prop, json>>>> {
    // ignore datatype

    if (value.is_object()) {
        auto obj = value.get<std::unordered_map<std::string, json>>();
        std::list<std::pair<Prop, json>> map;

        for (auto& [key, val] : obj) {
            map.emplace_back(Prop(std::string(key)), std::move(val));
        }

        return std::make_pair(ObjType::Map, std::move(map));
    }

    if (value.is_array()) {
        auto vec = value.get<std::vector<json>>();
        std::list<std::pair<Prop, json>> list;

        for (usize i = 0; i < vec.size(); ++i) {
            // treat an Automerge list as a singly linked list, insert items from front in reverse order use O(N) time
            list.emplace_back(Prop(0), std::move(vec[vec.size() - i - 1]));
        }

        return std::make_pair(ObjType::List, std::move(list));
    }

    // TODO: text

    return {};
}

// interop.rs: import_value
static auto json_to_value(const json& value, const std::optional<std::string>& datatype)
-> std::optional<std::pair<Value, std::list<std::pair<Prop, json>>>> {
    // ignore datatype

    if (!value.is_structured()) {
        auto scalar = json_to_scalar(value, {});
        if (scalar) {
            return std::make_pair(Value{ Value::SCALAR, std::move(*scalar) }, std::list<std::pair<Prop, json>>());
        }
    }
    else {
        auto object = json_to_object(value, {});
        if (object) {
            return std::make_pair(Value{ Value::OBJECT, object->first }, std::move(object->second));
        }
    }

    // err: "invalid parent_value"
    return {};
}

// lib.rs: subset
static void json_to_transaction(const ExId& obj, std::list<std::pair<Prop, json>>& vals, Transaction& tx) {
    for (auto iter = vals.begin(); iter != vals.end();) {
        Prop p = iter->first;
        auto parsed_value = json_to_value(iter->second, {});

        // delete current parent_value from list, and move iter to next one
        iter = vals.erase(iter);

        // invalid parent_value, ignore this key-parent_value
        if (!parsed_value.has_value()) {
            continue;
        }

        auto& [value, subvals] = *parsed_value;
        std::optional<ExId> opid = {};
        if (value.tag == Value::OBJECT) {
            auto& objtype = std::get<ObjType>(value.data);
            if (p.tag == Prop::Map) {
                opid = tx.put_object(obj, std::move(p), objtype);
            }
            else {
                opid = tx.insert_object(obj, std::get<usize>(p.data), objtype);
            }
        }
        else {
            auto& scalar = std::get<ScalarValue>(value.data);
            if (p.tag == Prop::Map) {
                tx.put(obj, std::move(p), std::move(scalar));
            }
            else {
                tx.insert(obj, std::get<usize>(p.data), std::move(scalar));
            }
        }

        if (opid) {
            json_to_transaction(*opid, subvals, tx);
        }
    }
}

Automerge json_to_automerge(const json& value) {
    Automerge doc;

    // set the initial ActorId to 0
    doc.set_actor(ActorId(false));

    if (value.is_null()) {
        // empty json
        return doc;
    }

    // parse json ROOT
    auto parsed_value = json_to_value(value, {});
    if (!parsed_value.has_value() || (parsed_value->first.tag != Value::OBJECT) ||
        (std::get<ObjType>(parsed_value->first.data) != ObjType::Map)) {
        // Automerge json ROOT should be a map
        return doc;
    }

    if (parsed_value->second.empty()) {
        // empty json
        return doc;
    }

    Transaction tx = doc.transaction();
    json_to_transaction(ExId(), parsed_value->second, tx);

    tx.commit();

    return doc;
}

/////////////////////////////////////////////////////////

// throw AutomergeError
ExId Automerge::put_object(const ExId& obj, Prop&& prop, const std::string& value_str) {
    try {
        auto value = json::parse(value_str);

        // parse json value
        auto parsed_value = json_to_value(value, {});
        if (!parsed_value.has_value() || parsed_value->first.tag != Value::OBJECT) {
            throw std::runtime_error("invalid json object[" + value_str + "]");
        }

        ensure_transaction_open();

        return json_replacing({ obj, prop }, std::move(*parsed_value));

        // TODO: undo/redo
    }
    catch (const json::exception&) {
        throw std::runtime_error("invalid json value[" + value_str + "]");
    }
}

// throw AutomergeError
ExId Automerge::insert_object(const ExId& obj, usize index, const std::string& value_str) {
    try {
        auto value = json::parse(value_str);

        // parse json value
        auto parsed_value = json_to_value(value, {});
        if (!parsed_value.has_value() || parsed_value->first.tag != Value::OBJECT) {
            throw std::runtime_error("invalid json object[" + value_str + "]");
        }

        ensure_transaction_open();

        return json_adding({ obj, Prop(index) }, std::move(*parsed_value));

        // TODO: undo/redo
    }
    catch (const json::exception&) {
        throw std::runtime_error("invalid json value[" + value_str + "]");
    }
}

/////////////////////////////////////////////////////////

// insert, put, insert_object, put_object
void Automerge::json_add(const json::json_pointer& path, const json& value) {
    // TODO: add parent_value type to distinguish string and text, int and uint

    // parse json value
    auto parsed_value = json_to_value(value, {});
    if (!parsed_value.has_value()) {
        throw std::runtime_error("invalid json value[" + value.dump() + "]");
    }

    // parse the path to find the place to insert
    auto item = json_pointer_parse(ExId(), ObjType::Map, path);

    // the parent path should exist
    if (item.tag == JsonPathParsed::Invalid) {
        throw std::runtime_error("path[" + std::get<std::string>(item.data) + "] not found");
    }

    // get the object id of the parent path and the property of the path
    auto& item_data = (item.tag == JsonPathParsed::ExistedPath) ?
        std::get<PropPair>(std::get<ValueTuple>(item.data)) : std::get<PropPair>(item.data);

    // insert into a map, the path should not exist
    if ((item_data.second.tag == Prop::Map) && (item.tag == JsonPathParsed::ExistedPath)) {
        throw std::runtime_error("object[" + path.parent_pointer().to_string() + "] already has item["
            + std::get<2>(std::get<ValueTuple>(item.data)).second.to_string() + "]");
    }

    ensure_transaction_open();

    // update automerge doc
    json_adding(item_data, std::move(*parsed_value));

    // update json doc
    if (item_data.second.tag == Prop::Seq) {
        // add an item into an array of json
        auto& parent_ref = json_doc[path.parent_pointer()];
        auto index = std::stoi(path.back());
        parent_ref.insert(parent_ref.begin() + index, value);
    }
    else {
        // add an item into a map of json
        json_doc[path] = value;
    }
}

// add an item in a list, add/replace an item in a map
ExId Automerge::json_adding(const PropPair& item, std::pair<Value, std::list<std::pair<Prop, json>>>&& value) {
    auto& [parent_id, prop] = item;
    auto& [root_value, vals] = value;

    // insert scalar at the place parsed above
    if (root_value.tag == Value::SCALAR) {
        auto& scalar = std::get<ScalarValue>(root_value.data);
        // insert in a list
        if (prop.tag == Prop::Seq) {
            _transaction->insert(parent_id, std::get<usize>(prop.data), std::move(scalar));
        }
        // insert in a map
        else {
            _transaction->put(parent_id, Prop(prop), std::move(scalar));
        }

        return parent_id;
    }
    // insert object at the place parsed above
    else {
        // First, create root object with the object id
        ExId root_id;
        auto& root_obj_type = std::get<ObjType>(root_value.data);

        // insert an object in a list
        if (prop.tag == Prop::Seq) {
            root_id = _transaction->insert_object(parent_id, std::get<usize>(prop.data), root_obj_type);
        }
        // insert an object in a map
        else {
            root_id = _transaction->put_object(parent_id, Prop(prop), root_obj_type);
        }

        // Second, add sub items under the root_id
        json_to_transaction(root_id, vals, *_transaction);

        return root_id;
    }
}

// put, put_object
void Automerge::json_replace(const json::json_pointer& path, const json& value) {
    // parse json value
    auto parsed_value = json_to_value(value, {});
    if (!parsed_value.has_value()) {
        throw std::runtime_error("invalid json value[" + value.dump() + "]");
    }

    // parse the path to find the item
    auto item = json_pointer_parse(ExId(), ObjType::Map, path);
    if (item.tag == JsonPathParsed::Invalid) {
        throw std::runtime_error("path[" + std::get<std::string>(item.data) + "] not found");
    }
    if (item.tag == JsonPathParsed::NewPath) {
        throw std::runtime_error("object[" + path.parent_pointer().to_string() + "] has no item["
            + std::get<PropPair>(item.data).second.to_string() + "]");
    }

    // get the object id of the parent path and the property of the path
    auto& prop_pair = std::get<PropPair>(std::get<ValueTuple>(item.data));

    ensure_transaction_open();

    // update automerge doc
    json_replacing(prop_pair, std::move(*parsed_value));

    // update json doc
    auto& item_ref = json_doc[path];
    item_ref = value;
}

// replace an item in a list, add/replace an item in a map
ExId Automerge::json_replacing(const PropPair& item, std::pair<Value, std::list<std::pair<Prop, json>>>&& value) {
    auto& [parent_id, prop] = item;
    auto& [root_value, vals] = value;

    // replace to object
    if (root_value.tag == Value::OBJECT) {
        // replace the original item to a new empty object
        ExId root_id = _transaction->put_object(parent_id, Prop(prop), std::get<ObjType>(root_value.data));

        // add sub items under the new object
        json_to_transaction(root_id, vals, *_transaction);

        return root_id;
    }
    // replace to scalar
    else {
        auto& scalar = std::get<ScalarValue>(root_value.data);
        _transaction->put(parent_id, Prop(prop), std::move(scalar));

        return parent_id;
    }
}

void Automerge::json_delete(const json::json_pointer& path) {
    // parse the path to find the item
    auto item = json_pointer_parse(ExId(), ObjType::Map, path);
    if (item.tag == JsonPathParsed::Invalid) {
        throw std::runtime_error("path[" + std::get<std::string>(item.data) + "] not found");
    }
    if (item.tag == JsonPathParsed::NewPath) {
        throw std::runtime_error("object[" + path.parent_pointer().to_string() + "] has no item["
            + std::get<PropPair>(item.data).second.to_string() + "]");
    }

    // get the object id of the parent path and the property of the path
    auto& [parent_id, prop] = std::get<PropPair>(std::get<ValueTuple>(item.data));

    ensure_transaction_open();

    // update automerge doc
    _transaction->delete_(parent_id, std::move(prop));

    // update json doc
    try {
        auto& parent_ref = json_doc.at(path.parent_pointer());
        if (prop.tag == Prop::Seq) {
            // delete an item from an array of json
            auto index = std::stoi(path.back());
            parent_ref.erase(index);
        }
        else {
            // delete an item from a map of json
            auto& key = path.back();
            parent_ref.erase(key);
        }
    }
    catch (std::exception&) {
        throw std::runtime_error("invalid path: " + path.to_string());
    }
}

std::pair<ExId, ObjType> Automerge::import_path(
    ExId&& prefix_id, ObjType prefix_type, const std::string_view& obj_path
) const {
    try {
        json::json_pointer json_path = (obj_path == "/") ? json::json_pointer() : json::json_pointer(std::string(obj_path));
        auto item = json_pointer_parse(std::move(prefix_id), prefix_type, json_path);
        if (item.tag != JsonPathParsed::ExistedPath) {
            throw;
        }

        auto& [id, value, prop] = std::get<ValueTuple>(item.data);
        if (value.tag != Value::OBJECT) {
            throw;
        }

        return { std::move(id), std::get<ObjType>(value.data) };
    }
    catch (const std::exception&) {
        throw std::invalid_argument("invalid path" + std::string(obj_path));
    }
}
