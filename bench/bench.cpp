// Benchmarks for city-generator; see bench.hpp for the options.

#include "../dasmig/citygen.hpp"
#include "bench.hpp"

int main(int argc, char** argv)
{
    dasmig::bench::runner run{"city-generator", argc, argv};

    run.load("load", [&] {
        dasmig::cg gen;
        return gen.load(run.tier());
    });

    dasmig::cg gen;
    if (!gen.load(run.tier())) return run.missing_tier();

    run.calls("get_city", [&](std::uint64_t seed) { return gen.get_city({}, seed); });
    run.calls("get_city BR", [&](std::uint64_t seed) {
        return gen.get_city({.country = "BR"}, seed);
    });
    run.calls("get_city JP uniform", [&](std::uint64_t seed) {
        return gen.get_city({.country = "JP", .weighted = false}, seed);
    });
    run.calls("get_city to_json", [&](std::uint64_t seed) {
        return gen.get_city({}, seed).to_json();
    });

    run.batches("get_city_batch", [&](std::size_t count, std::size_t threads, std::uint64_t seed) {
        return gen.get_city_batch({}, {.count = count, .threads = threads}, seed);
    });

    return run.finish();
}
