// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include <benchmark/benchmark.h>

#include "Automerge.h"

static Automerge repeated_increment(u64 n) {
    Automerge doc;
    doc.put(ExId(), Prop("counter"), ScalarValue{ ScalarValue::Counter, Counter() });
    for (u64 i = 0; i < n; ++i) {
        doc.increment(ExId(), Prop("counter"), 1);
    }
    doc.commit();

    return doc;
}

static Automerge repeated_put(u64 n) {
    Automerge doc;
    for (u64 i = 0; i < n; ++i) {
        doc.put(ExId(), Prop("0"), ScalarValue{ ScalarValue::Uint, i });
    }
    doc.commit();

    return doc;
}

static Automerge increasing_put(u64 n) {
    Automerge doc;
    for (u64 i = 0; i < n; ++i) {
        doc.put(ExId(), Prop(std::to_string(i)), ScalarValue{ ScalarValue::Uint, i });
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

static void map_repeated_put(benchmark::State& state) {
    for (auto _ : state) {
        repeated_put(state.range(0));
    }
}
BENCHMARK(map_repeated_put)->Arg(100)->Arg(1000)->Arg(10000);

static void map_repeated_increment(benchmark::State& state) {
    for (auto _ : state) {
        repeated_increment(state.range(0));
    }
}
BENCHMARK(map_repeated_increment)->Arg(100)->Arg(1000)->Arg(10000);

static void map_increasing_put(benchmark::State& state) {
    for (auto _ : state) {
        increasing_put(state.range(0));
    }
}
BENCHMARK(map_increasing_put)->Arg(100)->Arg(1000)->Arg(10000);

static void map_decreasing_put(benchmark::State& state) {
    for (auto _ : state) {
        decreasing_put(state.range(0));
    }
}
BENCHMARK(map_decreasing_put)->Arg(100)->Arg(1000)->Arg(10000);

static void map_save_repeated_put(benchmark::State& state) {
    auto doc = repeated_put(state.range(0));
    for (auto _ : state) {
        doc.save();
    }
}
BENCHMARK(map_save_repeated_put)->Arg(100)->Arg(1000)->Arg(10000);

static void map_save_repeated_increment(benchmark::State& state) {
    auto doc = repeated_increment(state.range(0));
    for (auto _ : state) {
        doc.save();
    }
}
BENCHMARK(map_save_repeated_increment)->Arg(100)->Arg(1000)->Arg(10000);

static void map_save_increasing_put(benchmark::State& state) {
    auto doc = increasing_put(state.range(0));
    for (auto _ : state) {
        doc.save();
    }
}
BENCHMARK(map_save_increasing_put)->Arg(100)->Arg(1000)->Arg(10000);

static void map_save_decreasing_put(benchmark::State& state) {
    auto doc = decreasing_put(state.range(0));
    for (auto _ : state) {
        doc.save();
    }
}
BENCHMARK(map_save_decreasing_put)->Arg(100)->Arg(1000)->Arg(10000);

static void map_load_repeated_put(benchmark::State& state) {
    auto bytes = repeated_put(state.range(0)).save();
    for (auto _ : state) {
        Automerge::load(make_bin_slice(bytes));
    }
}
BENCHMARK(map_load_repeated_put)->Arg(100)->Arg(1000)->Arg(10000);

static void map_load_repeated_increment(benchmark::State& state) {
    auto bytes = repeated_increment(state.range(0)).save();
    for (auto _ : state) {
        Automerge::load(make_bin_slice(bytes));
    }
}
// TODO: map_load_repeated_increment 10000
BENCHMARK(map_load_repeated_increment)->Arg(100)->Arg(1000);

static void map_load_increasing_put(benchmark::State& state) {
    auto bytes = increasing_put(state.range(0)).save();
    for (auto _ : state) {
        Automerge::load(make_bin_slice(bytes));
    }
}
BENCHMARK(map_load_increasing_put)->Arg(100)->Arg(1000)->Arg(10000);

static void map_load_decreasing_put(benchmark::State& state) {
    auto bytes = decreasing_put(state.range(0)).save();
    for (auto _ : state) {
        Automerge::load(make_bin_slice(bytes));
    }
}
BENCHMARK(map_load_decreasing_put)->Arg(100)->Arg(1000)->Arg(10000);

static void map_apply_repeated_put(benchmark::State& state) {
    auto changes = vector_of_pointer_to_vector(repeated_put(state.range(0)).get_changes({}));
    for (auto _ : state) {
        Automerge doc;
        doc.apply_changes(std::vector(changes));
    }
}
BENCHMARK(map_apply_repeated_put)->Arg(100)->Arg(1000)->Arg(10000);

static void map_apply_repeated_increment(benchmark::State& state) {
    auto changes = vector_of_pointer_to_vector(repeated_increment(state.range(0)).get_changes({}));
    for (auto _ : state) {
        Automerge doc;
        doc.apply_changes(std::vector(changes));
    }
}
BENCHMARK(map_apply_repeated_increment)->Arg(100)->Arg(1000)->Arg(10000);

static void map_apply_increasing_put(benchmark::State& state) {
    auto changes = vector_of_pointer_to_vector(increasing_put(state.range(0)).get_changes({}));
    for (auto _ : state) {
        Automerge doc;
        doc.apply_changes(std::vector(changes));
    }
}
BENCHMARK(map_apply_increasing_put)->Arg(100)->Arg(1000)->Arg(10000);

static void map_apply_decreasing_put(benchmark::State& state) {
    auto changes = vector_of_pointer_to_vector(decreasing_put(state.range(0)).get_changes({}));
    for (auto _ : state) {
        Automerge doc;
        doc.apply_changes(std::vector(changes));
    }
}
BENCHMARK(map_apply_decreasing_put)->Arg(100)->Arg(1000)->Arg(10000);
