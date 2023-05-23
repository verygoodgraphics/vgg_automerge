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

Automerge increasing_put(u64 n) {
    Automerge doc;
    for (u64 i = 0; i < n; ++i) {
        doc.put(ExId(), Prop(std::to_string(i)), ScalarValue{ ScalarValue::Int, (s64)i });
    }
    doc.commit();

    return doc;
}

void sync(DocWithSync& doc1, DocWithSync& doc2) {
    usize MAX_ITER = 0;
    usize iterations = 0;

    while (true) {
        if (iterations > MAX_ITER) {
            break;
        }

        auto a_to_b = doc1.doc.generate_sync_message(doc1.peer_state);
        if (!a_to_b.has_value()) {
            break;
        }
        doc2.doc.receive_sync_message(doc2.peer_state, std::move(*a_to_b));

        auto b_to_a = doc2.doc.generate_sync_message(doc2.peer_state);
        if (b_to_a.has_value()) {
            doc1.doc.receive_sync_message(doc1.peer_state, std::move(*b_to_a));
        }

        ++iterations;
    }
}

static void benchmark_sync(benchmark::State& state) {
    //std::vector<u64> sizes = { 100, 1000, 10000 };

    for (auto _ : state) {
        DocWithSync doc1;
        DocWithSync doc2;

        for (u64 i = 0; i < 100; ++i) {
            doc1.doc.put(ExId(), Prop(std::to_string(i)), ScalarValue{ ScalarValue::Int, (s64)i });            
            doc1.doc.commit();
            sync(doc1, doc2);
        }
    }
}
BENCHMARK(benchmark_sync);

BENCHMARK_MAIN();
