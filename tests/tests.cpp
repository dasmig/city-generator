#include "catch_amalgamated.hpp"
#include "../dasmig/citygen.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static dasmig::cg make_loaded_generator()
{
    dasmig::cg gen;
    // Try tier-based loading first, fall back to direct paths.
    if (!gen.load(dasmig::dataset::lite) &&
        !gen.load(dasmig::dataset::full))
    {
        for (const auto& candidate :
             {"resources/full/cities.tsv", "../resources/full/cities.tsv",
              "city-generator/resources/full/cities.tsv"})
        {
            if (std::filesystem::exists(candidate))
            {
                gen.load(candidate);
                break;
            }
        }
    }
    REQUIRE(gen.has_data());
    return gen;
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

TEST_CASE("load - populates city data from TSV", "[loading]")
{
    auto gen = make_loaded_generator();
    // Lite has ~25k cities, full has ~200k.
    REQUIRE(gen.city_count() > 10'000);
}

TEST_CASE("load - non-existent path is a no-op", "[loading]")
{
    dasmig::cg gen;
    gen.load("does_not_exist.tsv");
    REQUIRE_FALSE(gen.has_data());
    REQUIRE(gen.city_count() == 0);
}

TEST_CASE("load - directory path is a no-op", "[loading]")
{
    dasmig::cg gen;
    gen.load("resources");
    REQUIRE_FALSE(gen.has_data());
}

TEST_CASE("has_data - false before load", "[loading]")
{
    dasmig::cg gen;
    REQUIRE_FALSE(gen.has_data());
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

TEST_CASE("get_city - returns a valid city", "[generation]")
{
    auto gen = make_loaded_generator();
    auto c = gen.get_city();
    REQUIRE_FALSE(c.name.empty());
    REQUIRE(c.geonameid != 0);
    REQUIRE_FALSE(c.country_code.empty());
    REQUIRE_FALSE(c.timezone.empty());
}

TEST_CASE("get_city - throws when no data loaded", "[generation]")
{
    dasmig::cg gen;
    REQUIRE_THROWS_AS(gen.get_city(), std::runtime_error);
}

TEST_CASE("get_city - implicit string conversion returns name", "[generation]")
{
    auto gen = make_loaded_generator();
    auto c = gen.get_city();
    std::string s = c;
    REQUIRE(s == c.name);
}

TEST_CASE("get_city - stream operator outputs name", "[generation]")
{
    auto gen = make_loaded_generator();
    auto c = gen.get_city();
    std::ostringstream oss;
    oss << c;
    REQUIRE(oss.str() == c.name);
}

// ---------------------------------------------------------------------------
// Country filtering
// ---------------------------------------------------------------------------

TEST_CASE("get_city(country) - returns city from specified country",
          "[filtering]")
{
    auto gen = make_loaded_generator();
    for (int i = 0; i < 20; ++i)
    {
        auto c = gen.get_city("US");
        REQUIRE(c.country_code == "US");
    }
}

TEST_CASE("get_city(country) - throws for unknown country", "[filtering]")
{
    auto gen = make_loaded_generator();
    REQUIRE_THROWS_AS(gen.get_city("ZZ"), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Seeding and determinism
// ---------------------------------------------------------------------------

TEST_CASE("get_city(seed) - same seed produces same city", "[seeding]")
{
    auto gen = make_loaded_generator();
    auto c1 = gen.get_city(42);
    auto c2 = gen.get_city(42);
    REQUIRE(c1.geonameid == c2.geonameid);
    REQUIRE(c1.name == c2.name);
}

TEST_CASE("city::seed() - can replay a generation", "[seeding]")
{
    auto gen = make_loaded_generator();
    auto c1 = gen.get_city();
    auto c2 = gen.get_city(c1.seed());
    REQUIRE(c1.geonameid == c2.geonameid);
}

TEST_CASE("seed() / unseed() - produces deterministic sequence", "[seeding]")
{
    auto gen = make_loaded_generator();

    gen.seed(100);
    auto a1 = gen.get_city();
    auto a2 = gen.get_city();

    gen.seed(100);
    auto b1 = gen.get_city();
    auto b2 = gen.get_city();

    REQUIRE(a1.geonameid == b1.geonameid);
    REQUIRE(a2.geonameid == b2.geonameid);

    gen.unseed();
}

TEST_CASE("get_city(country, seed) - deterministic with country filter",
          "[seeding]")
{
    auto gen = make_loaded_generator();
    auto c1 = gen.get_city("JP", 99);
    auto c2 = gen.get_city("JP", 99);
    REQUIRE(c1.geonameid == c2.geonameid);
    REQUIRE(c1.country_code == "JP");
}

TEST_CASE("multiple loads accumulate data", "[loading]")
{
    dasmig::cg gen;
    // Use tier-based loading, fall back to direct path.
    if (!gen.load(dasmig::dataset::lite) &&
        !gen.load(dasmig::dataset::full))
    {
        for (const auto& candidate :
             {"resources/full/cities.tsv", "../resources/full/cities.tsv",
              "city-generator/resources/full/cities.tsv"})
        {
            if (std::filesystem::exists(candidate))
            {
                gen.load(candidate);
                break;
            }
        }
    }
    if (gen.has_data())
    {
        auto count1 = gen.city_count();
        gen.load(dasmig::dataset::lite);
        REQUIRE(gen.city_count() >= count1);
    }
}

// ---------------------------------------------------------------------------
// Malformed input handling
// ---------------------------------------------------------------------------

TEST_CASE("load - skips malformed lines", "[loading]")
{
    // Create a temp file with a header plus one good and one bad line.
    auto tmp = std::filesystem::temp_directory_path() / "citygen_test.tsv";
    {
        std::ofstream f{tmp};
        f << "geonameid\tname\tasciiname\tlatitude\tlongitude\tfeature_code\t"
             "country_code\tcc2\tadmin1_code\tadmin2_code\tadmin3_code\t"
             "admin4_code\tpopulation\televation\tdem\ttimezone\n";
        f << "123\tTestCity\tTestCity\t10.0\t20.0\tPPL\tXX\t\t01\t\t\t\t"
             "5000\t100\t110\tEurope/London\n";
        f << "bad_line_too_few_fields\n";
        f << "0\tBadId\tBadId\t0\t0\tPPL\tYY\t\t\t\t\t\t0\t0\t0\tUTC\n";
    }

    dasmig::cg gen;
    gen.load(tmp);
    // The good line (geonameid=123) loads; the bad line is skipped;
    // the zero-id line is also skipped.
    REQUIRE(gen.city_count() == 1);

    auto c = gen.get_city(42);
    REQUIRE(c.name == "TestCity");
    REQUIRE(c.country_code == "XX");
    REQUIRE(c.population == 5000);
    REQUIRE(c.elevation == 100);
    REQUIRE(c.dem == 110);

    std::filesystem::remove(tmp);
}

TEST_CASE("load - handles empty file (header only)", "[loading]")
{
    auto tmp = std::filesystem::temp_directory_path() / "citygen_empty.tsv";
    {
        std::ofstream f{tmp};
        f << "geonameid\tname\tasciiname\n";
    }

    dasmig::cg gen;
    gen.load(tmp);
    REQUIRE_FALSE(gen.has_data());

    std::filesystem::remove(tmp);
}

TEST_CASE("load - handles zero-byte file", "[loading]")
{
    auto tmp = std::filesystem::temp_directory_path() / "citygen_zero.tsv";
    {
        std::ofstream f{tmp}; // creates empty file
    }

    dasmig::cg gen;
    gen.load(tmp);
    REQUIRE_FALSE(gen.has_data());

    std::filesystem::remove(tmp);
}

TEST_CASE("load - handles CRLF line endings", "[loading]")
{
    auto tmp = std::filesystem::temp_directory_path() / "citygen_crlf.tsv";
    {
        std::ofstream f{tmp, std::ios::binary};
        f << "geonameid\tname\tasciiname\tlatitude\tlongitude\tfeature_code\t"
             "country_code\tcc2\tadmin1_code\tadmin2_code\tadmin3_code\t"
             "admin4_code\tpopulation\televation\tdem\ttimezone\r\n";
        f << "456\tCRLFCity\tCRLFCity\t30.0\t40.0\tPPLA\tDE\t\t\t\t\t\t"
             "10000\t200\t210\tEurope/Berlin\r\n";
    }

    dasmig::cg gen;
    gen.load(tmp);
    REQUIRE(gen.city_count() == 1);

    auto c = gen.get_city(42);
    REQUIRE(c.name == "CRLFCity");
    REQUIRE(c.timezone == "Europe/Berlin");

    std::filesystem::remove(tmp);
}

TEST_CASE("load - skips empty lines in body", "[loading]")
{
    auto tmp = std::filesystem::temp_directory_path() / "citygen_blanks.tsv";
    {
        std::ofstream f{tmp};
        f << "geonameid\tname\tasciiname\tlatitude\tlongitude\tfeature_code\t"
             "country_code\tcc2\tadmin1_code\tadmin2_code\tadmin3_code\t"
             "admin4_code\tpopulation\televation\tdem\ttimezone\n";
        f << "789\tBlankTest\tBlankTest\t1.0\t2.0\tPPL\tXX\t\t\t\t\t\t"
             "100\t50\t55\tUTC\n";
        f << "\n";
        f << "\n";
        f << "790\tBlankTest2\tBlankTest2\t3.0\t4.0\tPPL\tXX\t\t\t\t\t\t"
             "200\t60\t65\tUTC\n";
    }

    dasmig::cg gen;
    gen.load(tmp);
    REQUIRE(gen.city_count() == 2);

    std::filesystem::remove(tmp);
}

TEST_CASE("load - parse exception marks line as invalid", "[loading]")
{
    auto tmp = std::filesystem::temp_directory_path() / "citygen_bad_num.tsv";
    {
        std::ofstream f{tmp};
        f << "geonameid\tname\tasciiname\tlatitude\tlongitude\tfeature_code\t"
             "country_code\tcc2\tadmin1_code\tadmin2_code\tadmin3_code\t"
             "admin4_code\tpopulation\televation\tdem\ttimezone\n";
        // geonameid is not a number.
        f << "not_a_number\tBadCity\tBadCity\t10.0\t20.0\tPPL\tXX\t\t\t"
             "\t\t\t5000\t100\t110\tUTC\n";
    }

    dasmig::cg gen;
    gen.load(tmp);
    REQUIRE(gen.city_count() == 0);

    std::filesystem::remove(tmp);
}

// ---------------------------------------------------------------------------
// Const overload throws
// ---------------------------------------------------------------------------

TEST_CASE("get_city(seed) - throws when no data loaded", "[generation]")
{
    const dasmig::cg gen;
    REQUIRE_THROWS_AS(gen.get_city(42), std::runtime_error);
}

TEST_CASE("get_city(country) - throws when no data loaded", "[filtering]")
{
    dasmig::cg gen;
    REQUIRE_THROWS_AS(gen.get_city("US"), std::runtime_error);
}

TEST_CASE("get_city(country, seed) - throws when no data loaded",
          "[filtering]")
{
    const dasmig::cg gen;
    REQUIRE_THROWS_AS(gen.get_city("US", 42), std::runtime_error);
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

TEST_CASE("instance() returns same reference", "[singleton]")
{
    auto& a = dasmig::cg::instance();
    auto& b = dasmig::cg::instance();
    REQUIRE(&a == &b);
}

// ---------------------------------------------------------------------------
// Data integrity — all fields
// ---------------------------------------------------------------------------

TEST_CASE("city fields - all fields have reasonable values", "[data]")
{
    auto gen = make_loaded_generator();
    gen.seed(42);

    // Sample several cities to cover variety.
    for (int i = 0; i < 50; ++i)
    {
        auto c = gen.get_city();

        REQUIRE(c.geonameid != 0);
        REQUIRE_FALSE(c.name.empty());
        REQUIRE_FALSE(c.asciiname.empty());
        REQUIRE(c.latitude >= -90.0);
        REQUIRE(c.latitude <= 90.0);
        REQUIRE(c.longitude >= -180.0);
        REQUIRE(c.longitude <= 180.0);
        REQUIRE_FALSE(c.country_code.empty());
        REQUIRE(c.country_code.size() == 2);
        REQUIRE_FALSE(c.timezone.empty());
        REQUIRE_FALSE(c.feature_code.empty());
        // Seed round-trip works.
        REQUIRE(c.seed() != 0);
    }

    gen.unseed();
}

// ---------------------------------------------------------------------------
// Population weighting — stronger assertion
// ---------------------------------------------------------------------------

TEST_CASE("population weighting - majority of draws are populated cities",
          "[weighting]")
{
    auto gen = make_loaded_generator();
    gen.seed(12345);

    int large_count = 0;
    static constexpr int draws = 200;

    for (int i = 0; i < draws; ++i)
    {
        auto c = gen.get_city();
        if (c.population > 100'000)
        {
            ++large_count;
        }
    }

    // With population weighting, the vast majority of draws should be
    // cities >100k. With uniform selection over 231k cities (~4k have >100k),
    // we'd expect ~3 hits. With weighting we expect well over half.
    REQUIRE(large_count > draws / 2);
    gen.unseed();
}

// ---------------------------------------------------------------------------
// Dataset tier loading
// ---------------------------------------------------------------------------

TEST_CASE("load(dataset::lite) loads fewer cities than full", "[loading][tier]")
{
    dasmig::cg gen_lite;
    dasmig::cg gen_full;

    bool lite_ok = gen_lite.load(dasmig::dataset::lite);
    bool full_ok = gen_full.load(dasmig::dataset::full);

    if (lite_ok && full_ok)
    {
        REQUIRE(gen_lite.city_count() > 0);
        REQUIRE(gen_full.city_count() > 0);
        REQUIRE(gen_full.city_count() > gen_lite.city_count());
    }
}

TEST_CASE("load(dataset) returns false for missing tier", "[loading][tier]")
{
    dasmig::cg gen;
    // Loading from a nonexistent working directory should fail gracefully.
    auto old_cwd = std::filesystem::current_path();
    auto tmp = std::filesystem::temp_directory_path() / "citygen_empty_dir";
    std::filesystem::create_directories(tmp);
    std::filesystem::current_path(tmp);

    REQUIRE_FALSE(gen.load(dasmig::dataset::lite));
    REQUIRE_FALSE(gen.load(dasmig::dataset::full));
    REQUIRE_FALSE(gen.has_data());

    std::filesystem::current_path(old_cwd);
    std::filesystem::remove_all(tmp);
}

// ---------------------------------------------------------------------------
// Country index cache — consistency
// ---------------------------------------------------------------------------

TEST_CASE("country filter uses cached index correctly", "[country][cache]")
{
    auto gen = make_loaded_generator();
    gen.seed(42);

    // Draw 50 cities filtered by "BR" using the cached path.
    for (int i = 0; i < 50; ++i)
    {
        auto c = gen.get_city("BR");
        REQUIRE(c.country_code == "BR");
    }

    gen.unseed();
}

TEST_CASE("country filter seeded - cached path deterministic",
          "[country][cache]")
{
    auto gen = make_loaded_generator();

    auto c1 = gen.get_city("DE", 7777);
    auto c2 = gen.get_city("DE", 7777);
    REQUIRE(c1.geonameid == c2.geonameid);
    REQUIRE(c1.country_code == "DE");
}
