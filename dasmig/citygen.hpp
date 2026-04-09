#ifndef DASMIG_CITYGEN_HPP
#define DASMIG_CITYGEN_HPP

#include "random.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <random>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/// @file citygen.hpp
/// @brief City generator library — procedural city generation for C++23.
/// @author Diego Dasso Migotto (diegomigotto at hotmail dot com)
/// @see See doc/usage.md for the narrative tutorial.

namespace dasmig
{

/// @brief Dataset size tier for resource loading.
enum class dataset : std::uint8_t
{
    lite, ///< ~25k major cities (population >= 15,000).
    full  ///< ~200k cities (population >= 500).
};

/// @brief Return type for city generation, holding all GeoNames fields.
///
/// Supports implicit conversion to std::string (returns the city name)
/// and streaming via operator<<.
class city
{
  public:
    /// @brief GeoNames primary key.
    std::uint32_t geonameid{0};

    /// @brief UTF-8 city name.
    std::string name;

    /// @brief Plain ASCII transliteration of the name.
    std::string asciiname;

    /// @brief WGS84 latitude in decimal degrees.
    double latitude{0.0};

    /// @brief WGS84 longitude in decimal degrees.
    double longitude{0.0};

    /// @brief GeoNames feature code (PPL, PPLA, PPLC, etc.).
    std::string feature_code;

    /// @brief ISO-3166 two-letter country code.
    std::string country_code;

    /// @brief Alternate country codes (comma-separated).
    std::string cc2;

    /// @brief First-level administrative division code (state/province).
    std::string admin1_code;

    /// @brief Second-level administrative division code (county/district).
    std::string admin2_code;

    /// @brief Third-level administrative division code (township/commune).
    std::string admin3_code;

    /// @brief Fourth-level administrative division code (sub-district).
    std::string admin4_code;

    /// @brief City population.
    std::uint64_t population{0};

    /// @brief Elevation in meters (-9999 if unknown).
    std::int16_t elevation{-9999};

    /// @brief Digital elevation model value (more reliable than elevation).
    std::int16_t dem{0};

    /// @brief IANA timezone identifier (e.g., "Europe/London").
    std::string timezone;

    /// @brief Retrieve the random seed used to generate this city.
    /// @return The per-call seed for replay.
    /// @see cg::get_city(std::uint64_t)
    [[nodiscard]] std::uint64_t seed() const
    {
        return _seed;
    }

    /// @brief Implicit conversion to std::string.
    /// @return The UTF-8 city name.
    operator std::string() const // NOLINT(hicpp-explicit-conversions)
    {
        return name;
    }

    /// @brief Stream the city name to an output stream.
    /// @param os Output stream.
    /// @param c City to stream.
    /// @return Reference to the output stream.
    friend std::ostream& operator<<(std::ostream& os, const city& c)
    {
        os << c.name;
        return os;
    }

  private:
    std::uint64_t _seed{0}; ///< Random seed used to generate this city.

    friend class cg; ///< Allows cg to set the seed.
};

/// @brief City generator that produces random cities from GeoNames data.
///
/// Generates cities using population-weighted random selection from
/// GeoNames data. Larger cities are proportionally more likely to be
/// selected, mirroring real-world population distributions.
///
/// Can be used as a singleton via instance() or constructed independently.
/// Independent instances own their own data and random engine,
/// making them safe for concurrent use without shared state.
///
/// @par Thread safety
/// Each instance is independent. Concurrent calls to get_city() on
/// the **same** instance require external synchronization. load() mutates
/// internal state and must not be called concurrently with get_city()
/// on the same instance.
class cg
{
  public:
    /// @brief Default constructor — creates an empty generator with no data.
    ///
    /// Call load() to populate city data before generating.
    cg() = default;

    cg(const cg&) = delete;            ///< Not copyable.
    cg& operator=(const cg&) = delete; ///< Not copyable.
    cg(cg&&) = default;                ///< Move constructor.
    cg& operator=(cg&&) = default;     ///< Move assignment.
    ~cg() = default;                   ///< Default destructor.

    /// @brief Access the global singleton instance.
    ///
    /// The singleton auto-probes common resource paths on first access.
    /// For independent generators (e.g., inside an entity-generator
    /// component), prefer constructing a separate cg instance.
    /// @return Reference to the global cg instance.
    static cg& instance()
    {
        static cg inst{auto_probe_tag{}};
        return inst;
    }

    /// @brief Generate a random city, weighted by population.
    /// @return A city object with all GeoNames fields.
    /// @throws std::runtime_error If no city data has been loaded.
    [[nodiscard]] city get_city()
    {
        auto call_seed = static_cast<std::uint64_t>(_engine());
        return get_city(call_seed);
    }

    /// @brief Generate a deterministic city using a specific seed.
    ///
    /// Given the same loaded data and seed, this method always produces the
    /// same city. Retrieve the seed from a previous city via city::seed().
    ///
    /// @param call_seed Seed for reproducible results.
    /// @return A city object with all GeoNames fields.
    /// @throws std::runtime_error If no city data has been loaded.
    [[nodiscard]] city get_city(std::uint64_t call_seed) const
    {
        if (_cities.empty())
        {
            throw std::runtime_error(
                "No city data loaded. Call load() first.");
        }

        effolkronium::random_local call_engine;
        call_engine.seed(static_cast<std::mt19937::result_type>(
            (call_seed ^ (call_seed >> 32U))));

        auto idx = _distribution(call_engine.engine());
        city result = _cities[idx];
        result._seed = call_seed;
        return result;
    }

    /// @brief Generate a random city filtered by country code.
    /// @param country ISO-3166 two-letter country code (e.g., "BR", "US").
    /// @return A city object from the specified country.
    /// @throws std::runtime_error If no city data has been loaded.
    /// @throws std::invalid_argument If no cities match the country code.
    [[nodiscard]] city get_city(const std::string& country)
    {
        auto call_seed = static_cast<std::uint64_t>(_engine());
        return get_city(country, call_seed);
    }

    /// @brief Generate a deterministic city filtered by country code.
    /// @param country ISO-3166 two-letter country code.
    /// @param call_seed Seed for reproducible results.
    /// @return A city object from the specified country.
    /// @throws std::runtime_error If no city data has been loaded.
    /// @throws std::invalid_argument If no cities match the country code.
    [[nodiscard]] city get_city(const std::string& country,
                                std::uint64_t call_seed) const
    {
        if (_cities.empty())
        {
            throw std::runtime_error(
                "No city data loaded. Call load() first.");
        }

        auto it = _country_index.find(country);
        if (it == _country_index.end())
        {
            throw std::invalid_argument(
                "No cities found for country code: " + country);
        }

        effolkronium::random_local call_engine;
        call_engine.seed(static_cast<std::mt19937::result_type>(
            (call_seed ^ (call_seed >> 32U))));

        const auto& idx = it->second;
        auto selected = idx.distribution(call_engine.engine());
        city result = _cities[idx.city_indices[selected]];
        result._seed = call_seed;
        return result;
    }

    /// @name Seeding
    /// @{

    /// @brief Seed the internal random engine for deterministic sequences.
    ///
    /// Subsequent get_city() calls (without an explicit seed) draw
    /// per-call seeds from this engine, producing a reproducible sequence.
    ///
    /// @param seed_value The seed value.
    /// @return `*this` for chaining.
    cg& seed(std::uint64_t seed_value)
    {
        _engine.seed(seed_value);
        return *this;
    }

    /// @brief Reseed the engine with a non-deterministic source.
    ///
    /// Subsequent get_city() calls will produce non-reproducible results.
    /// @return `*this` for chaining.
    cg& unseed()
    {
        _engine.seed(std::random_device{}());
        return *this;
    }

    /// @}

    /// @brief Check whether any city data has been loaded.
    /// @return `true` if at least one city is available.
    [[nodiscard]] bool has_data() const
    {
        return !_cities.empty();
    }

    /// @brief Return the number of loaded cities.
    /// @return City count.
    [[nodiscard]] std::size_t city_count() const
    {
        return _cities.size();
    }

    /// @brief Load city data from a TSV file.
    ///
    /// Expects a tab-delimited file with a header row and 16 columns
    /// matching the GeoNames schema (as produced by
    /// scripts/prepare_geonames.py). Safe to call multiple times to add
    /// from different files.
    ///
    /// @param tsv_path Path to the TSV file.
    void load(const std::filesystem::path& tsv_path)
    {
        if (!std::filesystem::exists(tsv_path) ||
            !std::filesystem::is_regular_file(tsv_path))
        {
            return;
        }

        std::ifstream file{tsv_path};
        if (!file.is_open())
        {
            return;
        }

        std::string line;

        // Skip header row.
        if (!std::getline(file, line))
        {
            return;
        }

        while (std::getline(file, line))
        {
            if (line.empty())
            {
                continue;
            }

            // Strip trailing carriage return.
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            auto c = parse_line(line);
            if (c.geonameid != 0)
            {
                _cities.push_back(std::move(c));
            }
        }

        rebuild_indices();
    }

    /// @brief Load a specific dataset tier from a base resources directory.
    ///
    /// Probes common base paths ("resources", "../resources",
    /// "city-generator/resources") and loads from the `lite/` or `full/`
    /// subfolder according to @p tier.
    ///
    /// @param tier The dataset size to load (dataset::lite or dataset::full).
    /// @return `true` if a matching directory was found and loaded.
    bool load(dataset tier)
    {
        static constexpr std::array probe_paths = {
            "resources", "../resources", "city-generator/resources"};

        const char* subfolder =
            (tier == dataset::full) ? "full" : "lite";

        auto found = std::ranges::find_if(probe_paths, [&](const char* base) {
            const auto tsv =
                std::filesystem::path{base} / subfolder / "cities.tsv";
            return std::filesystem::is_regular_file(tsv);
        });
        if (found != probe_paths.end())
        {
            load(std::filesystem::path{*found} / subfolder / "cities.tsv");
            return true;
        }
        return false;
    }

  private:
    // All loaded cities.
    std::vector<city> _cities;

    // Population-weighted distribution over _cities indices.
    mutable std::discrete_distribution<std::size_t> _distribution;

    // Pre-built per-country index for O(1) country-filtered lookups.
    struct country_entry
    {
        std::vector<std::size_t> city_indices;
        mutable std::discrete_distribution<std::size_t> distribution;
    };

    std::unordered_map<std::string, country_entry> _country_index;

    // Per-instance random engine for seed drawing.
    std::mt19937_64 _engine{std::random_device{}()};

    // Tag type for the auto-probing singleton constructor.
    struct auto_probe_tag {};

    // Singleton constructor: auto-probes common resource locations.
    explicit cg(auto_probe_tag /*tag*/)
    {
        static constexpr std::array probe_paths = {
            "resources", "../resources", "city-generator/resources"};

        auto found = std::ranges::find_if(probe_paths, [](const char* p) {
            return std::filesystem::exists(p) &&
                   std::filesystem::is_directory(p);
        });
        if (found != probe_paths.end())
        {
            const std::filesystem::path base{*found};
            auto lite_tsv = base / "lite" / "cities.tsv";
            auto full_tsv = base / "full" / "cities.tsv";
            if (std::filesystem::is_regular_file(lite_tsv))
            {
                load(lite_tsv);
            }
            else if (std::filesystem::is_regular_file(full_tsv))
            {
                load(full_tsv);
            }
        }
    }

    // Rebuild the global distribution and country index.
    void rebuild_indices()
    {
        // Global distribution.
        std::vector<double> weights;
        weights.reserve(_cities.size());

        std::ranges::transform(_cities, std::back_inserter(weights),
                               [](const city& c) {
                                   return static_cast<double>(
                                       std::max<std::uint64_t>(c.population, 1));
                               });

        _distribution = std::discrete_distribution<std::size_t>(
            weights.begin(), weights.end());

        // Country index.
        _country_index.clear();

        for (auto&& [i, c] : _cities | std::views::enumerate)
        {
            auto& entry = _country_index[c.country_code];
            entry.city_indices.push_back(static_cast<std::size_t>(i));
        }

        // Build per-country distributions.
        for (auto& [code, entry] : _country_index)
        {
            std::vector<double> cw;
            cw.reserve(entry.city_indices.size());

            for (auto idx : entry.city_indices)
            {
                cw.push_back(static_cast<double>(
                    std::max<std::uint64_t>(_cities[idx].population, 1)));
            }

            entry.distribution = std::discrete_distribution<std::size_t>(
                cw.begin(), cw.end());
        }
    }

    // Parse a single tab-delimited line into a city object.
    // Expected 16 fields: geonameid, name, asciiname, latitude, longitude,
    // feature_code, country_code, cc2, admin1..4, population, elevation,
    // dem, timezone.
    static city parse_line(const std::string& line)
    {
        city c;

        // Split by tabs using C++23 views::split.
        std::vector<std::string> fields;
        fields.reserve(16);

        for (auto part : line | std::views::split('\t'))
        {
            fields.emplace_back(std::ranges::begin(part),
                                std::ranges::end(part));
        }

        // Expected field count.
        static constexpr std::size_t expected_fields{16};

        if (fields.size() < expected_fields)
        {
            return c; // geonameid stays 0, skipped by caller.
        }

        try
        {
            c.geonameid = static_cast<std::uint32_t>(std::stoul(fields[0]));
            c.name = fields[1];
            c.asciiname = fields[2];
            c.latitude = std::stod(fields[3]);
            c.longitude = std::stod(fields[4]);
            c.feature_code = fields[5];
            c.country_code = fields[6];
            c.cc2 = fields[7];
            c.admin1_code = fields[8];
            c.admin2_code = fields[9];
            c.admin3_code = fields[10];
            c.admin4_code = fields[11];
            c.population =
                fields[12].empty()
                    ? 0
                    : static_cast<std::uint64_t>(std::stoull(fields[12]));
            c.elevation =
                fields[13].empty()
                    ? static_cast<std::int16_t>(-9999)
                    : static_cast<std::int16_t>(std::stoi(fields[13]));
            c.dem = fields[14].empty()
                        ? static_cast<std::int16_t>(0)
                        : static_cast<std::int16_t>(std::stoi(fields[14]));
            c.timezone = fields[15];
        }
        catch (...)
        {
            c.geonameid = 0; // Mark as invalid.
        }

        return c;
    }
};

} // namespace dasmig

#endif // DASMIG_CITYGEN_HPP
