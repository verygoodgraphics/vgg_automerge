// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "Automerge.h"

// Based on https://automerge.github.io/docs/quickstart
Automerge quickstart() {
    Automerge doc1;
    auto result = doc1.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(std::string("Add card"), {}, {}); },
        [](Transaction& tx)->std::vector<ExId> {
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
        [&](Transaction& tx)->std::vector<ExId> {
            tx.put(card1, Prop("done"), ScalarValue{ ScalarValue::Boolean, true });

            return {};
        }
    );

    doc2.transact_with(
        [](const std::vector<ExId>& result) { return CommitOptions<OpObserver>(std::string("Delete card"), {}, {}); },
        [&](Transaction& tx)->std::vector<ExId> {
            tx.delete_(cards, Prop(0));

            return {};
        }
    );

    doc1.merge(doc2);

    assert(json(doc1) == json::parse(R"({
    "cards": [
        {
            "title": "Rewrite everything in Clojure",
            "done": true
        }
    ]
})"));
    assert(json(doc2) == json::parse(R"({
    "cards": [
        {
            "title": "Rewrite everything in Clojure",
            "done": false
        }
    ]
})"));

    assert(json(doc3) == json::parse(R"({
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
})"));

    return doc3;
}

void direct_edit_test() {
    auto doc = Automerge();
    doc.put_object(doc.import_object("/").first, Prop("todo"),
        R"([{"title": "work A", "done": false}, {"title": "work B", "done": true}])");

    doc.commit();

    assert(json(doc) == json::parse(R"({
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
})"));
}

void json_demo() {
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

    assert(new_json_obj == json_obj);

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

    assert(json(automerge_doc) == json::parse(R"({
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
})"));
}

void json_test(const Automerge& doc) {
    auto doc1 = json(doc).get<Automerge>();

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

    assert(json(doc1) == json::parse(R"({
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
})"));

    // json_replace3, object to scalar
    doc1.json_replace("/cards/2/title/3/n2"_json_pointer, false);
    // json_replace4, object to object
    doc1.json_replace("/cards/2/title/1"_json_pointer, { { "test", -9} });
    doc1.commit();

    assert(json(doc1) == json::parse(R"({
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
})"));
}

void hex_string_test() {
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

    assert(json(doc) == json_obj);
    // the doc's ActorId is new randomly generated, however it's not used as no new operations committed.
    // so, the binary is the same
    assert(binary == bin_vec);
}

static Automerge repeated_put(u64 n) {
    Automerge doc;
    for (u64 i = 0; i < n; ++i) {
        doc.put(ExId(), Prop("0"), ScalarValue{ ScalarValue::Uint, i });
    }
    doc.commit();

    return doc;
}

static Automerge repeated_increment(u64 n) {
    Automerge doc;
    doc.put(ExId(), Prop("counter"), ScalarValue{ ScalarValue::Counter, Counter() });
    for (u64 i = 0; i < n; ++i) {
        doc.increment(ExId(), Prop("counter"), 1);
    }
    doc.commit();

    return doc;
}

static Automerge decreasing_put(u64 n) {
    Automerge doc;
    for (u64 i = n; i > 0; --i) {
        doc.put(ExId(), Prop(std::to_string(i)), ScalarValue{ ScalarValue::Uint, i });
    }
    doc.commit();

    return doc;
}

static void map_repeated_put() {
    for (int i = 0; i < 100; ++i) {
        repeated_put(10000);
    }
}

static void map_repeated_increment() {
    for (int i = 0; i < 100; ++i) {
        repeated_increment(10000);
    }
}

static void map_decreasing_put() {
    for (int i = 0; i < 1000; ++i) {
        decreasing_put(10000);
    }
}

static void map_save_decreasing_put() {
    auto doc = decreasing_put(100);

    for (int i = 0; i < 100000; ++i) {
        doc.save();
    }
}

static void map_load_repeated_put() {
    auto bytes = repeated_put(10000).save();
    for (int i = 0; i < 100; ++i) {
        Automerge::load(make_bin_slice(bytes));
    }
}

static void map_load_decreasing_put() {
    auto bytes = decreasing_put(10000).save();

    for (int i = 0; i < 100; ++i) {
        Automerge::load(make_bin_slice(bytes));
    }
}

void vector_string() {
    std::vector<std::string> vs;

    for (int i = 0; i < 10000; ++i) {
        vs.push_back(std::to_string(i));
    }
}

int main()
{
    std::cout << "demo\n";

    auto doc = quickstart();

    direct_edit_test();

    json_demo();

    json_test(doc);

    hex_string_test();

    std::cout << "passed" << std::endl;

    //map_repeated_put();

    //map_repeated_increment();

    //map_decreasing_put();

    //map_save_decreasing_put();

    //map_load_repeated_put();

    //map_load_decreasing_put();

    //vector_string();

    return 0;
}
