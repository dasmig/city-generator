#include <iostream>
#include "../dasmig/citygen.hpp"

int main()
{
    // Singleton auto-probes for lite first, falls back to full.
    auto& gen = dasmig::cg::instance();

    // --- Or load a specific tier explicitly: ---
    // dasmig::cg gen;
    // gen.load(dasmig::dataset::lite);  // ~25k major cities
    // gen.load(dasmig::dataset::full);  // ~200k cities

    // Generate random cities (population-weighted).
    std::cout << "=== Random cities ===" << '\n';
    for (int i = 0; i < 10; ++i)
    {
        auto c = gen.get_city();
        std::cout << c.name << ", " << c.country_code
                  << " (pop. " << c.population << ")" << '\n';
    }

    // Generate cities from a specific country.
    std::cout << "\n=== Brazilian cities ===" << '\n';
    for (int i = 0; i < 5; ++i)
    {
        auto c = gen.get_city("BR");
        std::cout << c.name << ", " << c.admin1_code
                  << " (pop. " << c.population
                  << ", tz: " << c.timezone << ")" << '\n';
    }

    // Access all available fields.
    std::cout << "\n=== Full city details ===" << '\n';
    auto c = gen.get_city();
    std::cout << "Name:         " << c.name << '\n';
    std::cout << "ASCII name:   " << c.asciiname << '\n';
    std::cout << "Country:      " << c.country_code << '\n';
    std::cout << "Coordinates:  " << c.latitude << ", " << c.longitude
              << '\n';
    std::cout << "Population:   " << c.population << '\n';
    std::cout << "Elevation:    " << c.elevation << " m" << '\n';
    std::cout << "DEM:          " << c.dem << " m" << '\n';
    std::cout << "Timezone:     " << c.timezone << '\n';
    std::cout << "Feature code: " << c.feature_code << '\n';
    std::cout << "GeoNames ID:  " << c.geonameid << '\n';

    // Deterministic generation — same seed always produces the same city.
    std::cout << "\n=== Seeded (deterministic) ===" << '\n';
    auto seeded = gen.get_city(42);
    std::cout << "seed 42 -> " << seeded.name << ", " << seeded.country_code
              << '\n';

    // Replay: retrieve the seed from a city and replay it.
    auto replay = gen.get_city(seeded.seed());
    std::cout << "replay  -> " << replay.name << ", " << replay.country_code
              << '\n';

    // Seed the engine for a deterministic sequence.
    gen.seed(100);
    std::cout << "\n=== Seeded sequence ===" << '\n';
    for (int i = 0; i < 3; ++i)
    {
        std::cout << gen.get_city() << '\n';
    }
    gen.unseed(); // restore non-deterministic state

    // JSON with the replay signature (seed and data hash).
    std::cout << "\n=== JSON ===" << '\n';
    std::cout << gen.get_city("JP", std::uint64_t{7}).to_json() << '\n';

    // Extend the data with a layer: a new city in its own country.
    std::cout << "\n=== Extension layer ===" << '\n';
    const auto& layer = gen.extend_from_string(
        "geonameid\tname\tlatitude\tlongitude\tcountry_code\tpopulation\ttimezone\n"
        "99000001\tPoseidonis\t31.0\t-24.0\tQQ\t250000\tAtlantic/Azores\n",
        "atlantis");
    std::cout << layer.source << ": " << layer.added << " added\n";
    std::cout << "From QQ: " << gen.get_city("QQ") << '\n';

    // Library info.
    std::cout << "\n=== Info ===" << '\n';
    std::cout << "Cities loaded: " << gen.city_count() << '\n';
}
