#include <gtest/gtest.h>

#include "Automerge.h"

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

        ASSERT_NO_THROW(doc1.apply_changes(std::vector{ std::move(*change2) }));
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
