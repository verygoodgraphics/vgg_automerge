#include <gtest/gtest.h>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "Automerge.h"

namespace fs = std::filesystem;

class AutomergeTest : public ::testing::Test {
protected:
    void SetUp() override {
        start_time_ = time(nullptr);
    }

    void TearDown() override {
        // Gets the time when the test finishes
        const time_t end_time = time(nullptr);

        // Asserts that the test took no more than ~5 seconds.
        EXPECT_TRUE(end_time - start_time_ <= 5) << "The test took too long.";
    }

    // The UTC time (in seconds) when the test starts
    time_t start_time_ = 0;
};

TEST_F(AutomergeTest, QuickStart) {
    Automerge doc1;
    auto result = doc1.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(std::string("Add card"), {}, {}); },
        [](Transaction& tx) -> std::vector<ExId> {
            ExId cards = tx.put_object(ExId(), Prop("cards"), ObjType::List);
            ExId card1 = tx.insert_object(cards, 0, ObjType::Map);
            tx.put(card1, Prop("title"), ScalarValue{ ScalarValue::Str, std::string("Rewrite everything in Clojure") });
            tx.put(card1, Prop("done"), ScalarValue{ ScalarValue::Boolean, false });

            ExId card2 = tx.insert_object(cards, 0, ObjType::Map);
            tx.put(card2, Prop("title"), ScalarValue{ ScalarValue::Str, std::string("Rewrite everything in Haskell") });
            tx.put(card2, Prop("done"), ScalarValue{ ScalarValue::Boolean, false });

            return { cards, card1 };
        }
    );
    ExId& cards = result.first[0];
    ExId& card1 = result.first[1];

    Automerge doc3;
    doc3.merge(doc1);

    auto binary = doc1.save();
    Automerge doc2 = Automerge::load({ binary.cbegin(), binary.size() });

    doc1.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(std::string("Mark card as done"), {}, {}); },
        [&](Transaction& tx) -> std::vector<ExId> {
            tx.put(card1, Prop("done"), ScalarValue{ ScalarValue::Boolean, true });

            return {};
        }
    );

    doc2.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(std::string("Delete card"), {}, {}); },
        [&](Transaction& tx) -> std::vector<ExId> {
            tx.delete_(cards, Prop(0));

            return {};
        }
    );

    doc1.merge(doc2);

    EXPECT_EQ(json::parse(R"({
    "cards": [
        {
            "title": "Rewrite everything in Clojure",
            "done": true
        }
    ]
})"), json(doc1));

    EXPECT_EQ(json::parse(R"({
    "cards": [
        {
            "title": "Rewrite everything in Clojure",
            "done": false
        }
    ]
})"), json(doc2));

    EXPECT_EQ(json::parse(R"({
    "cards": [
        {
            "title": "Rewrite everything in Haskell",
            "done": false
        },
        {
            "title": "Rewrite everything in Clojure",
            "done": false
        }
    ]
})"), json(doc3));
}

TEST_F(AutomergeTest, DirectEdit) {
    auto doc = Automerge();
    doc.put_object(doc.import_object("/").first, Prop("todo"),
        R"([{"title": "work A", "done": false}, {"title": "work B", "done": true}])");

    doc.commit();

    EXPECT_EQ(json::parse(R"({
    "todo": [
        {
            "title": "work A",
            "done": false
        },
        {
            "title": "work B",
            "done": true
        }
    ]
})"), json(doc));
}

TEST_F(AutomergeTest, JsonDemo) {
    json json_obj = json::parse(R"({
    "todo": [
        {
            "title": "work A",
            "done": false
        },
        {
            "title": "work B",
            "done": true
        }
    ]
})");

    // from_json
    Automerge automerge_doc = json_obj.get<Automerge>();

    // to_json
    json new_json_obj = automerge_doc;

    EXPECT_EQ(json_obj, new_json_obj);

    // json_add
    json work_C = { {"title", "work C"}, {"done", false} };
    automerge_doc.json_add("/todo/0"_json_pointer, work_C);
    automerge_doc.commit();

    // json_replace
    automerge_doc.json_replace("/todo/1/done"_json_pointer, true);
    automerge_doc.commit();

    // json_delete
    automerge_doc.json_delete("/todo/2"_json_pointer);
    automerge_doc.commit();

    EXPECT_EQ(json::parse(R"({
    "todo": [
        {
            "title": "work C",
            "done": false
        },
        {
            "title": "work A",
            "done": true
        }
    ]
})"), json(automerge_doc));
}

TEST_F(AutomergeTest, JsonEdit) {
    auto doc = json::parse(R"({
    "cards": [
        {
            "title": "Rewrite everything in Haskell",
            "done": false
        },
        {
            "title": "Rewrite everything in Clojure",
            "done": false
        }
    ]
})");
    auto doc1 = doc.get<Automerge>();

    // json_delete
    doc1.json_delete("/cards/1/done"_json_pointer);

    // json_add
    json item3 = { {"title", "yyy"}, {"done", true} };
    doc1.json_add("/cards/1"_json_pointer, item3);

    // json_replace1, scalar to scalar
    doc1.json_replace("/cards/0/done"_json_pointer, true);
    // json_replace2, scalar to object
    doc1.json_replace("/cards/2/title"_json_pointer, { nullptr, { "test", "ok"}, 1.5, {{"n1", -4}, {"n2", {{"tmp", 9}}}} });

    doc1.commit();

    EXPECT_EQ(json::parse(R"({
    "cards": [
        {
            "title": "Rewrite everything in Haskell",
            "done": true
        },
        {
            "title": "yyy",
            "done": true
        },
        {
            "title": [
                null,
                [ "test", "ok" ],
                1.5,
                {
                    "n1": -4,
                    "n2": { "tmp": 9 }
                }
            ]
        }
    ]
})"), json(doc1));

    // json_replace3, object to scalar
    doc1.json_replace("/cards/2/title/3/n2"_json_pointer, false);
    // json_replace4, object to object
    doc1.json_replace("/cards/2/title/1"_json_pointer, { { "test", -9} });

    doc1.commit();

    EXPECT_EQ(json::parse(R"({
    "cards": [
        {
            "title": "Rewrite everything in Haskell",
            "done": true
        },
        {
            "title": "yyy",
            "done": true
        },
        {
            "title": [
                null,
                { "test": -9 },
                1.5,
                {
                    "n1": -4,
                    "n2": false
                }
            ]
        }
    ]
})"), json(doc1));
}

TEST_F(AutomergeTest, HexString) {
    json json_obj = json::parse(R"({
    "cards": [
        {
            "title": "Rewrite everything in Haskell",
            "done": false
        },
        {
            "title": "Rewrite everything in Clojure",
            "done": false
        }
    ]
})");

    // binary of an automerge doc which will generate a json like json_obj
    std::string_view hex_str = "856f4a83cfce556500fa0101103c2b36846c3041b99bcf1dfbb8a029e20169fba6b9d5ac0327ea6cb4b255866c9e1a24193024ad336c759eab6fd6f2958c070102030213022302350a400256020b010402081306152021022308340342065609573a8001027f007f017f077f007f0841646420636172647f007f070001060000010201020202050001020000047f05636172647300027c04646f6e65057469746c6504646f6e65057469746c6507007901047d027f047f0102047f020200040103007c01d60301d603526577726974652065766572797468696e6720696e20436c6f6a757265526577726974652065766572797468696e6720696e204861736b656c6c0700";

    // load Automerge doc from the hex string
    std::vector<u8> bin_vec;
    bin_vec.reserve(hex_str.size() / 2);
    auto hex_2_u8 = [](const char ch)->u8 {
        if (ch >= 'a') {
            return ch - 'a' + 10;
        }
        else {
            return ch - '0';
        }
    };
    for (auto hex_ch = hex_str.cbegin(); hex_ch != hex_str.cend(); hex_ch += 2) {
        bin_vec.push_back((hex_2_u8(hex_ch[0]) << 4) + hex_2_u8(hex_ch[1]));
    }
    Automerge doc = Automerge::load({ bin_vec.cbegin(), bin_vec.size() });

    // save the Automerge doc back to binary
    auto binary = doc.save();

    EXPECT_EQ(json_obj, json(doc));
    // the doc's ActorId is new randomly generated, however it's not used as no new operations committed.
    // so, the binary is the same
    EXPECT_EQ(bin_vec, binary);
}

/////////////////////////////////////////////////////////
// automerge/tests/test.rs
/////////////////////////////////////////////////////////

auto get_all_to_set(const Automerge& doc, const ExId& obj, Prop&& prop) {
    std::unordered_multiset<std::string> set;

    for (auto& vp : doc.get_all(obj, std::move(prop))) {
        auto& value = vp.second;
        if (value.tag == Value::OBJECT) {
            auto& type = std::get<ObjType>(value.data);
            if (type == ObjType::List) {
                set.insert("o_list");
            }
            else {
                set.insert("o_map");
            }
        }
        else {
            set.insert(std::get<ScalarValue>(value.data).to_string());
        }
    }

    return set;
}

TEST_F(AutomergeTest, NoConflictOnRepeatedAssignment) {
    Automerge doc;
    doc.put(ExId(), Prop("foo"), ScalarValue{ ScalarValue::Int, (s64)1 });
    doc.put(ExId(), Prop("foo"), ScalarValue{ ScalarValue::Int, (s64)2 });
    doc.commit();

    EXPECT_EQ(json::parse(R"({
    "foo": 2
})"), json(doc));
}

TEST_F(AutomergeTest, RepeatedMapAssignmentWhichResolvesConflictNotIgnored) {
    Automerge doc1;
    Automerge doc2;

    doc1.put(ExId(), Prop("field"), ScalarValue{ ScalarValue::Int, (s64)123 });
    doc1.commit();

    doc2.merge(doc1);

    doc2.put(ExId(), Prop("field"), ScalarValue{ ScalarValue::Int, (s64)456 });
    doc2.commit();

    doc1.put(ExId(), Prop("field"), ScalarValue{ ScalarValue::Int, (s64)789 });
    doc1.commit();

    doc1.merge(doc2);

    EXPECT_EQ(2, doc1.get_all(ExId(), Prop("field")).size());

    doc1.put(ExId(), Prop("field"), ScalarValue{ ScalarValue::Int, (s64)123 });
    doc1.commit();

    EXPECT_EQ(json::parse(R"({
    "field": 123
})"), json(doc1));
}

TEST_F(AutomergeTest, RepeatedListAssignmentWhichResolvesConflictNotIgnored) {
    Automerge doc1;
    Automerge doc2;

    auto list_id = doc1.put_object(ExId(), Prop("list"), ObjType::List);

    doc1.insert(list_id, 0, ScalarValue{ ScalarValue::Int, (s64)123 });
    doc1.commit();

    doc2.merge(doc1);

    doc2.put(list_id, Prop(0), ScalarValue{ ScalarValue::Int, (s64)456 });
    doc2.commit();

    doc1.merge(doc2);

    doc1.put(list_id, Prop(0), ScalarValue{ ScalarValue::Int, (s64)789 });
    doc1.commit();

    EXPECT_EQ(json::parse(R"({
    "list": [ 789 ]
})"), json(doc1));
}

TEST_F(AutomergeTest, ListDeletion) {
    Automerge doc;
    auto list_id = doc.put_object(ExId(), Prop("list"), ObjType::List);

    doc.insert(list_id, 0, ScalarValue{ ScalarValue::Int, (s64)123 });
    doc.insert(list_id, 1, ScalarValue{ ScalarValue::Int, (s64)456 });
    doc.insert(list_id, 2, ScalarValue{ ScalarValue::Int, (s64)789 });
    doc.delete_(list_id, Prop(1));
    doc.commit();

    EXPECT_EQ(json::parse(R"({
    "list": [ 123, 789 ]
})"), json(doc));
}

TEST_F(AutomergeTest, MergeConcurrentMapPropUpdates) {
    Automerge doc1;
    Automerge doc2;

    doc1.put(ExId(), Prop("foo"), ScalarValue{ ScalarValue::Str, std::string("bar")});
    doc1.commit();

    doc2.put(ExId(), Prop("hello"), ScalarValue{ ScalarValue::Str, std::string("world") });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ((ScalarValue{ ScalarValue::Str, std::string("bar") }),
        std::get<ScalarValue>(doc1.get(ExId(), Prop("foo"))->second.data));

    EXPECT_EQ(json::parse(R"({
    "foo": "bar",
    "hello": "world"
})"), json(doc1));

    doc2.merge(doc1);

    EXPECT_EQ(json::parse(R"({
    "foo": "bar",
    "hello": "world"
})"), json(doc2));
}

// increment not implement
TEST_F(AutomergeTest, AddConcurrentIncrementsOfSameProperty) {
    Automerge doc1;
    Automerge doc2;

    doc1.put(ExId(), Prop("counter"), ScalarValue{ ScalarValue::Counter, Counter(0) });
    doc1.commit();
    doc2.merge(doc1);

    doc1.increment(ExId(), Prop("counter"), 1);
    doc1.commit();
    doc2.increment(ExId(), Prop("counter"), 2);
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ(json::parse(R"({
    "counter": 3
})"), json(doc1));
}

TEST_F(AutomergeTest, ConcurrentUpdatesOfSameField) {
    Automerge doc1;
    Automerge doc2;

    doc1.put(ExId(), Prop("field"), ScalarValue{ ScalarValue::Str, std::string("one") });
    doc1.commit();

    doc2.put(ExId(), Prop("field"), ScalarValue{ ScalarValue::Str, std::string("two") });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ((std::unordered_multiset<std::string>{ "one", "two" }), 
        get_all_to_set(doc1, ExId(), Prop("field")));
}

TEST_F(AutomergeTest, ConcurrentUpdatesOfSameListElement) {
    Automerge doc1;
    Automerge doc2;
    auto list_id = doc1.put_object(ExId(), Prop("birds"), ObjType::List);

    doc1.insert(list_id, 0, ScalarValue{ ScalarValue::Str, std::string("finch") });
    doc1.commit();
    doc2.merge(doc1);

    doc1.put(list_id, Prop(0), ScalarValue{ ScalarValue::Str, std::string("greenfinch") });
    doc1.commit();
    doc2.put(list_id, Prop(0), ScalarValue{ ScalarValue::Str, std::string("goldfinch") });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ((std::unordered_multiset<std::string>{ "greenfinch", "goldfinch" }),
        get_all_to_set(doc1, list_id, Prop(0)));
}

TEST_F(AutomergeTest, AssignmentConflictsOfDifferentTypes) {
    Automerge doc1;
    Automerge doc2;
    Automerge doc3;

    doc1.put(ExId(), Prop("field"), ScalarValue{ ScalarValue::Str, std::string("string") });
    doc1.commit();

    doc2.put_object(ExId(), Prop("field"), ObjType::List);
    doc2.commit();
    
    doc3.put_object(ExId(), Prop("field"), ObjType::Map);
    doc3.commit();

    doc1.merge(doc2);
    doc1.merge(doc3);

    EXPECT_EQ((std::unordered_multiset<std::string>{ "string", "o_list", "o_map" }),
        get_all_to_set(doc1, ExId(), Prop("field")));
}

TEST_F(AutomergeTest, ChangesWithinConflictingMapField) {
    Automerge doc1;
    Automerge doc2;

    doc1.put(ExId(), Prop("field"), ScalarValue{ ScalarValue::Str, std::string("string") });
    doc1.commit();

    auto map_id = doc2.put_object(ExId(), Prop("field"), ObjType::Map);
    doc2.put(map_id, Prop("innerKey"), ScalarValue{ ScalarValue::Int, (s64)42 });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ((std::unordered_multiset<std::string>{ "string", "o_map" }),
        get_all_to_set(doc1, ExId(), Prop("field")));

    json j1 = doc1;
    EXPECT_TRUE(json::parse(R"({
    "field": "string"
})") == j1 
    || json::parse(R"({
    "field": {
        "innerKey": 42
    }
})") == j1);
}

TEST_F(AutomergeTest, ChangesWithinConflictingListElement) {
    Automerge doc1;
    Automerge doc2;
    doc1.set_actor(ActorId(std::string("01234567")));
    doc2.set_actor(ActorId(std::string("89abcdef")));

    auto list_id = doc1.put_object(ExId(), Prop("list"), ObjType::List);
    doc1.insert(list_id, 0, ScalarValue{ ScalarValue::Str, std::string("hello") });
    doc1.commit();
    doc2.merge(doc1);

    auto map_in_doc1 = doc1.put_object(list_id, Prop(0), ObjType::Map);
    doc1.put(map_in_doc1, Prop("map1"), ScalarValue{ ScalarValue::Boolean, true });
    doc1.put(map_in_doc1, Prop("key"), ScalarValue{ ScalarValue::Int, (s64)1 });
    doc1.commit();

    auto map_in_doc2 = doc2.put_object(list_id, Prop(0), ObjType::Map);
    doc2.put(map_in_doc2, Prop("map2"), ScalarValue{ ScalarValue::Boolean, true });
    doc2.put(map_in_doc2, Prop("key"), ScalarValue{ ScalarValue::Int, (s64)2 });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ(json::parse(R"({
    "list": [{
        "map1": true,
        "key": 1
    }]
})"), doc1);
}

TEST_F(AutomergeTest, ConcurrentlyAssignedNestedMapsShouldNotMerge) {
    Automerge doc1;
    Automerge doc2;

    auto doc1_map_id = doc1.put_object(ExId(), Prop("config"), ObjType::Map);
    doc1.put(doc1_map_id, Prop("background"), ScalarValue{ ScalarValue::Str, std::string("blue") });
    doc1.commit();

    auto doc2_map_id = doc2.put_object(ExId(), Prop("config"), ObjType::Map);
    doc2.put(doc2_map_id, Prop("logo_url"), ScalarValue{ ScalarValue::Str, std::string("logo.png") });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ((std::unordered_multiset<std::string>{ "o_map", "o_map" }),
        get_all_to_set(doc1, ExId(), Prop("config")));

    json j1 = doc1;
    EXPECT_TRUE(json::parse(R"({
    "config": {
        "background": "blue"
    }
})") == j1
|| json::parse(R"({
    "config": {
        "logo_url": "logo.png"
    }
})") == j1);
}

TEST_F(AutomergeTest, ConcurrentInsertionsAtDifferentListPositions) {
    Automerge doc1;
    Automerge doc2;
    doc1.set_actor(ActorId(std::string("01234567")));
    doc2.set_actor(ActorId(std::string("89abcdef")));
    ASSERT_TRUE(doc1.get_actor() < doc2.get_actor());

    auto list_id = doc1.put_object(ExId(), Prop("list"), ObjType::List);

    doc1.insert(list_id, 0, ScalarValue{ ScalarValue::Str, std::string("one") });
    doc1.insert(list_id, 1, ScalarValue{ ScalarValue::Str, std::string("three") });
    doc1.commit();
    doc2.merge(doc1);

    doc1.insert(list_id, 1, ScalarValue{ ScalarValue::Str, std::string("two") });
    doc1.commit();
    doc2.insert(list_id, 2, ScalarValue{ ScalarValue::Str, std::string("four") });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ(json::parse(R"({
    "list": ["one", "two", "three", "four"]
})"), json(doc1));
}

TEST_F(AutomergeTest, ConcurrentInsertionsAtSameListPositions) {
    Automerge doc1;
    Automerge doc2;
    doc1.set_actor(ActorId(std::string("01234567")));
    doc2.set_actor(ActorId(std::string("89abcdef")));
    ASSERT_TRUE(doc1.get_actor() < doc2.get_actor());

    auto list_id = doc1.put_object(ExId(), Prop("birds"), ObjType::List);

    doc1.insert(list_id, 0, ScalarValue{ ScalarValue::Str, std::string("parakeet") });
    doc1.commit();
    doc2.merge(doc1);

    doc1.insert(list_id, 1, ScalarValue{ ScalarValue::Str, std::string("starling") });
    doc1.commit();
    doc2.insert(list_id, 1, ScalarValue{ ScalarValue::Str, std::string("chaffinch") });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ(json::parse(R"({
    "birds": ["parakeet", "chaffinch", "starling"]
})"), json(doc1));
}

TEST_F(AutomergeTest, ConcurrentAssignmentAndDeletionOfMapEntry) {
    Automerge doc1;
    Automerge doc2;

    doc1.put(ExId(), Prop("bestBird"), ScalarValue{ ScalarValue::Str, std::string("robin") });
    doc1.commit();
    doc2.merge(doc1);

    doc1.delete_(ExId(), Prop("bestBird"));
    doc1.commit();
    doc2.put(ExId(), Prop("bestBird"), ScalarValue{ ScalarValue::Str, std::string("magpie") });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ(json::parse(R"({
    "bestBird": "magpie"
})"), json(doc1));
}

TEST_F(AutomergeTest, ConcurrentAssignmentAndDeletionOfListEntry) {
    Automerge doc1;
    Automerge doc2;

    auto list_id = doc1.put_object(ExId(), Prop("birds"), ObjType::List);

    doc1.insert(list_id, 0, ScalarValue{ ScalarValue::Str, std::string("blackbird") });
    doc1.insert(list_id, 1, ScalarValue{ ScalarValue::Str, std::string("thrush") });
    doc1.insert(list_id, 2, ScalarValue{ ScalarValue::Str, std::string("goldfinch") });
    doc1.commit();
    doc2.merge(doc1);

    doc1.put(list_id, Prop(1), ScalarValue{ ScalarValue::Str, std::string("starling") });
    doc1.commit();
    doc2.delete_(list_id, Prop(1));
    doc2.commit();

    EXPECT_EQ(json::parse(R"({
    "birds": ["blackbird", "goldfinch"]
})"), json(doc2));

    EXPECT_EQ(json::parse(R"({
    "birds": ["blackbird", "starling", "goldfinch"]
})"), json(doc1));

    doc1.merge(doc2);

    EXPECT_EQ(json::parse(R"({
    "birds": ["blackbird", "starling", "goldfinch"]
})"), json(doc1));
}

TEST_F(AutomergeTest, InsertionAfterDeletedListElement) {
    Automerge doc1;
    Automerge doc2;

    auto list_id = doc1.put_object(ExId(), Prop("birds"), ObjType::List);

    doc1.insert(list_id, 0, ScalarValue{ ScalarValue::Str, std::string("blackbird") });
    doc1.insert(list_id, 1, ScalarValue{ ScalarValue::Str, std::string("thrush") });
    doc1.insert(list_id, 2, ScalarValue{ ScalarValue::Str, std::string("goldfinch") });
    doc1.commit();
    doc2.merge(doc1);

    doc1.delete_(list_id, Prop(1));
    doc1.delete_(list_id, Prop(1));
    doc1.commit();
    doc2.insert(list_id, 2, ScalarValue{ ScalarValue::Str, std::string("starling") });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ(json::parse(R"({
    "birds": ["blackbird", "starling"]
})"), json(doc1));

    doc2.merge(doc1);

    EXPECT_EQ(json::parse(R"({
    "birds": ["blackbird", "starling"]
})"), json(doc2));
}

TEST_F(AutomergeTest, ConcurrentDeletionOfSameListElement) {
    Automerge doc1;
    Automerge doc2;

    auto list_id = doc1.put_object(ExId(), Prop("birds"), ObjType::List);

    doc1.insert(list_id, 0, ScalarValue{ ScalarValue::Str, std::string("albatross") });
    doc1.insert(list_id, 1, ScalarValue{ ScalarValue::Str, std::string("buzzard") });
    doc1.insert(list_id, 2, ScalarValue{ ScalarValue::Str, std::string("cormorant") });
    doc1.commit();
    doc2.merge(doc1);

    doc1.delete_(list_id, Prop(1));
    doc1.commit();
    doc2.delete_(list_id, Prop(1));
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ(json::parse(R"({
    "birds": ["albatross", "cormorant"]
})"), json(doc1));

    doc2.merge(doc1);

    EXPECT_EQ(json::parse(R"({
    "birds": ["albatross", "cormorant"]
})"), json(doc2));
}

TEST_F(AutomergeTest, ConcurrentUpdatesAtDifferentLevels) {
    Automerge doc1;
    Automerge doc2;

    auto animals = doc1.put_object(ExId(), Prop("animals"), ObjType::Map);
    auto birds = doc1.put_object(animals, Prop("birds"), ObjType::Map);
    doc1.put(birds, Prop("pink"), ScalarValue{ ScalarValue::Str, std::string("flamingo") });
    doc1.put(birds, Prop("black"), ScalarValue{ ScalarValue::Str, std::string("starling") });
    doc1.commit();

    auto mammals = doc1.put_object(animals, Prop("mammals"), ObjType::List);
    doc1.insert(mammals, 0, ScalarValue{ ScalarValue::Str, std::string("badger") });
    doc1.commit();

    doc2.merge(doc1);

    doc1.put(birds, Prop("brown"), ScalarValue{ ScalarValue::Str, std::string("sparrow") });
    doc1.commit();

    doc2.delete_(animals, Prop("birds"));
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ(json::parse(R"({
    "animals": {
        "mammals": ["badger"]
    }
})"), json(doc1));

    EXPECT_EQ(json::parse(R"({
    "animals": {
        "mammals": ["badger"]
    }
})"), json(doc2));
}

TEST_F(AutomergeTest, ConcurrentUpdatesOfConcurrentlyDeletedObjects) {
    Automerge doc1;
    Automerge doc2;

    auto birds = doc1.put_object(ExId(), Prop("birds"), ObjType::Map);
    auto blackbird = doc1.put_object(birds, Prop("blackbird"), ObjType::Map);
    doc1.put(blackbird, Prop("feathers"), ScalarValue{ ScalarValue::Str, std::string("black") });
    doc1.commit();

    doc2.merge(doc1);

    doc1.delete_(birds, Prop("blackbird"));
    doc1.commit();

    doc2.put(blackbird, Prop("beak"), ScalarValue{ ScalarValue::Str, std::string("orange") });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ(json::parse(R"({
    "birds": {
    }
})"), json(doc1));
}

TEST_F(AutomergeTest, DoesNotInterleaveSequenceInsertionsAtSamePosition) {
    Automerge doc1;
    Automerge doc2;
    doc1.set_actor(ActorId(std::string("01234567")));
    doc2.set_actor(ActorId(std::string("89abcdef")));

    auto wisdom = doc1.put_object(ExId(), Prop("wisdom"), ObjType::List);
    doc1.commit();
    doc2.merge(doc1);

    doc1.insert(wisdom, 0, ScalarValue{ ScalarValue::Str, std::string("to") });
    doc1.insert(wisdom, 1, ScalarValue{ ScalarValue::Str, std::string("be") });
    doc1.insert(wisdom, 2, ScalarValue{ ScalarValue::Str, std::string("is") });
    doc1.insert(wisdom, 3, ScalarValue{ ScalarValue::Str, std::string("to") });
    doc1.insert(wisdom, 4, ScalarValue{ ScalarValue::Str, std::string("do") });
    doc1.commit();


    doc2.insert(wisdom, 0, ScalarValue{ ScalarValue::Str, std::string("to") });
    doc2.insert(wisdom, 1, ScalarValue{ ScalarValue::Str, std::string("do") });
    doc2.insert(wisdom, 2, ScalarValue{ ScalarValue::Str, std::string("is") });
    doc2.insert(wisdom, 3, ScalarValue{ ScalarValue::Str, std::string("to") });
    doc2.insert(wisdom, 4, ScalarValue{ ScalarValue::Str, std::string("be") });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_EQ(json::parse(R"({
    "wisdom": ["to", "do", "is", "to", "be", "to", "be", "is", "to", "do"]
})"), json(doc1));
}

TEST_F(AutomergeTest, MultipleInsertionsAtSameListPositionWithInsertionByGreaterActorId) {
    Automerge doc1;
    Automerge doc2;
    doc1.set_actor(ActorId(std::string("01234567")));
    doc2.set_actor(ActorId(std::string("89abcdef")));
    ASSERT_TRUE(doc1.get_actor() < doc2.get_actor());

    auto list = doc1.put_object(ExId(), Prop("list"), ObjType::List);
    doc1.insert(list, 0, ScalarValue{ ScalarValue::Str, std::string("two") });
    doc1.commit();
    doc2.merge(doc1);

    doc2.insert(list, 0, ScalarValue{ ScalarValue::Str, std::string("one") });
    doc2.commit();

    EXPECT_EQ(json::parse(R"({
    "list": ["one", "two"]
})"), json(doc2));
}

TEST_F(AutomergeTest, MultipleInsertionsAtSameListPositionWithInsertionByLesserActorId) {
    Automerge doc1;
    Automerge doc2;
    doc2.set_actor(ActorId(std::string("01234567")));
    doc1.set_actor(ActorId(std::string("89abcdef")));
    ASSERT_TRUE(doc2.get_actor() < doc1.get_actor());

    auto list = doc1.put_object(ExId(), Prop("list"), ObjType::List);
    doc1.insert(list, 0, ScalarValue{ ScalarValue::Str, std::string("two") });
    doc1.commit();
    doc2.merge(doc1);

    doc2.insert(list, 0, ScalarValue{ ScalarValue::Str, std::string("one") });
    doc2.commit();

    EXPECT_EQ(json::parse(R"({
    "list": ["one", "two"]
})"), json(doc2));
}

TEST_F(AutomergeTest, InsertionsConsistentWithCausality) {
    Automerge doc1;
    Automerge doc2;

    auto list = doc1.put_object(ExId(), Prop("list"), ObjType::List);
    doc1.insert(list, 0, ScalarValue{ ScalarValue::Str, std::string("four") });
    doc1.commit();
    doc2.merge(doc1);

    doc2.insert(list, 0, ScalarValue{ ScalarValue::Str, std::string("three") });
    doc2.commit();
    doc1.merge(doc2);

    doc1.insert(list, 0, ScalarValue{ ScalarValue::Str, std::string("two") });
    doc1.commit();
    doc2.merge(doc1);

    doc2.insert(list, 0, ScalarValue{ ScalarValue::Str, std::string("one") });
    doc2.commit();

    EXPECT_EQ(json::parse(R"({
    "list": ["one", "two", "three", "four"]
})"), json(doc2));
}

TEST_F(AutomergeTest, SaveAndRestoreEmpty) {
    Automerge doc;
    auto loaded = Automerge::load(make_bin_slice(doc.save()));

    EXPECT_EQ(json::parse(R"({
})"), json(loaded));
}

TEST_F(AutomergeTest, SaveRestoreComplex) {
    Automerge doc1;
    Automerge doc2;

    auto todos = doc1.put_object(ExId(), Prop("todos"), ObjType::List);
    auto first_todo = doc1.insert_object(todos, 0, ObjType::Map);
    doc1.put(first_todo, Prop("title"), ScalarValue{ ScalarValue::Str, std::string("water plants") });
    doc1.put(first_todo, Prop("done"), ScalarValue{ ScalarValue::Boolean, false });
    doc1.commit();
    doc2.merge(doc1);

    doc2.put(first_todo, Prop("title"), ScalarValue{ ScalarValue::Str, std::string("weed plants") });
    doc2.commit();
    doc1.put(first_todo, Prop("title"), ScalarValue{ ScalarValue::Str, std::string("kill plants") });
    doc1.commit();
    doc1.merge(doc2);

    auto reloaded = Automerge::load(make_bin_slice(doc1.save()));

    EXPECT_EQ((std::unordered_multiset<std::string>{ "weed plants", "kill plants" }),
        get_all_to_set(reloaded, first_todo, Prop("title")));

    json jr = reloaded;
    EXPECT_TRUE(json::parse(R"({
    "todos": [
        {
            "title": "weed plants",
            "done": false
        }]
})") == jr
|| json::parse(R"({
    "todos": [
        {
            "title": "kill plants",
            "done": false
        }]
})") == jr);
}

TEST_F(AutomergeTest, HandleRepeatedOutOfOrderChanges) {
    Automerge doc1;

    auto list = doc1.put_object(ExId(), Prop("list"), ObjType::List);
    doc1.insert(list, 0, ScalarValue{ ScalarValue::Str, std::string("a") });
    doc1.commit();

    auto doc2 = doc1.fork();

    doc1.insert(list, 1, ScalarValue{ ScalarValue::Str, std::string("b") });
    doc1.commit();
    doc1.insert(list, 2, ScalarValue{ ScalarValue::Str, std::string("c") });
    doc1.commit();
    doc1.insert(list, 3, ScalarValue{ ScalarValue::Str, std::string("d") });
    doc1.commit();

    auto changes = vector_of_pointer_to_vector(doc1.get_changes({}));

    doc2.apply_changes(std::vector<Change>(changes.cbegin() + 2, changes.cend()));
    doc2.apply_changes(std::vector<Change>(changes.cbegin() + 2, changes.cend()));
    doc2.apply_changes(std::move(changes));

    EXPECT_EQ(doc1.save(), doc2.save());
}

TEST_F(AutomergeTest, SaveRestoreComplexTransactional) {
    Automerge doc1;
    Automerge doc2;

    ExId first_todo = doc1.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [](Transaction& d)->std::vector<ExId> {
            auto todos = d.put_object(ExId(), Prop("todos"), ObjType::List);
            auto first_todo = d.insert_object(todos, 0, ObjType::Map);
            d.put(first_todo, Prop("title"), ScalarValue{ ScalarValue::Str, std::string("water plants") });
            d.put(first_todo, Prop("done"), ScalarValue{ ScalarValue::Boolean, false });

            return { first_todo };
        }
    ).first[0];
    doc2.merge(doc1);

    doc2.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [&](Transaction& tx)->std::vector<ExId> {
            tx.put(first_todo, Prop("title"), ScalarValue{ ScalarValue::Str, std::string("weed plants") });

            return {};
        }
    );

    doc1.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [&](Transaction& tx)->std::vector<ExId> {
            tx.put(first_todo, Prop("title"), ScalarValue{ ScalarValue::Str, std::string("kill plants") });

            return {};
        }
    );

    doc1.merge(doc2);

    auto reloaded = Automerge::load(make_bin_slice(doc1.save()));

    EXPECT_EQ((std::unordered_multiset<std::string>{ "weed plants", "kill plants" }),
        get_all_to_set(reloaded, first_todo, Prop("title")));

    json jr = reloaded;
    EXPECT_TRUE(json::parse(R"({
    "todos": [
        {
            "title": "weed plants",
            "done": false
        }]
})") == jr
|| json::parse(R"({
    "todos": [
        {
            "title": "kill plants",
            "done": false
        }]
})") == jr);
}

TEST_F(AutomergeTest, ListCounterDel) {
    Automerge doc1;
    doc1.set_actor(ActorId(std::string("01234567")));

    auto list = doc1.put_object(ExId(), Prop("list"), ObjType::List);
    doc1.insert(list, 0, ScalarValue{ ScalarValue::Str, std::string("a") });
    doc1.insert(list, 1, ScalarValue{ ScalarValue::Str, std::string("b") });
    doc1.insert(list, 2, ScalarValue{ ScalarValue::Str, std::string("c") });
    doc1.commit();

    auto doc2 = Automerge::load(make_bin_slice(doc1.save()));
    doc2.set_actor(ActorId(std::string("456789ab")));

    auto doc3 = Automerge::load(make_bin_slice(doc1.save()));
    doc3.set_actor(ActorId(std::string("89abcdef")));

    doc1.put(list, Prop(1), ScalarValue{ ScalarValue::Counter, Counter(0) });
    doc1.commit();
    doc2.put(list, Prop(1), ScalarValue{ ScalarValue::Counter, Counter(10) });
    doc2.commit();
    doc3.put(list, Prop(1), ScalarValue{ ScalarValue::Counter, Counter(100) });
    doc3.commit();

    doc1.put(list, Prop(2), ScalarValue{ ScalarValue::Counter, Counter(0) });
    doc1.commit();
    doc2.put(list, Prop(2), ScalarValue{ ScalarValue::Counter, Counter(10) });
    doc2.commit();
    doc3.put(list, Prop(2), ScalarValue{ ScalarValue::Int, (s64)100 });
    doc3.commit();

    doc1.increment(list, 1, 1);
    doc1.increment(list, 2, 1);
    doc1.commit();

    doc1.merge(doc2);
    doc1.merge(doc3);

    EXPECT_EQ((std::unordered_multiset<std::string>{ "1", "10", "100" }),
        get_all_to_set(doc1, list, Prop(1)));

    EXPECT_EQ((std::unordered_multiset<std::string>{ "1", "10", "100" }),
        get_all_to_set(doc1, list, Prop(2)));

    doc1.increment(list, 1, 1);
    doc1.increment(list, 2, 1);
    doc1.commit();

    EXPECT_EQ((std::unordered_multiset<std::string>{ "2", "11", "101" }),
        get_all_to_set(doc1, list, Prop(1)));

    EXPECT_EQ((std::unordered_multiset<std::string>{ "2", "11" }),
        get_all_to_set(doc1, list, Prop(2)));

    doc1.delete_(list, 2);
    doc1.commit();
    EXPECT_EQ(2, doc1.length(list));

    auto doc4 = Automerge::load(make_bin_slice(doc1.save()));
    EXPECT_EQ(2, doc4.length(list));

    doc1.delete_(list, 1);
    doc1.commit();
    EXPECT_EQ(1, doc1.length(list));

    auto doc5 = Automerge::load(make_bin_slice(doc1.save()));
    EXPECT_EQ(1, doc5.length(list));
}

TEST_F(AutomergeTest, ObserveCounterChangeApplication) {
    Automerge doc;

    doc.put(ExId(), Prop("counter"), ScalarValue{ ScalarValue::Counter, Counter(1) });
    doc.commit();

    doc.increment(ExId(), Prop("counter"), 2);
    doc.commit();
    doc.increment(ExId(), Prop("counter"), 5);
    doc.commit();

    auto changes = vector_of_pointer_to_vector(doc.get_changes({}));

    // TODO: observer
    Automerge doc_ob;
    doc_ob.apply_changes(std::move(changes));

    EXPECT_EQ(json::parse(R"({
    "counter": 8
})"), json(doc_ob));
}

TEST_F(AutomergeTest, IncrementNonCounterMap) {
    Automerge doc;
    EXPECT_THROW(doc.increment(ExId(), Prop("nothing"), 2), AutomergeError);

    doc.put(ExId(), Prop("non-counter"), ScalarValue{ ScalarValue::Str, std::string("mystring") });
    doc.commit();
    EXPECT_THROW(doc.increment(ExId(), Prop("non-counter"), 2), AutomergeError);

    doc.put(ExId(), Prop("counter"), ScalarValue{ ScalarValue::Counter, Counter(1) });
    doc.commit();
    EXPECT_NO_THROW(doc.increment(ExId(), Prop("counter"), 2));

    Automerge doc1;
    doc1.set_actor(ActorId(std::vector<u8>{1}));
    Automerge doc2;
    doc2.set_actor(ActorId(std::vector<u8>{2}));
    ASSERT_TRUE(doc1.get_actor() < doc2.get_actor());

    doc1.put(ExId(), Prop("key"), ScalarValue{ ScalarValue::Counter, Counter(1) });
    doc1.commit();
    doc2.put(ExId(), Prop("key"), ScalarValue{ ScalarValue::Str, std::string("mystring") });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_NO_THROW(doc1.increment(ExId(), Prop("key"), 2));
    doc1.commit();

    EXPECT_EQ(json::parse(R"({
    "key": 3
})"), json(doc1));
}

TEST_F(AutomergeTest, IncrementNonCounterList) {
    Automerge doc;
    auto list = doc.put_object(ExId(), Prop("list"), ObjType::List);
    doc.commit();

    doc.insert(list, 0, ScalarValue{ ScalarValue::Str, std::string("mystring") });
    doc.commit();
    EXPECT_THROW(doc.increment(list, Prop(0), 2), AutomergeError);

    doc.insert(list, 0, ScalarValue{ ScalarValue::Counter, Counter(1) });
    doc.commit();
    EXPECT_NO_THROW(doc.increment(list, Prop(0), 2));

    Automerge doc1;
    doc1.set_actor(ActorId(std::vector<u8>{1}));
    list = doc1.put_object(ExId(), Prop("list"), ObjType::List);
    doc1.commit();
    doc1.insert(list, 0, ScalarValue());
    doc1.commit();

    Automerge doc2 = doc1.fork();
    doc2.set_actor(ActorId(std::vector<u8>{2}));
    ASSERT_TRUE(doc1.get_actor() < doc2.get_actor());

    doc1.put(list, Prop(0), ScalarValue{ ScalarValue::Counter, Counter(1) });
    doc1.commit();
    doc2.put(list, Prop(0), ScalarValue{ ScalarValue::Str, std::string("mystring") });
    doc2.commit();

    doc1.merge(doc2);

    EXPECT_NO_THROW(doc1.increment(list, Prop(0), 2));
    doc1.commit();

    EXPECT_EQ(json::parse(R"({
    "list": [3]
})"), json(doc1));
}

TEST_F(AutomergeTest, TestLocalIncInMap) {
    Automerge doc1;
    doc1.set_actor(ActorId(std::string("01234567")));

    doc1.put(ExId(), Prop("hello"), ScalarValue{ ScalarValue::Str, std::string("world") });
    doc1.commit();

    auto doc2 = Automerge::load(make_bin_slice(doc1.save()));
    doc2.set_actor(ActorId(std::string("456789ab")));

    auto doc3 = Automerge::load(make_bin_slice(doc1.save()));
    doc3.set_actor(ActorId(std::string("89abcdef")));

    doc1.put(ExId(), Prop("cnt"), ScalarValue{ ScalarValue::Uint, (u64)20 });
    doc1.commit();

    doc2.put(ExId(), Prop("cnt"), ScalarValue{ ScalarValue::Counter, Counter(0) });
    doc2.commit();

    doc3.put(ExId(), Prop("cnt"), ScalarValue{ ScalarValue::Counter, Counter(10) });
    doc3.commit();

    doc1.merge(doc2);
    doc1.merge(doc3);

    EXPECT_EQ((std::unordered_multiset<std::string>{ "20", "0", "10" }),
        get_all_to_set(doc1, ExId(), Prop("cnt")));

    EXPECT_EQ(json::parse(R"({
    "cnt": 20,
    "hello": "world"
})"), json(doc1));

    doc1.increment(ExId(), Prop("cnt"), 5);
    doc1.commit();

    EXPECT_EQ((std::unordered_multiset<std::string>{ "5", "15" }),
        get_all_to_set(doc1, ExId(), Prop("cnt")));

    EXPECT_EQ(json::parse(R"({
    "cnt": 5,
    "hello": "world"
})"), json(doc1));

    auto bin1 = doc1.save();
    auto doc4 = Automerge::load(make_bin_slice(bin1));
    EXPECT_EQ(bin1, doc4.save());
}

// TODO: test_merging_test_conflicts_then_saving_and_loading, text not implement

TEST_F(AutomergeTest, DeleteOnlyChange) {
    Automerge doc1;
    ExId list = doc1.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [](Transaction& d)->std::vector<ExId> {
            auto l = d.put_object(ExId(), Prop("list"), ObjType::List);
            d.insert(l, 0, ScalarValue{ ScalarValue::Str, std::string("a") });

            return { l };
        }
    ).first[0];

    auto doc2 = Automerge::load(make_bin_slice(doc1.save()));
    doc2.set_actor(ActorId(doc1.get_actor()));
    doc2.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [&](Transaction& d)->std::vector<ExId> {
            d.delete_(list, Prop(0));

            return {};
        }
    );

    auto doc3 = Automerge::load(make_bin_slice(doc2.save()));
    doc3.set_actor(ActorId(doc1.get_actor()));
    doc3.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [&](Transaction& d)->std::vector<ExId> {
            d.insert(list, 0, ScalarValue{ ScalarValue::Str, std::string("b") });

            return {};
        }
    );

    auto doc4 = Automerge::load(make_bin_slice(doc3.save()));
    doc4.set_actor(ActorId(doc1.get_actor()));

    auto changes = doc4.get_changes({});

    ASSERT_EQ(3, changes.size());
    EXPECT_EQ(4, changes[2]->start_op);
}

TEST_F(AutomergeTest, SaveAndReloadCreateObject) {
    Automerge doc1;
    ExId list = doc1.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [](Transaction& d)->std::vector<ExId> {
            return { d.put_object(ExId(), Prop("foo"), ObjType::List) };
        }
    ).first[0];

    auto doc2 = Automerge::load(make_bin_slice(doc1.save()));
    doc2.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [&](Transaction& d)->std::vector<ExId> {
            d.insert(list, 0, ScalarValue{ ScalarValue::Uint, (u64)1 });

            return {};
        }
    );

    EXPECT_EQ(json::parse(R"({
    "foo": [1]
})"), json(doc2));

    EXPECT_NO_THROW(Automerge::load(make_bin_slice(doc2.save())));
}

TEST_F(AutomergeTest, TestCompressedChanges) {
    Automerge doc;
    // DEFLATE_MIN_SIZE(Encoder.h) is 250, so this should trigger compression
    doc.put(ExId(), Prop("bytes"), ScalarValue{ ScalarValue::Bytes, std::vector<u8>(300, 10) });
    doc.commit();
    
    Change change = std::move(doc.get_last_local_change().value());
    auto& bytes = change.bytes;
    EXPECT_TRUE(bytes.uncompressed.size() > DEFLATE_MIN_SIZE);

    change.compress();
    EXPECT_TRUE(bytes.compressed.size() < DEFLATE_MIN_SIZE);

    auto reloaded = Change::from_bytes(std::move(bytes.compressed));
    EXPECT_EQ(bytes.uncompressed, reloaded->bytes.uncompressed);
}

// TODO: compress save not implement
//TEST_F(AutomergeTest, TestCompressedDocCols) {
//    Automerge doc;
//    auto list = doc.put_object(ExId(), Prop("list"), ObjType::List);
//    doc.commit();
//
//    std::vector<u64> expected;
//    expected.reserve(200);
//    for (u64 i = 0; i < 200; ++i) {
//        doc.insert(list, i, ScalarValue{ ScalarValue::Uint, i });
//        doc.commit();
//        expected.push_back(i);
//    }
//
//    auto& bytes = change.bytes;
//    EXPECT_TRUE(bytes.uncompressed.size() > DEFLATE_MIN_SIZE);
//
//    change.compress();
//    EXPECT_TRUE(bytes.compressed.size() < DEFLATE_MIN_SIZE);
//
//    auto reloaded = Change::from_bytes(std::move(bytes.compressed));
//    EXPECT_EQ(bytes.uncompressed, reloaded->bytes.uncompressed);
//}

// TODO: Expand Change not implement
TEST_F(AutomergeTest, TestChangeEncodingExpandedChangeRoundTrip) {
    std::vector<u8> change_bytes = {
        0x85, 0x6f, 0x4a, 0x83, // magic bytes
        0xb2, 0x98, 0x9e, 0xa9, // checksum
        1, 61, 0, 2, 0x12, 0x34, // chunkType: change, length, deps, actor '1234'
        1, 1, 252, 250, 220, 255, 5, // seq, startOp, time
        14, 73, 110, 105, 116, 105, 97, 108, 105, 122, 97, 116, 105, 111,
        110, // message: 'Initialization'
        0, 6, // actor list, column count
        0x15, 3, 0x34, 1, 0x42, 2, // keyStr, insert, action
        0x56, 2, 0x57, 1, 0x70, 2, // valLen, valRaw, predNum
        0x7f, 1, 0x78, // keyStr: 'x'
        1,    // insert: false
        0x7f, 1, // action: set
        0x7f, 19, // valLen: 1 byte of type uint
        1,  // valRaw: 1
        0x7f, 0, // predNum: 0
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, // 10 trailing bytes
    };

    auto change = Change::from_bytes(std::vector<u8>(change_bytes));
    EXPECT_EQ(change_bytes, change->bytes.uncompressed);
}

// TODO: text not implement
TEST_F(AutomergeTest, LoadDocWithDeletedObjects) {
    Automerge doc;
    
    doc.put_object(ExId(), Prop("list"), ObjType::List);
    doc.commit();

    //doc.put_object(ExId(), Prop("text"), ObjType::Text);
    
    doc.put_object(ExId(), Prop("map"), ObjType::Map);
    doc.commit();

    //doc.put_object(ExId(), Prop("table"), ObjType::Table);

    doc.delete_(ExId(), Prop("list"));
    doc.commit();
    doc.delete_(ExId(), Prop("map"));
    doc.commit();

    auto saved = doc.save();
    EXPECT_NO_THROW(Automerge::load(make_bin_slice(saved)));
}

TEST_F(AutomergeTest, InsertAfterManyDeletes) {
    Automerge doc;

    auto obj = doc.put_object(ExId(), Prop("map"), ObjType::Map);
    doc.commit();

    for (int i = 0; i < 100; ++i) {
        ASSERT_NO_THROW(doc.put(obj, Prop(std::to_string(i)), ScalarValue{ ScalarValue::Int, (s64)i }));
        doc.commit();
        ASSERT_NO_THROW(doc.delete_(obj, Prop(std::to_string(i))));
        doc.commit();
    }
}

TEST_F(AutomergeTest, SimpleBadSaveload) {
    Automerge doc;
    doc.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [](Transaction& d)->std::vector<ExId> {
            d.put(ExId(), Prop("count"), ScalarValue{ ScalarValue::Int, (s64)0 });
            return {};
        }
    );

    doc.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [](Transaction& d)->std::vector<ExId> { return {}; }
    );

    doc.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [](Transaction& d)->std::vector<ExId> {
            d.put(ExId(), Prop("count"), ScalarValue{ ScalarValue::Int, (s64)0 });
            return {};
        }
    );

    auto bytes = doc.save();
    EXPECT_NO_THROW(Automerge::load(make_bin_slice(bytes)));
}

// TODO: text not implement
TEST_F(AutomergeTest, OpsOnWrongObjects) {
    Automerge doc;

    auto list = doc.put_object(ExId(), Prop("list"), ObjType::List);
    doc.commit();

    doc.insert(list, 0, ScalarValue{ ScalarValue::Str, std::string("a") });
    doc.commit();
    doc.insert(list, 1, ScalarValue{ ScalarValue::Str, std::string("b") });
    doc.commit();

    EXPECT_THROW(doc.put(list, Prop("a"), ScalarValue{ ScalarValue::Str, std::string("AAA") }),
        std::runtime_error);

    auto map = doc.put_object(ExId(), Prop("map"), ObjType::Map);

    doc.put(map, Prop("a"), ScalarValue{ ScalarValue::Str, std::string("AAA") });
    doc.commit();
    doc.put(map, Prop("b"), ScalarValue{ ScalarValue::Str, std::string("BBB") });
    doc.commit();

    EXPECT_THROW(doc.insert(map, 0, ScalarValue{ ScalarValue::Str, std::string("b") }),
        std::runtime_error);
}

// TODO: storage should be updated
//TEST_F(AutomergeTest, FuzzCrashers) {
//    for (auto& p : fs::directory_iterator("fuzz-crashers")) {
//        std::ifstream file(p.path(), std::ios::binary);
//        EXPECT_TRUE(file.is_open()) << "file: " << p.path();
//
//        std::vector<u8> bytes(std::istreambuf_iterator<char>(file), {});
//        EXPECT_ANY_THROW(Automerge::load(make_bin_slice(bytes))) << "file: " << p.path();
//    }
//}

std::vector<u8> fixture(const std::string& name) {
    std::ifstream file("fixtures/" + name, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    std::vector<u8> bytes(std::istreambuf_iterator<char>(file), {});
    return bytes;
}

// TODO: storage should be updated
//TEST_F(AutomergeTest, OverlongLeb) {
//    // the value metadata says "2", but the LEB is only 1-byte long and there's an extra 0
//    auto bytes = fixture("counter_value_has_incorrect_meta.automerge");
//    EXPECT_FALSE(bytes.empty()) << "counter_value_has_incorrect_meta.automerge not found";
//    EXPECT_ANY_THROW(Automerge::load(make_bin_slice(bytes)));
//
//    // the LEB is overlong (using 2 bytes where one would have sufficed)
//    bytes = fixture("counter_value_is_overlong.automerge");
//    EXPECT_FALSE(bytes.empty()) << "counter_value_is_overlong.automerge not found";
//    EXPECT_ANY_THROW(Automerge::load(make_bin_slice(bytes)));
//    
//    // the LEB is correct
//    bytes = fixture("counter_value_is_ok.automerge");
//    EXPECT_FALSE(bytes.empty()) << "counter_value_is_ok.automerge not found";
//    EXPECT_NO_THROW(Automerge::load(make_bin_slice(bytes)));
//}

// TODO: storage should be updated
//TEST_F(AutomergeTest, load) {
//    auto check_fixture = [](const std::string& name) {
//        auto bytes = fixture(name);
//        EXPECT_FALSE(bytes.empty()) << name << " not found";
//        auto doc = Automerge::load(make_bin_slice(bytes));
//
//        ExId map_id = doc.get(ExId(), Prop("a"))->first;
//        EXPECT_EQ((Value{ Value::SCALAR, ScalarValue{ ScalarValue::Str, std::string("b") } }),
//            doc.get(map_id, Prop("a"))->second);
//    };
//
//    check_fixture("two_change_chunks.automerge");
//    check_fixture("two_change_chunks_compressed.automerge");
//    check_fixture("two_change_chunks_out_of_order.automerge");
//}

TEST_F(AutomergeTest, Negtive64) {
    Automerge doc;
    EXPECT_NO_THROW(doc.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [](Transaction& d)->std::vector<ExId> {
            d.put(ExId(), Prop("a"), ScalarValue{ ScalarValue::Int, (s64)-64 });
            return {};
        }
    ));
}

TEST_F(AutomergeTest, ObjId64bits) {
    auto check_fixture = [](const std::string& name) {
        auto bytes = fixture(name);
        EXPECT_FALSE(bytes.empty()) << name << " not found";
        auto doc = Automerge::load(make_bin_slice(bytes));

        ExId map_id = doc.get(ExId(), Prop("a"))->first;
        EXPECT_FALSE(ExId() == map_id);
    };

    check_fixture("64bit_obj_id_change.automerge");
    check_fixture("64bit_obj_id_doc.automerge");
}

TEST_F(AutomergeTest, badChangeOnOptreeNodeBoundary) {
    Automerge doc;
    EXPECT_NO_THROW(doc.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [](Transaction& d)->std::vector<ExId> {
            d.put(ExId(), Prop("a"), ScalarValue{ ScalarValue::Str, std::string("z")});
            d.put(ExId(), Prop("b"), ScalarValue{ ScalarValue::Int, (s64)0 });
            d.put(ExId(), Prop("c"), ScalarValue{ ScalarValue::Int, (s64)0 });
            return {};
        }
    ));

    u64 iterations = 15;
    for (u64 i = 0; i < iterations; ++i) {
        EXPECT_NO_THROW(doc.transact_with(
            [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
            [&](Transaction& d)->std::vector<ExId> {
                std::string s((usize)i, 'a');
                d.put(ExId(), Prop("a"), ScalarValue{ ScalarValue::Str, s });
                d.put(ExId(), Prop("b"), ScalarValue{ ScalarValue::Uint, i + 1 });
                d.put(ExId(), Prop("c"), ScalarValue{ ScalarValue::Uint, i + 1 });
                return {};
            }
        ));
    }

    auto doc2 = Automerge::load(make_bin_slice(doc.save()));
    EXPECT_NO_THROW(doc.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [&](Transaction& d)->std::vector<ExId> {
            u64 i = iterations + 2;
            std::string s((usize)i, 'a');
            d.put(ExId(), Prop("a"), ScalarValue{ ScalarValue::Str, s });
            d.put(ExId(), Prop("b"), ScalarValue{ ScalarValue::Uint, i });
            d.put(ExId(), Prop("c"), ScalarValue{ ScalarValue::Uint, i });
            return {};
        }
    ));

    auto change = doc.get_changes(doc2.get_heads());
    EXPECT_NO_THROW(doc2.apply_changes(vector_of_pointer_to_vector(change)));
    EXPECT_NO_THROW(Automerge::load(make_bin_slice(doc2.save())));
}

TEST_F(AutomergeTest, RegressionNthMiscount) {
    Automerge doc;
    EXPECT_NO_THROW(doc.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [](Transaction& d)->std::vector<ExId> {
            auto list_id = d.put_object(ExId(), Prop("listval"), ObjType::List);
            for (usize i = 0; i < 30; ++i) {
                d.insert(list_id, i, ScalarValue{ ScalarValue::Null, {} });
                auto map = d.put_object(list_id, Prop(i), ObjType::Map);
                d.put(map, Prop("test"), ScalarValue{ ScalarValue::Int, (s64)i });
            }
            return {};
        }
    ));

    for (usize i = 0; i < 30; ++i) {
        auto [list_id, obj_type1] = doc.get(ExId(), Prop("listval")).value();
        EXPECT_EQ((Value{ Value::OBJECT, ObjType::List }), obj_type1) << "on run " << i;

        auto [map_id, obj_type2] = doc.get(list_id, Prop(i)).value();
        EXPECT_EQ((Value{ Value::OBJECT, ObjType::Map }), obj_type2) << "on run " << i;

        auto obj_type3 = std::move(doc.get(map_id, Prop("test"))->second);
        EXPECT_EQ(
            (Value{ Value::SCALAR, ScalarValue{ ScalarValue::Int, (s64)i } }),
            obj_type3
        ) << "on run " << i;
    }
}

TEST_F(AutomergeTest, RegressionNthMiscountSmaller) {
    Automerge doc;
    EXPECT_NO_THROW(doc.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(); },
        [](Transaction& d)->std::vector<ExId> {
            auto list_id = d.put_object(ExId(), Prop("listval"), ObjType::List);
            for (usize i = 0; i < B * 4; ++i) {
                d.insert(list_id, i, ScalarValue{ ScalarValue::Null, {} });
                d.put(list_id, Prop(i), ScalarValue{ ScalarValue::Int, (s64)i });
            }
            return {};
        }
    ));

    for (usize i = 0; i < B * 4; ++i) {
        auto [list_id, obj_type1] = doc.get(ExId(), Prop("listval")).value();
        EXPECT_EQ((Value{ Value::OBJECT, ObjType::List }), obj_type1);

        auto obj_type2 = std::move(doc.get(list_id, Prop(i))->second);
        EXPECT_EQ(
            (Value{ Value::SCALAR, ScalarValue{ ScalarValue::Int, (s64)i } }),
            obj_type2
        ) << "on run " << i;
    }
}

TEST_F(AutomergeTest, RegressionInsertOpid) {
    Automerge doc;

    auto list_id = doc.put_object(ExId(), Prop("list"), ObjType::List);
    doc.commit();

    auto change1 = doc.get_last_local_change();

    for (usize i = 0; i < 30; ++i) {
        doc.insert(list_id, i, ScalarValue{ ScalarValue::Null, {} });
        doc.put(list_id, Prop(i), ScalarValue{ ScalarValue::Int, (s64)i });
    }
    doc.commit();

    auto change2 = doc.get_last_local_change();

    Automerge new_doc;
    new_doc.apply_changes_with(std::vector{ std::move(*change1) }, {});
    new_doc.apply_changes_with(std::vector{ std::move(*change2) }, {});

    for (usize i = 0; i < 30; ++i) {
        auto [_1, doc_val] = doc.get(list_id, Prop(i)).value();
        auto [_2, new_doc_val] = new_doc.get(list_id, Prop(i)).value();

        EXPECT_EQ(
            (Value{ Value::SCALAR, ScalarValue{ ScalarValue::Int, (s64)i } }),
            doc_val
        ) << "doc_val on run " << i;
        EXPECT_EQ(
            (Value{ Value::SCALAR, ScalarValue{ ScalarValue::Int, (s64)i } }),
            new_doc_val
        ) << "new_doc_val on run " << i;
    }

    // TODO: patch not implement
}

TEST_F(AutomergeTest, BigList) {
    Automerge doc;

    auto list_id = doc.put_object(ExId(), Prop("list"), ObjType::List);
    doc.commit();

    auto change1 = doc.get_last_local_change();

    for (usize i = 0; i < B; ++i) {
        doc.insert(list_id, i, ScalarValue{ ScalarValue::Null, {} });
    }
    for (usize i = 0; i < B; ++i) {
        doc.put_object(list_id, Prop(i), ObjType::Map);
    }
    doc.commit();

    auto change2 = doc.get_last_local_change();

    Automerge new_doc;
    new_doc.apply_changes_with(std::vector{ std::move(*change1) }, {});
    new_doc.apply_changes_with(std::vector{ std::move(*change2) }, {});

    for (usize i = 0; i < B; ++i) {
        auto [_1, doc_val] = doc.get(list_id, Prop(i)).value();
        auto [_2, new_doc_val] = new_doc.get(list_id, Prop(i)).value();

        EXPECT_EQ(
            (Value{ Value::OBJECT, ObjType::Map }),
            doc_val
        ) << "doc_val on run " << i;
        EXPECT_EQ(
            (Value{ Value::OBJECT, ObjType::Map }),
            new_doc_val
        ) << "new_doc_val on run " << i;
    }

    // TODO: patch not implement
}

// TODO: mark not implement

/////////////////////////////////////////////////////////
// automerge/src/sync.rs
/////////////////////////////////////////////////////////

class SyncTest : public AutomergeTest {
};

void sync(Automerge& a, Automerge& b, State& a_sync_state, State& b_sync_state) {
    usize MAX_ITER = 10;
    usize iterations = 0;

    while (true) {
        auto a_to_b = a.generate_sync_message(a_sync_state);
        auto b_to_a = b.generate_sync_message(b_sync_state);
        if (!a_to_b.has_value() && !b_to_a.has_value()) {
            break;
        }

        if (iterations > MAX_ITER) {
            throw std::runtime_error("failed to sync in 10 iterations");
        }

        if (a_to_b.has_value()) {
            b.receive_sync_message(b_sync_state, std::move(*a_to_b));
        }
        if (b_to_a.has_value()) {
            a.receive_sync_message(a_sync_state, std::move(*b_to_a));
        }

        ++iterations;
    }
}

TEST_F(SyncTest, EmptyMessageEncodeDecode) {
    SyncMessage msg;
    auto encoded = msg.encode();
    auto decoded = SyncMessage::decode({ encoded.cbegin(), encoded.size() });

    EXPECT_TRUE(decoded.has_value());
}

TEST_F(SyncTest, GenerateMessageTwiceDoesNothing) {
    Automerge doc;
    doc.json_add("/key"_json_pointer, "value");
    doc.commit();

    EXPECT_EQ(json::parse(R"({"key": "value"})"), json(doc));

    State sync_state;
    EXPECT_TRUE(doc.generate_sync_message(sync_state).has_value());
    EXPECT_FALSE(doc.generate_sync_message(sync_state).has_value());
}

TEST_F(SyncTest, ShouldNotReplyIfNoData) {
    Automerge doc1;
    Automerge doc2;
    State s1;
    State s2;

    auto m1 = doc1.generate_sync_message(s1);
    ASSERT_TRUE(m1.has_value());

    ASSERT_NO_THROW(doc2.receive_sync_message(s2, std::move(*m1)));

    auto m2 = doc2.generate_sync_message(s2);
    ASSERT_FALSE(m2.has_value());
}

TEST_F(SyncTest, ShouldAllowSimultaneousMessageDuringSynchronisation) {
    // create & synchronize two nodes
    Automerge doc1;
    doc1.set_actor(ActorId(std::string("abc123")));
    Automerge doc2;
    doc2.set_actor(ActorId(std::string("def456")));
    State s1;
    State s2;

    doc1.json_add("/x"_json_pointer, 0);
    doc1.commit();
    doc2.json_add("/y"_json_pointer, 0);
    doc2.commit();

    for (int i = 1; i < 5; ++i) {
        doc1.json_replace("/x"_json_pointer, i);
        doc1.commit();
        doc2.json_replace("/y"_json_pointer, i);
        doc2.commit();
    }

    ChangeHash head1 = doc1.get_heads()[0];
    ChangeHash head2 = doc2.get_heads()[0];

    // both sides report what they have but have no shared peer state
    auto msg1to2 = doc1.generate_sync_message(s1);
    ASSERT_TRUE(msg1to2.has_value());
    auto msg2to1 = doc2.generate_sync_message(s2);
    ASSERT_TRUE(msg2to1.has_value());

    EXPECT_EQ(0, msg1to2->changes.size());
    EXPECT_EQ(0, msg1to2->have[0].last_sync.size());
    EXPECT_EQ(0, msg2to1->changes.size());
    EXPECT_EQ(0, msg2to1->have[0].last_sync.size());

    // doc1 and doc2 receive that message and update sync state
    ASSERT_NO_THROW(doc1.receive_sync_message(s1, std::move(*msg2to1)));
    ASSERT_NO_THROW(doc2.receive_sync_message(s2, std::move(*msg1to2)));

    // now both reply with their local changes the other lacks
    // (standard warning that 1% of the time this will result in a "need" message)
    msg1to2 = doc1.generate_sync_message(s1);
    ASSERT_TRUE(msg1to2.has_value());
    EXPECT_EQ(5, msg1to2->changes.size());

    msg2to1 = doc2.generate_sync_message(s2);
    ASSERT_TRUE(msg2to1.has_value());
    EXPECT_EQ(5, msg2to1->changes.size());

    // both should now apply the changes
    ASSERT_NO_THROW(doc1.receive_sync_message(s1, std::move(*msg2to1)));
    EXPECT_EQ(0, doc1.get_missing_deps(std::vector<ChangeHash>()).size());

    ASSERT_NO_THROW(doc2.receive_sync_message(s2, std::move(*msg1to2)));
    EXPECT_EQ(0, doc2.get_missing_deps(std::vector<ChangeHash>()).size());

    // The response acknowledges the changes received and sends no further changes
    msg1to2 = doc1.generate_sync_message(s1);
    ASSERT_TRUE(msg1to2.has_value());
    EXPECT_EQ(0, msg1to2->changes.size());

    msg2to1 = doc2.generate_sync_message(s2);
    ASSERT_TRUE(msg2to1.has_value());
    EXPECT_EQ(0, msg2to1->changes.size());

    // After receiving acknowledgements, their shared heads should be equal
    ASSERT_NO_THROW(doc1.receive_sync_message(s1, std::move(*msg2to1)));
    ASSERT_NO_THROW(doc2.receive_sync_message(s2, std::move(*msg1to2)));

    EXPECT_EQ(s1.shared_heads, s2.shared_heads);

    // We're in sync, no more messages required
    EXPECT_FALSE(doc1.generate_sync_message(s1).has_value());
    EXPECT_FALSE(doc2.generate_sync_message(s2).has_value());

    // If we make one more change and start another sync then its lastSync should be updated
    doc1.json_replace("/x"_json_pointer, 5);
    doc1.commit();

    msg1to2 = doc1.generate_sync_message(s1);
    ASSERT_TRUE(msg1to2.has_value());

    std::vector expected_heads { head1, head2 };
    std::sort(expected_heads.begin(), expected_heads.end());

    std::vector actual_heads = msg1to2->have[0].last_sync;
    std::sort(actual_heads.begin(), actual_heads.end());

    EXPECT_EQ(expected_heads, actual_heads);
}

TEST_F(SyncTest, ShouldHandleFalsePositiveHead) {
    // Scenario:                                                            ,-- n1
    // c0 <-- c1 <-- c2 <-- c3 <-- c4 <-- c5 <-- c6 <-- c7 <-- c8 <-- c9 <-+
    //                                                                      `-- n2
    // where n2 is a false positive in the Bloom filter containing {n1}.
    // lastSync is c9.

    Automerge doc1;
    doc1.set_actor(ActorId(std::string("abc123")));
    Automerge doc2;
    doc2.set_actor(ActorId(std::string("def456")));
    State s1;
    State s2;

    for (int i = 0; i < 10; ++i) {
        ASSERT_NO_THROW(doc1.put(ExId(), Prop("x"), ScalarValue{ ScalarValue::Int, (s64)i }));
        doc1.commit();
    }

    ASSERT_NO_THROW(sync(doc1, doc2, s1, s2));

    // search for false positive; see comment above
    s32 i = 0;
    while (true) {
        Automerge doc1copy = doc1;
        doc1copy.set_actor(ActorId(std::string("01234567")));
        auto val1 = std::to_string(i) + " @ n1";
        ASSERT_NO_THROW(doc1copy.put(ExId(), Prop("x"), ScalarValue{ ScalarValue::Str, std::move(val1) }));
        doc1copy.commit();

        // should copy from doc2?
        Automerge doc2copy = doc1;
        doc2copy.set_actor(ActorId(std::string("89abcdef")));
        auto val2 = std::to_string(i) + " @ n2";
        ASSERT_NO_THROW(doc2copy.put(ExId(), Prop("x"), ScalarValue{ ScalarValue::Str, std::move(val2) }));
        doc2copy.commit();

        auto heads = doc1copy.get_heads();
        auto n1_bloom = BloomFilter(vector_to_vector_of_pointer(heads));
        if (n1_bloom.contains_hash(doc2copy.get_heads()[0])) {
            doc1 = doc1copy;
            doc2 = doc2copy;

            break;
        }

        ++i;
    }

    auto all_heads = doc1.get_heads();
    vector_extend(all_heads, doc2.get_heads());
    std::sort(all_heads.begin(), all_heads.end());

    // reset sync states
    auto ns1 = State::decode(make_bin_slice(s1.encode()));
    auto ns2 = State::decode(make_bin_slice(s2.encode()));

    ASSERT_NO_THROW(sync(doc1, doc2, *ns1, *ns2));
    EXPECT_EQ(all_heads, doc1.get_heads());
    EXPECT_EQ(all_heads, doc2.get_heads());
}

TEST_F(SyncTest, ShouldHandleChainsOfFalsePositive) {
    // Scenario:                         ,-- c5
    // c0 <-- c1 <-- c2 <-- c3 <-- c4 <-+
    //                                   `-- n2c1 <-- n2c2 <-- n2c3
    // where n2c1 and n2c2 are both false positives in the Bloom filter containing {c5}.
    // lastSync is c4.

    Automerge doc1;
    doc1.set_actor(ActorId(std::string("abc123")));
    Automerge doc2;
    doc2.set_actor(ActorId(std::string("def456")));
    State s1;
    State s2;

    for (int i = 0; i < 10; ++i) {
        ASSERT_NO_THROW(doc1.put(ExId(), Prop("x"), ScalarValue{ ScalarValue::Int, (s64)i }));
        doc1.commit();
    }

    ASSERT_NO_THROW(sync(doc1, doc2, s1, s2));

    ASSERT_NO_THROW(doc1.put(ExId(), Prop("x"), ScalarValue{ ScalarValue::Int, (s64)5 }));
    doc1.commit();

    auto heads = doc1.get_heads();
    auto bloom = BloomFilter(vector_to_vector_of_pointer(heads));

    // search for false positive; see comment above
    s32 i = 0;
    while (true) {
        Automerge doc = doc2;
        doc2.set_actor(ActorId(std::string("89abcdef")));

        ASSERT_NO_THROW(doc.put(ExId(), Prop("x"), 
            ScalarValue{ ScalarValue::Str, std::to_string(i) + " at 89abdef" }));
        doc.commit();

        if (bloom.contains_hash(doc.get_heads()[0])) {
            doc2 = doc;
            break;
        }

        ++i;
    }

    // find another false positive building on the first
    i = 0;
    while (true) {
        Automerge doc = doc2;
        doc2.set_actor(ActorId(std::string("89abcdef")));

        ASSERT_NO_THROW(doc.put(ExId(), Prop("x"),
            ScalarValue{ ScalarValue::Str, std::to_string(i) + " again" }));
        doc.commit();

        if (bloom.contains_hash(doc.get_heads()[0])) {
            doc2 = doc;
            break;
        }

        ++i;
    }

    ASSERT_NO_THROW(doc2.put(ExId(), Prop("x"), ScalarValue{ ScalarValue::Str, std::string("final @ 89abdef") }));

    auto all_heads = doc1.get_heads();
    vector_extend(all_heads, doc2.get_heads());
    std::sort(all_heads.begin(), all_heads.end());

    auto ns1 = State::decode(make_bin_slice(s1.encode()));
    auto ns2 = State::decode(make_bin_slice(s2.encode()));    
    ASSERT_NO_THROW(sync(doc1, doc2, *ns1, *ns2));
    EXPECT_EQ(all_heads, doc1.get_heads());
    EXPECT_EQ(all_heads, doc2.get_heads());
}

TEST_F(SyncTest, ShouldHandleLotsOfBranchingAndMerging) {
    Automerge doc1;
    doc1.set_actor(ActorId(std::string("01234567")));
    Automerge doc2;
    doc2.set_actor(ActorId(std::string("89abcdef")));
    Automerge doc3;
    doc3.set_actor(ActorId(std::string("fedcba98")));
    State s1;
    State s2;

    ASSERT_NO_THROW(doc1.put(ExId(), Prop("x"), ScalarValue{ ScalarValue::Int, (s64)0 }));
    doc1.commit();
    auto change = doc1.get_last_local_change();
    ASSERT_TRUE(change.has_value());

    ASSERT_NO_THROW(doc2.apply_changes(std::vector{ *change }));
    ASSERT_NO_THROW(doc3.apply_changes(std::vector{ std::move(*change) }));

    ASSERT_NO_THROW(doc3.put(ExId(), Prop("x"), ScalarValue{ ScalarValue::Int, (s64)1 }));
    doc3.commit();

    //        - n1c1 <------ n1c2 <------ n1c3 <-- etc. <-- n1c20 <------ n1c21
    //       /          \/           \/                              \/
    //      /           /\           /\                              /\
    // c0 <---- n2c1 <------ n2c2 <------ n2c3 <-- etc. <-- n2c20 <------ n2c21
    //      \                                                          /
    //       ---------------------------------------------- n3c1 <-----
    for (int i = 1; i < 20; ++i) {
        ASSERT_NO_THROW(doc1.put(ExId(), Prop("n1"), ScalarValue{ ScalarValue::Int, (s64)i }));
        doc1.commit();
        ASSERT_NO_THROW(doc2.put(ExId(), Prop("n2"), ScalarValue{ ScalarValue::Int, (s64)i }));
        doc2.commit();
        
        auto change1 = doc1.get_last_local_change();
        ASSERT_TRUE(change1.has_value());
        auto change2 = doc2.get_last_local_change();
        ASSERT_TRUE(change2.has_value());

        ASSERT_NO_THROW(doc1.apply_changes(std::vector{ *change2 }));
        ASSERT_NO_THROW(doc2.apply_changes(std::vector{ std::move(*change1) }));
    }

    ASSERT_NO_THROW(sync(doc1, doc2, s1, s2));

    // Having n3's last change concurrent to the last sync heads forces us into the slower code path
    auto change3 = doc3.get_last_local_change();
    ASSERT_TRUE(change3.has_value());
    ASSERT_NO_THROW(doc2.apply_changes(std::vector{ std::move(*change3) }));

    ASSERT_NO_THROW(doc1.put(ExId(), Prop("n1"), ScalarValue{ ScalarValue::Str, std::string("final") }));
    doc1.commit();
    ASSERT_NO_THROW(doc2.put(ExId(), Prop("n1"), ScalarValue{ ScalarValue::Str, std::string("final") }));
    doc2.commit();

    ASSERT_NO_THROW(sync(doc1, doc2, s1, s2));

    EXPECT_EQ(doc1.get_heads(), doc2.get_heads());
}

struct DocWithSync {
    Automerge doc;
    State peer_state;
};

Automerge repeated_increment(u64 n) {
    Automerge doc;
    doc.put(ExId(), Prop("counter"), ScalarValue{ ScalarValue::Counter, Counter() });
    for (u64 i = 0; i < n; ++i) {
        doc.increment(ExId(), Prop("counter"), 1);
    }
    doc.commit();

    return doc;
}

Automerge repeated_put(u64 n) {
    Automerge doc;
    for (u64 i = 0; i < n; ++i) {
        doc.put(ExId(), Prop("0"), ScalarValue{ ScalarValue::Uint, i });
    }
    doc.commit();

    return doc;
}

Automerge increasing_put(u64 n) {
    Automerge doc;
    for (u64 i = 0; i < n; ++i) {
        doc.put(ExId(), Prop(std::to_string(i)), ScalarValue{ ScalarValue::Uint, i });
    }
    doc.commit();

    return doc;
}

void sync(DocWithSync& doc1, DocWithSync& doc2) {
    while (true) {
        auto a_to_b = doc1.doc.generate_sync_message(doc1.peer_state);
        if (!a_to_b.has_value()) {
            break;
        }
        doc2.doc.receive_sync_message(doc2.peer_state, std::move(*a_to_b));

        auto b_to_a = doc2.doc.generate_sync_message(doc2.peer_state);
        if (b_to_a.has_value()) {
            doc1.doc.receive_sync_message(doc1.peer_state, std::move(*b_to_a));
        }
    }
}

TEST(BenchmarkTest, BenchmarkSync) {
    DocWithSync doc1;
    DocWithSync doc2;

    for (u64 i = 0; i < 100; ++i) {
        doc1.doc.put(ExId(), Prop(std::to_string(i)), ScalarValue{ ScalarValue::Int, (s64)i });
        doc1.doc.commit();
        sync(doc1, doc2);
    }
}

TEST(BenchmarkTest, BenchmarkLoadPut) {
    auto bytes = repeated_put(10000).save();
    auto str = hex_to_string(make_bin_slice(bytes));
    Automerge::load(make_bin_slice(bytes));
}
