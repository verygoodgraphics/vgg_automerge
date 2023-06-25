#include <benchmark/benchmark.h>

#include "Automerge.h"

static void BM_StringCreation(benchmark::State& state) {
  for (auto _ : state)
    std::string empty_string;
}
// Register the function as a benchmark
BENCHMARK(BM_StringCreation);

// Define another benchmark
static void BM_StringCopy(benchmark::State& state) {
  std::string x = "hello";
  for (auto _ : state)
    std::string copy(x);
}
BENCHMARK(BM_StringCopy);

struct DocWithSync {
    Automerge doc;
    State peer_state;
};

static Automerge increasing_put(u64 n) {
    Automerge doc;
    for (u64 i = 0; i < n; ++i) {
        doc.put(ExId(), Prop(std::to_string(i)), ScalarValue{ ScalarValue::Int, (s64)i });
    }
    doc.commit();

    return doc;
}

static void sync(DocWithSync& doc1, DocWithSync& doc2) {
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

static void sync_unidirectional(benchmark::State& state) {
    auto _doc1 = increasing_put(state.range(0));

    for (auto _ : state) {
        DocWithSync doc1 = { _doc1, State() };
        DocWithSync doc2;

        sync(doc1, doc2);
    }
}
//BENCHMARK(sync_unidirectional)->Arg(100)->Arg(1000)->Arg(10000);

static void sync_unidirectional_every_change(benchmark::State& state) {
    for (auto _ : state) {
        DocWithSync doc1;
        DocWithSync doc2;

        u64 size = state.range(0);
        for (u64 i = 0; i < size; ++i) {
            doc1.doc.put(ExId(), Prop(std::to_string(i)), ScalarValue{ ScalarValue::Uint, i });            
            doc1.doc.commit();
            sync(doc1, doc2);
        }
    }
}
//BENCHMARK(sync_unidirectional_every_change)->Arg(100)->Arg(1000)->Arg(10000);

BENCHMARK_MAIN();
