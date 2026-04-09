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
    std::cout << "=== Random cities ===" << std::endl;
    for (int i = 0; i < 10; ++i)
    {
        auto c = gen.get_city();
        std::cout << c.name << ", " << c.country_code
                  << " (pop. " << c.population << ")" << std::endl;
    }

    // Generate cities from a specific country.
    std::cout << "\n=== Brazilian cities ===" << std::endl;
    for (int i = 0; i < 5; ++i)
    {
        auto c = gen.get_city("BR");
        std::cout << c.name << ", " << c.admin1_code
                  << " (pop. " << c.population
                  << ", tz: " << c.timezone << ")" << std::endl;
    }

    // Access all available fields.
    std::cout << "\n=== Full city details ===" << std::endl;
    auto c = gen.get_city();
    std::cout << "Name:         " << c.name << std::endl;
    std::cout << "ASCII name:   " << c.asciiname << std::endl;
    std::cout << "Country:      " << c.country_code << std::endl;
    std::cout << "Coordinates:  " << c.latitude << ", " << c.longitude
              << std::endl;
    std::cout << "Population:   " << c.population << std::endl;
    std::cout << "Elevation:    " << c.elevation << " m" << std::endl;
    std::cout << "DEM:          " << c.dem << " m" << std::endl;
    std::cout << "Timezone:     " << c.timezone << std::endl;
    std::cout << "Feature code: " << c.feature_code << std::endl;
    std::cout << "GeoNames ID:  " << c.geonameid << std::endl;

    // Deterministic generation — same seed always produces the same city.
    std::cout << "\n=== Seeded (deterministic) ===" << std::endl;
    auto seeded = gen.get_city(42);
    std::cout << "seed 42 -> " << seeded.name << ", " << seeded.country_code
              << std::endl;

    // Replay: retrieve the seed from a city and replay it.
    auto replay = gen.get_city(seeded.seed());
    std::cout << "replay  -> " << replay.name << ", " << replay.country_code
              << std::endl;

    // Seed the engine for a deterministic sequence.
    gen.seed(100);
    std::cout << "\n=== Seeded sequence ===" << std::endl;
    for (int i = 0; i < 3; ++i)
    {
        std::cout << gen.get_city() << std::endl;
    }
    gen.unseed(); // restore non-deterministic state

    // Library info.
    std::cout << "\n=== Info ===" << std::endl;
    std::cout << "Cities loaded: " << gen.city_count() << std::endl;
}
