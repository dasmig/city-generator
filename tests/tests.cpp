#include "catch_amalgamated.hpp"
#include "../dasmig/citygen.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
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
    auto c = gen.get_city({});
    REQUIRE_FALSE(c.name.empty());
    REQUIRE(c.geonameid != 0);
    REQUIRE_FALSE(c.country_code.empty());
    REQUIRE_FALSE(c.timezone.empty());
}

TEST_CASE("get_city - throws when no data loaded", "[generation]")
{
    dasmig::cg gen;
    REQUIRE_THROWS_AS(gen.get_city({}), std::runtime_error);
}

TEST_CASE("get_city - implicit string conversion returns name", "[generation]")
{
    auto gen = make_loaded_generator();
    auto c = gen.get_city({});
    std::string s = c;
    REQUIRE(s == c.name);
}

TEST_CASE("get_city - stream operator outputs name", "[generation]")
{
    auto gen = make_loaded_generator();
    auto c = gen.get_city({});
    std::ostringstream oss;
    oss << c;
    REQUIRE(oss.str() == c.name);
}

// ---------------------------------------------------------------------------
// Country filtering
// ---------------------------------------------------------------------------

TEST_CASE("get_city(request) - returns city from specified country",
          "[filtering]")
{
    auto gen = make_loaded_generator();
    for (int i = 0; i < 20; ++i)
    {
        auto c = gen.get_city({.country = "US"});
        REQUIRE(c.country_code == "US");
    }
}

TEST_CASE("get_city(request) - throws for unknown country", "[filtering]")
{
    auto gen = make_loaded_generator();
    REQUIRE_THROWS_AS(gen.get_city({.country = "ZZ"}), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Seeding and determinism
// ---------------------------------------------------------------------------

TEST_CASE("get_city(request, seed) - same seed produces same city", "[seeding]")
{
    auto gen = make_loaded_generator();
    auto c1 = gen.get_city({}, 42);
    auto c2 = gen.get_city({}, 42);
    REQUIRE(c1.geonameid == c2.geonameid);
    REQUIRE(c1.name == c2.name);
}

TEST_CASE("city::seed() - can replay a generation", "[seeding]")
{
    auto gen = make_loaded_generator();
    auto c1 = gen.get_city({});
    auto c2 = gen.get_city({}, c1.seed());
    REQUIRE(c1.geonameid == c2.geonameid);
}

TEST_CASE("a caller-owned rng gives a reproducible sequence", "[seeding]")
{
    auto gen = make_loaded_generator();
    dasmig::core::rng a{100};
    dasmig::core::rng b{100};
    for (int i = 0; i < 5; ++i)
    {
        REQUIRE(gen.get_city({}, a()).geonameid == gen.get_city({}, b()).geonameid);
    }
}

TEST_CASE("get_city(request, seed) - deterministic with country filter",
          "[seeding]")
{
    auto gen = make_loaded_generator();
    auto c1 = gen.get_city({.country = "JP"}, 99);
    auto c2 = gen.get_city({.country = "JP"}, 99);
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
        (void)gen.load(dasmig::dataset::lite);
        REQUIRE(gen.city_count() == count1); // loading again replaces
    }
}

// ---------------------------------------------------------------------------
// Malformed input handling
// ---------------------------------------------------------------------------

TEST_CASE("load - a malformed line is an error with its location", "[loading]")
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
    try
    {
        gen.load(tmp);
        FAIL("expected core::tsv_error");
    }
    catch (const dasmig::core::tsv_error& e)
    {
        REQUIRE(e.line() == 3); // the line with too few fields
    }
    REQUIRE_FALSE(gen.has_data()); // nothing was loaded

    // Without the bad lines the good one loads.
    {
        std::ofstream f{tmp};
        f << "geonameid\tname\tasciiname\tlatitude\tlongitude\tfeature_code\t"
             "country_code\tcc2\tadmin1_code\tadmin2_code\tadmin3_code\t"
             "admin4_code\tpopulation\televation\tdem\ttimezone\n";
        f << "123\tTestCity\tTestCity\t10.0\t20.0\tPPL\tXX\t\t01\t\t\t\t"
             "5000\t100\t110\tEurope/London\n";
    }
    gen.load(tmp);
    REQUIRE(gen.city_count() == 1);
    auto c = gen.get_city({}, 42);
    REQUIRE(c.name == "TestCity");
    REQUIRE(c.country_code == "XX");
    REQUIRE(c.population == 5000);
    REQUIRE(c.elevation == 100);
    REQUIRE(c.dem == 110);

    // A geonameid of 0 is not valid.
    {
        std::ofstream f{tmp, std::ios::app};
        f << "0\tBadId\tBadId\t0\t0\tPPL\tYY\t\t\t\t\t\t0\t0\t0\tUTC\n";
    }
    REQUIRE_THROWS_AS(gen.load(tmp), dasmig::core::tsv_error);
    REQUIRE(gen.city_count() == 1); // unchanged

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

TEST_CASE("load - a zero-byte file is an error", "[loading]")
{
    auto tmp = std::filesystem::temp_directory_path() / "citygen_zero.tsv";
    {
        std::ofstream f{tmp}; // creates empty file
    }

    dasmig::cg gen;
    REQUIRE_THROWS_AS(gen.load(tmp), dasmig::core::tsv_error); // no header line
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

    auto c = gen.get_city({}, 42);
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

TEST_CASE("load - a non-numeric id is an error", "[loading]")
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
    try
    {
        gen.load(tmp);
        FAIL("expected core::tsv_error");
    }
    catch (const dasmig::core::tsv_error& e)
    {
        REQUIRE(e.line() == 2);
        REQUIRE(e.column() == "geonameid");
    }
    REQUIRE(gen.city_count() == 0);

    std::filesystem::remove(tmp);
}

// ---------------------------------------------------------------------------
// Const overload throws
// ---------------------------------------------------------------------------

TEST_CASE("get_city(request, seed) - throws when no data loaded", "[generation]")
{
    const dasmig::cg gen;
    REQUIRE_THROWS_AS(gen.get_city({}, 42), std::runtime_error);
}

TEST_CASE("get_city(request) - country throws when no data loaded", "[filtering]")
{
    dasmig::cg gen;
    REQUIRE_THROWS_AS(gen.get_city({.country = "US"}), std::runtime_error);
}

TEST_CASE("get_city(request, seed) - country throws when no data loaded",
          "[filtering]")
{
    const dasmig::cg gen;
    REQUIRE_THROWS_AS(gen.get_city({.country = "US"}, 42), std::runtime_error);
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
    dasmig::core::rng rng{42};

    // Sample several cities to cover variety.
    for (int i = 0; i < 50; ++i)
    {
        auto c = gen.get_city({}, rng());

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
}

// ---------------------------------------------------------------------------
// Population weighting — stronger assertion
// ---------------------------------------------------------------------------

TEST_CASE("population weighting - majority of draws are populated cities",
          "[weighting]")
{
    auto gen = make_loaded_generator();
    dasmig::core::rng rng{12345};

    int large_count = 0;
    static constexpr int draws = 200;

    for (int i = 0; i < draws; ++i)
    {
        auto c = gen.get_city({}, rng());
        if (c.population > 100'000)
        {
            ++large_count;
        }
    }

    // With population weighting, the vast majority of draws should be
    // cities >100k. With uniform selection over 231k cities (~4k have >100k),
    // we'd expect ~3 hits. With weighting we expect well over half.
    REQUIRE(large_count > draws / 2);
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

#ifndef DASMIG_CITY_EMBED // compiled-in data is never missing
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
#endif

// ---------------------------------------------------------------------------
// Country index cache — consistency
// ---------------------------------------------------------------------------

TEST_CASE("country filter uses cached index correctly", "[country][cache]")
{
    auto gen = make_loaded_generator();
    dasmig::core::rng rng{42};

    // Draw 50 cities filtered by "BR" using the cached path.
    for (int i = 0; i < 50; ++i)
    {
        auto c = gen.get_city({.country = "BR"}, rng());
        REQUIRE(c.country_code == "BR");
    }
}

TEST_CASE("country filter seeded - cached path deterministic",
          "[country][cache]")
{
    auto gen = make_loaded_generator();

    auto c1 = gen.get_city({.country = "DE"}, 7777);
    auto c2 = gen.get_city({.country = "DE"}, 7777);
    REQUIRE(c1.geonameid == c2.geonameid);
    REQUIRE(c1.country_code == "DE");
}

// ---------------------------------------------------------------------------
// Uniform (non-weighted) generation
// ---------------------------------------------------------------------------

TEST_CASE("request weighted defaults to true", "[weighting]")
{
    REQUIRE(dasmig::city_request{}.weighted == true);
}

TEST_CASE("request weighted=false enables uniform selection", "[weighting]")
{
    auto gen = make_loaded_generator();
    dasmig::core::rng rng{99};

    // With uniform selection, smaller cities should appear much more often
    // than with weighted selection. Count cities below the median.
    // Use 100k as threshold — works for both lite and full tiers.
    int below_100k = 0;
    static constexpr int draws = 200;

    for (int i = 0; i < draws; ++i)
    {
        auto c = gen.get_city({.weighted = false}, rng());
        if (c.population < 100'000)
        {
            ++below_100k;
        }
    }

    // The vast majority of cities in any tier have pop < 100k.
    // Uniform selection should produce many of them (vs. weighted which
    // strongly favors the few large cities).
    REQUIRE(below_100k > draws / 2);
}

TEST_CASE("request weighted=false with country filter", "[weighting][country]")
{
    auto gen = make_loaded_generator();
    dasmig::core::rng rng{42};

    for (int i = 0; i < 20; ++i)
    {
        auto c = gen.get_city({.country = "JP", .weighted = false}, rng());
        REQUIRE(c.country_code == "JP");
    }
}

// ---------------------------------------------------------------------------
// Admin1 code resolution
// ---------------------------------------------------------------------------

TEST_CASE("has_admin1 - false before load", "[admin1]")
{
    dasmig::cg gen;
    REQUIRE_FALSE(gen.has_admin1());
}

TEST_CASE("load_admin1 - loads mappings from file", "[admin1]")
{
    auto tmp = std::filesystem::temp_directory_path() / "admin1_test.txt";
    {
        std::ofstream f{tmp};
        f << "US.CA\tCalifornia\tCalifornia\t5332921\n";
        f << "US.NY\tNew York\tNew York\t5128638\n";
        f << "DE.07\tNorth Rhine-Westphalia\tNorth Rhine-Westphalia\t2861876\n";
    }

    dasmig::cg gen;
    gen.load_admin1(tmp);
    REQUIRE(gen.has_admin1());

    std::filesystem::remove(tmp);
}

TEST_CASE("load_admin1 - non-existent path is a no-op", "[admin1]")
{
    dasmig::cg gen;
    gen.load_admin1("does_not_exist.txt");
    REQUIRE_FALSE(gen.has_admin1());
}

TEST_CASE("resolve_admin1 - returns name for known code", "[admin1]")
{
    auto tmp = std::filesystem::temp_directory_path() / "admin1_resolve.txt";
    {
        std::ofstream f{tmp};
        f << "US.CA\tCalifornia\tCalifornia\t5332921\n";
        f << "JP.40\tTokyo\tTokyo\t1850144\n";
    }

    dasmig::cg gen;
    gen.load_admin1(tmp);

    REQUIRE(gen.resolve_admin1("US", "CA") == "California");
    REQUIRE(gen.resolve_admin1("JP", "40") == "Tokyo");

    std::filesystem::remove(tmp);
}

TEST_CASE("resolve_admin1 - falls back to raw code for unknown", "[admin1]")
{
    auto tmp = std::filesystem::temp_directory_path() / "admin1_fallback.txt";
    {
        std::ofstream f{tmp};
        f << "US.CA\tCalifornia\tCalifornia\t5332921\n";
    }

    dasmig::cg gen;
    gen.load_admin1(tmp);

    // Unknown code in known country.
    REQUIRE(gen.resolve_admin1("US", "ZZ") == "ZZ");
    // Unknown country entirely.
    REQUIRE(gen.resolve_admin1("XX", "01") == "01");

    std::filesystem::remove(tmp);
}

TEST_CASE("resolve_admin1 - empty code returns empty string", "[admin1]")
{
    dasmig::cg gen;
    REQUIRE(gen.resolve_admin1("US", "").empty());
}

TEST_CASE("load_admin1 - a malformed line is an error", "[admin1]")
{
    auto tmp = std::filesystem::temp_directory_path() / "admin1_bad.txt";
    {
        std::ofstream f{tmp};
        f << "US.CA\tCalifornia\tCalifornia\t5332921\n";
        f << "no_tab_here\n";
        f << "\n";
        f << "DE.07\tNorth Rhine-Westphalia\tNorth Rhine-Westphalia\t2861876\n";
    }

    dasmig::cg gen;
    try
    {
        gen.load_admin1(tmp);
        FAIL("expected core::tsv_error");
    }
    catch (const dasmig::core::tsv_error& e)
    {
        REQUIRE(e.line() == 2); // "no_tab_here" is not COUNTRY.CODE
        REQUIRE(e.column() == "code");
    }
    REQUIRE_FALSE(gen.has_admin1()); // nothing was loaded

    std::filesystem::remove(tmp);
}

TEST_CASE("load_admin1 - handles CRLF line endings", "[admin1]")
{
    auto tmp = std::filesystem::temp_directory_path() / "admin1_crlf.txt";
    {
        std::ofstream f{tmp, std::ios::binary};
        f << "BR.27\tSão Paulo\tSao Paulo\t3448433\r\n";
        f << "BR.21\tRio de Janeiro\tRio de Janeiro\t3451189\r\n";
    }

    dasmig::cg gen;
    gen.load_admin1(tmp);
    REQUIRE(gen.resolve_admin1("BR", "27") == "São Paulo");
    REQUIRE(gen.resolve_admin1("BR", "21") == "Rio de Janeiro");

    std::filesystem::remove(tmp);
}

// ---------------------------------------------------------------------------
// Known answers (rule 2): the same seed and data give these exact results on
// every compiler and platform. A change here is a change of output and needs
// a major version.
// ---------------------------------------------------------------------------

namespace
{

dasmig::cg load_lite_file()
{
    dasmig::cg gen;
    for (const char* path : {"resources/lite/cities.tsv", "../resources/lite/cities.tsv"})
    {
        if (std::filesystem::is_regular_file(path))
        {
            gen.load(std::filesystem::path{path});
            break;
        }
    }
    REQUIRE(gen.has_data());
    return gen;
}

const std::string city_header =
    "geonameid\tname\tasciiname\tlatitude\tlongitude\tfeature_code\tcountry_code\tcc2\t"
    "admin1_code\tadmin2_code\tadmin3_code\tadmin4_code\tpopulation\televation\tdem\ttimezone\n";

std::string city_row(std::string_view id, std::string_view name, std::string_view country,
                     std::string_view population)
{
    return std::string{id} + "\t" + std::string{name} + "\t" + std::string{name} +
           "\t10.5\t20.25\tPPL\t" + std::string{country} + "\t\t01\t\t\t\t" + std::string{population} +
           "\t\t5\tUTC\n";
}

dasmig::cg make_request_generator()
{
    dasmig::cg gen;
    gen.extend_from_string(
        city_header + city_row("101", "Small XX", "XX", "0") +
            city_row("102", "Large XX", "XX", "999") +
            city_row("201", "Small YY", "YY", "1") +
            city_row("202", "Large YY", "YY", "999"),
        "request.tsv", dasmig::core::layer_mode::replace);
    return gen;
}

} // namespace

// ---------------------------------------------------------------------------
// Per-call requests
// ---------------------------------------------------------------------------

TEST_CASE("city_request defaults select globally with population weighting", "[request]")
{
    auto gen = make_request_generator();
    const dasmig::city_request request;
    REQUIRE_FALSE(request.country.has_value());
    REQUIRE(request.weighted);

    for (std::uint64_t seed = 0; seed < 64; ++seed)
    {
        REQUIRE(gen.get_city(request, seed).to_json() ==
                gen.get_city({.weighted = true}, seed).to_json());
    }
}

TEST_CASE("city_request country filters the selection",
          "[request][filtering][weighting]")
{
    auto gen = make_request_generator();
    for (const bool weighted : {true, false})
    {
        for (const std::string country : {"XX", "YY"})
        {
            for (std::uint64_t seed = 0; seed < 64; ++seed)
            {
                const auto c = gen.get_city({.country = country, .weighted = weighted}, seed);
                REQUIRE(c.country_code == country);
            }
        }
    }
}

TEST_CASE("city_request controls population-weighted and uniform probabilities",
          "[request][weighting]")
{
    const auto gen = make_request_generator();
    for (const auto& country : {std::optional<std::string>{}, std::optional<std::string>{"XX"},
                                std::optional<std::string>{"YY"}})
    {
        int weighted_large = 0;
        int uniform_large = 0;
        bool saw_zero_population = false;
        for (std::uint64_t seed = 0; seed < 1000; ++seed)
        {
            const auto weighted = gen.get_city({.country = country}, seed);
            const auto uniform = gen.get_city({.country = country, .weighted = false}, seed);
            if (weighted.population == 999) ++weighted_large;
            if (uniform.population == 999) ++uniform_large;
            if (weighted.population == 0) saw_zero_population = true;
        }
        REQUIRE(weighted_large > 980);
        REQUIRE(uniform_large > 400);
        REQUIRE(uniform_large < 600);
        if (country == "XX") REQUIRE(saw_zero_population);
    }
}

TEST_CASE("city_request replays every seed with the same data and options",
          "[request][seeding][json]")
{
    auto gen = make_request_generator();
    for (const bool weighted : {true, false})
    {
        for (const auto& country : {std::optional<std::string>{}, std::optional<std::string>{"XX"},
                                    std::optional<std::string>{"YY"}})
        {
            const dasmig::city_request request{.country = country, .weighted = weighted};
            for (const auto seed : {std::uint64_t{0}, std::uint64_t{42},
                                    std::numeric_limits<std::uint64_t>::max()})
            {
                const auto c = std::as_const(gen).get_city(request, seed);
                REQUIRE(c.seed() == seed);
                REQUIRE(c.signature().data == gen.data_hash());
                REQUIRE(gen.get_city(request, c.seed()).to_json() == c.to_json());
            }
        }
    }
}

TEST_CASE("city_request preserves empty-data and country errors", "[request][errors]")
{
    const dasmig::cg empty;
    for (const auto& country : {std::optional<std::string>{}, std::optional<std::string>{""},
                                std::optional<std::string>{"ZZ"}})
    {
        for (const bool weighted : {true, false})
        {
            REQUIRE_THROWS_AS(empty.get_city({.country = country, .weighted = weighted}, 42),
                              std::runtime_error);
            REQUIRE_THROWS_WITH(empty.get_city({.country = country, .weighted = weighted}, 42),
                                "No city data loaded. Call load() first.");
        }
    }

    const auto gen = make_request_generator();
    for (const std::string country : {"", "ZZ", "xx"})
    {
        for (const bool weighted : {true, false})
        {
            REQUIRE_THROWS_AS(gen.get_city({.country = country, .weighted = weighted}, 42),
                              std::invalid_argument);
            REQUIRE_THROWS_WITH(gen.get_city({.country = country, .weighted = weighted}, 42),
                                "No cities found for country code: " + country);
        }
    }
}

TEST_CASE("city_request does not leak filters or engine state", "[request][seeding]")
{
    auto gen = make_request_generator();
    auto control = make_request_generator();
    const auto hash = gen.data_hash();
    const auto count = gen.city_count();
    const auto layers = gen.layers().size();
    for (const bool weighted : {true, false})
    {
        for (std::uint64_t seed = 0; seed < 20; ++seed)
        {
            const dasmig::city_request request{.country = "XX", .weighted = !weighted};
            REQUIRE(gen.get_city(request, seed).country_code == "XX");
            REQUIRE(gen.get_city({.country = "YY", .weighted = !weighted}, seed).country_code == "YY");
            REQUIRE_THROWS_AS(gen.get_city({.country = "ZZ", .weighted = !weighted}, seed),
                              std::invalid_argument);
            REQUIRE(request.country == "XX");
            REQUIRE(request.weighted == !weighted);
            REQUIRE(gen.get_city({}, seed).to_json() == control.get_city({}, seed).to_json());
            REQUIRE(gen.get_city({.country = "YY"}, seed).to_json() ==
                    control.get_city({.country = "YY"}, seed).to_json());
        }
    }
    REQUIRE(gen.data_hash() == hash);
    REQUIRE(gen.city_count() == count);
    REQUIRE(gen.layers().size() == layers);
}

TEST_CASE("known answers for fixed seeds", "[kat]")
{
    auto gen = load_lite_file();
    REQUIRE(gen.data_hash() == 0x7e8af51013ede34aULL);

    REQUIRE(gen.get_city({}, 1).to_json() ==
            R"({"geonameid":953781,"name":"Soweto","asciiname":"Soweto","latitude":-26.26781,)"
            R"("longitude":27.85849,"feature_code":"PPL","country_code":"ZA","cc2":"","admin1_code":"06",)"
            R"("admin2_code":"JHB","admin3_code":"JHB","admin4_code":"","population":1695047,)"
            R"("elevation":null,"dem":1632,"timezone":"Africa/Johannesburg",)"
            R"("signature":{"seed":"0000000000000001","data":"7e8af51013ede34a"}})");
    REQUIRE(gen.get_city({.country = "BR"}, 3).geonameid == 3470127);
    REQUIRE(gen.get_city({.weighted = false}, 2).geonameid == 3446019);
    REQUIRE(gen.get_city({.country = "JP", .weighted = false}, 4).geonameid == 2112802);
    REQUIRE(gen.get_city({}, 1).geonameid == 953781);

    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::uint64_t s = 0; s < 10000; ++s)
    {
        for (const char c : gen.get_city({}, s).to_json())
        {
            h ^= static_cast<unsigned char>(c);
            h *= 0x100000001b3ULL;
        }
    }
    REQUIRE(h == 0x792f3ef7e0e4fd8cULL);
}

// ---------------------------------------------------------------------------
// JSON and signatures
// ---------------------------------------------------------------------------

TEST_CASE("to_json and signature", "[json]")
{
    dasmig::cg gen;
    gen.extend_from_string(city_header + city_row("101", "Alpha", "XX", "1000"), "one.tsv",
                           dasmig::core::layer_mode::replace);
    const auto c = gen.get_city({}, 5);
    REQUIRE(c.seed() == 5);
    REQUIRE(c.signature().data == gen.data_hash());
    REQUIRE(c.to_json() ==
            R"({"geonameid":101,"name":"Alpha","asciiname":"Alpha","latitude":10.5,"longitude":20.25,)"
            R"("feature_code":"PPL","country_code":"XX","cc2":"","admin1_code":"01","admin2_code":"",)"
            R"("admin3_code":"","admin4_code":"","population":1000,"elevation":null,"dem":5,"timezone":"UTC",)"
            R"("signature":{"seed":"0000000000000005","data":")" +
                dasmig::core::seed_signature{.seed = 0, .data = gen.data_hash()}.to_string().substr(17) +
                R"("}})");
}

// ---------------------------------------------------------------------------
// Extending data (rule 8)
// ---------------------------------------------------------------------------

TEST_CASE("extend adds, overrides and removes cities", "[extend]")
{
    auto gen = load_lite_file();
    const auto count = gen.city_count();
    const auto base_hash = gen.data_hash();

    // A new city in a new country, and Soweto renamed.
    const auto& info = gen.extend_from_string(
        city_header + city_row("101", "Atlantis City", "QQ", "500000") +
            city_row("953781", "Soweto Renamed", "ZA", "1695047"),
        "atlantis.tsv");
    REQUIRE(info.added == 1);
    REQUIRE(info.replaced == 1);
    REQUIRE(gen.city_count() == count + 1);
    REQUIRE(gen.get_city({.country = "QQ"}).name == "Atlantis City");
    REQUIRE(gen.get_city({}, 1).name == "Soweto Renamed"); // same draw, new data
    REQUIRE(gen.data_hash() != base_hash);

    gen.extend_from_string("geonameid\tdisabled\n101\t1\n", "sink.tsv");
    REQUIRE(gen.city_count() == count);
    REQUIRE_THROWS_AS(gen.get_city({.country = "QQ"}), std::invalid_argument);

    gen.load(std::filesystem::path{gen.layers().front().source});
    REQUIRE(gen.data_hash() == base_hash);
}

TEST_CASE("a bad layer is rejected and leaves the data unchanged", "[extend][errors]")
{
    auto gen = load_lite_file();
    const auto hash = gen.data_hash();
    const auto count = gen.city_count();

    REQUIRE_THROWS_WITH(gen.extend_from_string(city_header + city_row("101", "X", "QQ", "lots"), "bad.tsv"),
                        "bad.tsv:2: column 'population': expected a number, got 'lots'");
    REQUIRE_THROWS_WITH(gen.extend_from_string(city_header + city_row("0", "X", "QQ", "5"), "zero.tsv"),
                        "zero.tsv:2: column 'geonameid': must be positive");
    std::string bad_elevation = city_header + city_row("101", "X", "QQ", "5");
    bad_elevation.replace(bad_elevation.find("\t\t5\tUTC"), 7, "\t99999\t5\tUTC");
    REQUIRE_THROWS_AS(gen.extend_from_string(bad_elevation, "elev.tsv"), dasmig::core::tsv_error);
    REQUIRE_THROWS_AS(gen.extend_from_string(city_header + city_row("953781", "X", "ZA", "5"), "clash.tsv",
                                             dasmig::core::layer_mode::add),
                      dasmig::core::tsv_error);

    REQUIRE(gen.data_hash() == hash);
    REQUIRE(gen.city_count() == count);
    REQUIRE(gen.layers().size() == 1);
}

#ifdef DASMIG_CITY_EMBED
TEST_CASE("compiled-in data needs no files and matches them", "[embed]")
{
    const auto old_cwd = std::filesystem::current_path();
    const auto tmp = std::filesystem::temp_directory_path() / "cg_embed_dir";
    std::filesystem::create_directories(tmp);
    std::filesystem::current_path(tmp);
    dasmig::cg gen;
    const bool loaded = gen.load(dasmig::dataset::lite);
    std::filesystem::current_path(old_cwd);
    std::filesystem::remove_all(tmp);

    REQUIRE(loaded);
    REQUIRE(gen.layers().front().source == "embedded:lite/cities.tsv");
    REQUIRE(gen.data_hash() == 0x7e8af51013ede34aULL); // same as the file
    REQUIRE(gen.get_city({}, 1).geonameid == 953781);
}
#endif
