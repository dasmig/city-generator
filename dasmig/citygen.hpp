// SPDX-License-Identifier: MIT
// Copyright (c) 2020-2026 Diego Dasso Migotto

#ifndef DASMIG_CITYGEN_HPP
#define DASMIG_CITYGEN_HPP

#include "core.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef DASMIG_CITY_EMBED
#include "citygen_data.hpp"
#endif

/// @file citygen.hpp
/// @brief City generator library — procedural city generation for C++23.
/// @author Diego Dasso Migotto (diegomigotto at hotmail dot com)
/// @see See doc/usage.md for the narrative tutorial.

DASMIG_CORE_REQUIRE(1, 3);

namespace dasmig
{

/// @brief Per-call city selection options.
struct city_request
{
    /// @brief ISO-3166 two-letter country code, or no filter when absent.
    /// An explicit empty or unknown code is an error if no cities match.
    std::optional<std::string> country{std::nullopt};

    /// @brief Population-weighted selection (default); false selects uniformly.
    bool weighted{true};
};

/// @brief A point on Earth and the filters for the nearest city to it.
/// @code
/// auto c = gen.find_nearest({.latitude = -23.55, .longitude = -46.63});
/// @endcode
struct nearest_request
{
    /// @brief WGS84 latitude in decimal degrees, -90 to 90.
    double latitude{0.0};

    /// @brief WGS84 longitude in decimal degrees, -180 to 180.
    double longitude{0.0};

    /// @brief ISO-3166 two-letter country code, or no filter when absent.
    /// A code no loaded city has finds nothing.
    std::optional<std::string> country{std::nullopt};

    /// @brief Skip cities with fewer people than this.
    std::uint64_t min_population{0};
};

/// @brief Return type for city generation, holding all GeoNames fields.
///
/// Supports implicit conversion to std::string (returns the city name),
/// streaming via operator<< and JSON output via to_json().
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

    /// @brief Population count.
    std::uint64_t population{0};

    /// @brief Elevation in metres (-9999 if unknown).
    std::int16_t elevation{-9999};

    /// @brief Digital elevation model value in metres (SRTM3 or GTOPO30).
    std::int16_t dem{0};

    /// @brief IANA timezone identifier (e.g. "America/Sao_Paulo").
    std::string timezone;

    /// @brief Retrieve the random seed used to generate this city.
    /// @return The per-call seed for replay.
    /// @see cg::get_city(const city_request&)
    /// @see cg::get_city(const city_request&, std::uint64_t)
    [[nodiscard]] std::uint64_t seed() const
    {
        return _signature.seed;
    }

    /// @brief The seed and the data this city was drawn from.
    /// @return The replay signature; `data` is cg::data_hash() at the time
    ///         of generation.
    [[nodiscard]] const core::seed_signature& signature() const
    {
        return _signature;
    }

    /// @brief Write this city as a JSON object.
    ///
    /// `elevation` is null when unknown (-9999).
    ///
    /// @param w Writer positioned where a value is allowed.
    void to_json(core::json_writer& w) const
    {
        w.begin_object()
            .field("geonameid", geonameid)
            .field("name", name)
            .field("asciiname", asciiname)
            .field("latitude", latitude)
            .field("longitude", longitude)
            .field("feature_code", feature_code)
            .field("country_code", country_code)
            .field("cc2", cc2)
            .field("admin1_code", admin1_code)
            .field("admin2_code", admin2_code)
            .field("admin3_code", admin3_code)
            .field("admin4_code", admin4_code)
            .field("population", population)
            .key("elevation");
        if (elevation == unknown_elevation)
        {
            w.value(nullptr);
        }
        else
        {
            w.value(static_cast<int>(elevation));
        }
        w.field("dem", static_cast<int>(dem)).field("timezone", timezone).key("signature");
        _signature.to_json(w);
        w.end_object();
    }

    /// @brief This city as a JSON document.
    /// @return Compact JSON.
    [[nodiscard]] std::string to_json() const
    {
        core::json_writer w;
        to_json(w);
        return w.str();
    }

    /// @brief Implicit conversion to std::string.
    /// @return The city name.
    operator std::string() const // NOLINT(hicpp-explicit-conversions)
    {
        return name;
    }

    /// @brief Stream the city name to an output stream.
    /// @param os Output stream.
    /// @param c City to stream.
    /// @return The stream.
    friend std::ostream& operator<<(std::ostream& os, const city& c)
    {
        os << c.name;
        return os;
    }

    /// @brief The elevation value that means "unknown".
    static constexpr std::int16_t unknown_elevation = -9999;

  private:
    core::seed_signature _signature;
    friend class cg;
};

/// @brief City generator that produces random cities from GeoNames data.
///
/// Generates cities using population-weighted random selection by
/// default. Larger cities are proportionally more likely to be selected,
/// mirroring real-world demographic distributions.
///
/// Output is identical on every compiler and platform (dasmig-core).
///
/// Can be used as a singleton via instance() or constructed independently.
///
/// @par Thread safety
/// Each instance is independent. Concurrent calls to get_city() on the
/// **same** instance require external synchronization. load() and
/// extend() mutate internal state and must not be called concurrently
/// with get_city() on the same instance.
class cg
{
  public:
    /// @brief Default constructor — creates an empty generator with no data.
    cg() = default;

    cg(const cg&) = delete;            ///< Not copyable.
    cg& operator=(const cg&) = delete; ///< Not copyable.
    cg(cg&&) noexcept = default;       ///< Move constructor.
    cg& operator=(cg&&) noexcept = default; ///< Move assignment.
    ~cg() = default;                   ///< Default destructor.

    /// @brief Access the global singleton instance.
    ///
    /// Loads the `lite` tier (or `full` if `lite` is missing) on first
    /// access, from compiled-in data or an auto-probed `resources/`.
    /// @return Reference to the global cg instance.
    static cg& instance()
    {
        static cg inst{auto_probe_tag{}};
        return inst;
    }

    // -- Generation -------------------------------------------------------

    /// @brief Generate a random city using the internal engine.
    ///        Use `{}` for the defaults.
    /// @param request Country filter and selection mode for this call only.
    /// @return A city object with all fields.
    /// @throws std::runtime_error If no data has been loaded.
    /// @throws std::invalid_argument If no cities match the requested country.
    [[nodiscard]] city get_city(const city_request& request)
    {
        return get_city(request, _engine());
    }

    /// @brief Generate a deterministic city with explicit per-call options.
    ///
    /// Replay requires the same seed, request and loaded data. The internal
    /// engine is not changed.
    ///
    /// @param request Country filter and selection mode for this call only.
    /// @param call_seed Seed for reproducible results.
    /// @return A city object with all fields and its replay signature.
    /// @throws std::runtime_error If no data has been loaded.
    /// @throws std::invalid_argument If no cities match the requested country.
    [[nodiscard]] city get_city(const city_request& request,
                                std::uint64_t call_seed) const
    {
        require_data_();
        auto r = core::rng{call_seed}.split("city");
        if (!request.country)
        {
            const auto idx = request.weighted
                                 ? _distribution(r)
                                 : static_cast<std::size_t>(core::bounded(r, _cities.size()));
            return result_(_cities[idx], call_seed);
        }
        const auto it = _country_index.find(*request.country);
        if (it == _country_index.end())
        {
            throw std::invalid_argument("No cities found for country code: " + *request.country);
        }
        const auto& entry = it->second;
        const auto selected = request.weighted
                                  ? entry.distribution(r)
                                  : static_cast<std::size_t>(core::bounded(r, entry.indices.size()));
        return result_(_cities[entry.indices[selected]], call_seed);
    }

    // -- Lookup -----------------------------------------------------------

    /// @brief Find the loaded city closest to a point.
    ///
    /// This is a lookup, not a draw: it uses no random numbers, takes no seed
    /// and gives the same city for the same point and loaded data. Distance is
    /// the great-circle distance on a sphere; when two cities are equally
    /// close, the one earlier in the data wins. The returned city has an empty
    /// signature.
    ///
    /// @param request The point, and optionally a country and a minimum
    ///        population to filter by.
    /// @return The nearest city, or no value when none matches the filters.
    /// @throws std::runtime_error If no data has been loaded.
    /// @throws std::invalid_argument If the latitude is outside -90 to 90 or
    ///         the longitude is outside -180 to 180 (or either is not a number).
    [[nodiscard]] std::optional<city> find_nearest(const nearest_request& request) const
    {
        require_data_();
        if (!(request.latitude >= -90.0 && request.latitude <= 90.0) ||
            !(request.longitude >= -180.0 && request.longitude <= 180.0))
        {
            throw std::invalid_argument("Latitude must be -90 to 90 and longitude -180 to 180");
        }

        // core::math, not std::sin and std::cos, so a point almost halfway
        // between two cities finds the same one on every platform.
        const auto unit = [](double latitude, double longitude)
        {
            const auto lat = core::math::sin_cos_deg(latitude);
            const auto lon = core::math::sin_cos_deg(longitude);
            return std::array<double, 3>{lat.cos * lon.cos, lat.cos * lon.sin, lat.sin};
        };
        const auto target = unit(request.latitude, request.longitude);

        std::optional<std::size_t> best;
        double best_distance = 0.0;
        const auto consider = [&](std::size_t i)
        {
            const auto& c = _cities[i];
            if (c.population < request.min_population) return;
            const auto v = unit(c.latitude, c.longitude);
            const double dx = v[0] - target[0];
            const double dy = v[1] - target[1];
            const double dz = v[2] - target[2];
            // The chord, which grows with the arc.
            const double distance =
                core::math::fenced(dx * dx) + core::math::fenced(dy * dy) + core::math::fenced(dz * dz);
            if (!best || distance < best_distance)
            {
                best = i;
                best_distance = distance;
            }
        };

        if (request.country)
        {
            const auto it = _country_index.find(*request.country);
            if (it == _country_index.end()) return std::nullopt;
            for (const auto i : it->second.indices) consider(i);
        }
        else
        {
            for (std::size_t i = 0; i < _cities.size(); ++i) consider(i);
        }
        if (!best) return std::nullopt;
        return _cities[*best];
    }

    // -- Batch generation -------------------------------------------------

    /// @brief Generate a batch of city results from a seed drawn like
    ///        get_city() draws its own.
    ///
    /// Item `i` is `get_city(request, core::item_seed(batch_seed, options.first + i))`
    /// for the drawn batch seed; see the seeded overload.
    ///
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @return The results, in index order.
    [[nodiscard]] std::vector<city> get_city_batch(const city_request& request,
                                                  const core::batch_options& options)
    {
        return get_city_batch(request, options, _engine());
    }

    /// @brief Generate a batch of city results, deterministically.
    ///
    /// Item `i` is exactly `get_city(request, core::item_seed(batch_seed,
    /// options.first + i))`, so an item replays on its own, a range can be
    /// generated directly, two batches that follow each other equal one, and
    /// the output does not depend on `options.threads`.
    ///
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness (see
    ///        core::unique_rule; the rule must be made for `city`).
    /// @param batch_seed Seed of the whole batch.
    /// @return The results, in index order.
    /// @throws std::invalid_argument If the request or options are invalid.
    /// @throws std::runtime_error If no data has been loaded, or a unique
    ///         batch runs out of attempts.
    [[nodiscard]] std::vector<city> get_city_batch(const city_request& request,
                                                  const core::batch_options& options,
                                                  std::uint64_t batch_seed) const
    {
        return core::run_batch(options, batch_seed, [&](std::uint64_t item_seed)
                               { return get_city(request, item_seed); });
    }

    /// @brief Stream a batch of city results to a sink without holding the
    ///        whole batch.
    ///
    /// Generates the same items as get_city_batch() and calls
    /// `sink(index, item)` in index order on the calling thread; `index`
    /// includes `options.first`.
    ///
    /// @tparam Sink Callable taking `(std::uint64_t, city&)`; for example
    ///         core::jsonl_writer or core::csv_writer.
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @param batch_seed Seed of the whole batch.
    /// @param sink Receives the items.
    template <typename Sink>
    void for_each_city(const city_request& request, const core::batch_options& options,
                        std::uint64_t batch_seed, Sink&& sink) const
    {
        core::for_each_batch(options, batch_seed,
                             [&](std::uint64_t item_seed) { return get_city(request, item_seed); },
                             std::forward<Sink>(sink));
    }

    /// @brief Stream a batch of city results with a batch seed drawn like
    ///        get_city() draws its own.
    /// @tparam Sink Callable taking `(std::uint64_t, city&)`.
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @param sink Receives the items.
    template <typename Sink>
    void for_each_city(const city_request& request, const core::batch_options& options,
                        Sink&& sink)
    {
        for_each_city(request, options, _engine(), std::forward<Sink>(sink));
    }


    // -- Admin1 resolution ------------------------------------------------

    /// @brief Load first-level administrative division names, replacing
    ///        any loaded before.
    ///
    /// Reads a GeoNames `admin1CodesASCII.txt` file: no header, four
    /// tab-separated columns (`CC.CODE`, name, ASCII name, geonameid).
    /// A path that is not a regular file is ignored.
    ///
    /// @param path Path to the admin1 codes file.
    /// @throws core::tsv_error If the file is malformed; the loaded names
    ///         are then unchanged.
    void load_admin1(const std::filesystem::path& path)
    {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) return;
        const auto doc = core::tsv_document::load(path, {"code", "name", "asciiname", "geonameid"});
        std::map<std::string, std::string, std::less<>> names;
        for (const auto& row : doc.rows())
        {
            const auto code = row.get<std::string>("code");
            if (!code.contains('.'))
            {
                throw row.error("code", "expected COUNTRY.CODE, got '" + code + "'");
            }
            names.insert_or_assign(code, row.get<std::string>("name"));
        }
        _admin1_names = std::move(names);
    }

    /// @brief Resolve an admin1 code to its human-readable name.
    ///
    /// @param country_code ISO-3166 two-letter country code.
    /// @param admin1_code First-level admin division code.
    /// @return The admin1 name, or @p admin1_code if not found, or empty
    ///         if @p admin1_code is empty.
    [[nodiscard]] std::string resolve_admin1(const std::string& country_code,
                                             const std::string& admin1_code) const
    {
        if (admin1_code.empty()) return {};
        const auto it = _admin1_names.find(country_code + "." + admin1_code);
        if (it != _admin1_names.end()) return it->second;
        return admin1_code;
    }

    /// @brief Check whether admin1 name data has been loaded.
    /// @return True when names are loaded.
    [[nodiscard]] bool has_admin1() const
    {
        return !_admin1_names.empty();
    }

    // -- Data management --------------------------------------------------

    /// @brief Check whether any city data has been loaded.
    /// @return True when at least one city is loaded.
    [[nodiscard]] bool has_data() const
    {
        return !_cities.empty();
    }

    /// @brief Return the number of loaded cities.
    /// @return The count.
    [[nodiscard]] std::size_t city_count() const
    {
        return _cities.size();
    }

    /// @brief Load city data from a TSV file, replacing all data and layers
    ///        loaded before.
    ///
    /// Expects the `cities.tsv` columns (see doc/data.md). A path that is
    /// not a regular file is ignored.
    ///
    /// @param tsv_path Path to the TSV file.
    /// @throws core::tsv_error If the file is malformed; the data is then
    ///         unchanged.
    void load(const std::filesystem::path& tsv_path)
    {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(tsv_path, ec)) return;
        (void)extend(core::tsv_document::load(tsv_path), core::layer_mode::replace);
    }

    /// @brief Load a dataset tier, replacing all data and layers loaded
    ///        before.
    ///
    /// Uses the compiled-in copy when the library is built with
    /// `DASMIG_CITY_EMBED`, otherwise searches `resources/`,
    /// `../resources`, `city-generator/resources` and
    /// `../city-generator/resources`.
    ///
    /// @param tier The dataset size to load.
    /// @return `true` if the tier was found and loaded.
    /// @throws core::tsv_error If the file is malformed.
    [[nodiscard]] bool load(dataset tier)
    {
        const std::string path = std::string{core::tier_name(tier)} + "/" + std::string{data_file_};
        auto doc = core::load_resource(path, embedded_files_(),
                                       core::find_resources(library_name_, path));
        if (!doc) return false;
        (void)extend(std::move(*doc), core::layer_mode::replace);
        return true;
    }

    /// @brief Add a data layer from a TSV file with the `cities.tsv`
    ///        columns.
    ///
    /// Rows are keyed by `geonameid`. In the default override mode a row
    /// replaces the loaded city with the same id and new ids are added; a
    /// row with `disabled` set to 1 removes its city.
    ///
    /// @param file The layer file.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did.
    /// @throws core::tsv_error If the file cannot be read or a row is
    ///         invalid; the data is then unchanged.
    const core::layer_info& extend(const std::filesystem::path& file,
                                   core::layer_mode mode = core::layer_mode::override)
    {
        return extend(core::tsv_document::load(file), mode);
    }

    /// @brief Add a data layer from TSV text.
    /// @param text TSV with the `cities.tsv` columns.
    /// @param source Name used in errors.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did.
    /// @throws core::tsv_error If a row is invalid; the data is unchanged.
    const core::layer_info& extend_from_string(std::string_view text, std::string source,
                                               core::layer_mode mode = core::layer_mode::override)
    {
        return extend(core::tsv_document::parse(text, std::move(source)), mode);
    }

    /// @brief Add a data layer.
    /// @param doc TSV with the `cities.tsv` columns.
    /// @param mode How the layer combines with the data loaded before.
    /// @return What the layer did.
    /// @throws core::tsv_error If a row is invalid; the data is unchanged.
    const core::layer_info& extend(core::tsv_document doc,
                                   core::layer_mode mode = core::layer_mode::override)
    {
        const auto& info = _table.extend(std::move(doc), mode, [](const core::tsv_row& row) {
            (void)parse_(row);
        });
        rebuild_();
        return info;
    }

    /// @brief The data layers in effect, in order.
    /// @return The layer records.
    [[nodiscard]] const std::vector<core::layer_info>& layers() const
    {
        return _table.layers();
    }

    /// @brief Hash of the loaded city data, recorded in each result's
    ///        signature(). Admin1 names are not included: they do not
    ///        affect which city is drawn.
    /// @return The hash; 0 when nothing is loaded.
    [[nodiscard]] std::uint64_t data_hash() const { return _table.data_hash(); }

  private:
    static constexpr std::string_view library_name_ = "city-generator";
    static constexpr std::string_view data_file_ = "cities.tsv";

    static core::table_schema schema_()
    {
        return {.key = {"geonameid"},
                .required = {"name", "latitude", "longitude", "country_code", "population",
                             "timezone"},
                .optional = {"asciiname", "feature_code", "cc2", "admin1_code", "admin2_code",
                             "admin3_code", "admin4_code", "elevation", "dem"}};
    }

    // Parse and check a row; throws core::tsv_error located at the row.
    static city parse_(const core::tsv_row& row)
    {
        city c;
        c.geonameid = row.get<std::uint32_t>("geonameid");
        if (c.geonameid == 0) throw row.error("geonameid", "must be positive");
        c.name = row.get<std::string>("name");
        c.asciiname = row.get_or<std::string>("asciiname", "");
        c.latitude = row.get<double>("latitude");
        c.longitude = row.get<double>("longitude");
        if (c.latitude < -90.0 || c.latitude > 90.0)
        {
            throw row.error("latitude", "must be between -90 and 90");
        }
        if (c.longitude < -180.0 || c.longitude > 180.0)
        {
            throw row.error("longitude", "must be between -180 and 180");
        }
        c.feature_code = row.get_or<std::string>("feature_code", "");
        c.country_code = row.get<std::string>("country_code");
        c.cc2 = row.get_or<std::string>("cc2", "");
        c.admin1_code = row.get_or<std::string>("admin1_code", "");
        c.admin2_code = row.get_or<std::string>("admin2_code", "");
        c.admin3_code = row.get_or<std::string>("admin3_code", "");
        c.admin4_code = row.get_or<std::string>("admin4_code", "");
        c.population = row.get<std::uint64_t>("population");
        c.elevation = row.get_or<std::int16_t>("elevation", city::unknown_elevation);
        c.dem = row.get_or<std::int16_t>("dem", 0);
        c.timezone = row.get<std::string>("timezone");
        return c;
    }

    // Population weight; every city keeps a chance even with population 0.
    static double weight_(const city& c)
    {
        return static_cast<double>(std::max<std::uint64_t>(c.population, 1));
    }

    struct country_entry
    {
        std::vector<std::size_t> indices;
        core::discrete distribution{1.0};
    };

    core::layered_table _table{schema_()};
    std::vector<city> _cities; // in table row order
    core::discrete _distribution{1.0}; // replaced in rebuild_() when data is loaded
    std::map<std::string, country_entry, std::less<>> _country_index;
    std::map<std::string, std::string, std::less<>> _admin1_names;
    core::rng _engine{core::rng::from_entropy()};

    struct auto_probe_tag {};

    explicit cg(auto_probe_tag /*tag*/)
    {
        if (!load(dataset::lite)) (void)load(dataset::full);
    }

    static std::span<const core::embedded_file> embedded_files_()
    {
#ifdef DASMIG_CITY_EMBED
        return embedded::citygen::files;
#else
        return {};
#endif
    }

    void require_data_() const
    {
        if (_cities.empty())
        {
            throw std::runtime_error("No city data loaded. Call load() first.");
        }
    }

    [[nodiscard]] city result_(const city& c, std::uint64_t call_seed) const
    {
        city result = c;
        result._signature = {.seed = call_seed, .data = data_hash()};
        return result;
    }

    void rebuild_()
    {
        std::vector<city> cities;
        cities.reserve(_table.size());
        for (const auto& row : _table.rows()) cities.push_back(parse_(row));

        core::discrete distribution{1.0};
        std::map<std::string, country_entry, std::less<>> countries;
        if (!cities.empty())
        {
            std::vector<double> weights;
            weights.reserve(cities.size());
            std::map<std::string, std::vector<double>, std::less<>> country_weights;
            for (std::size_t i = 0; i < cities.size(); ++i)
            {
                const auto& c = cities[i];
                weights.push_back(weight_(c));
                countries[c.country_code].indices.push_back(i);
                country_weights[c.country_code].push_back(weight_(c));
            }
            distribution = core::discrete{weights};
            for (auto& [code, entry] : countries)
            {
                entry.distribution = core::discrete{country_weights[code]};
            }
        }
        _cities = std::move(cities);
        _distribution = std::move(distribution);
        _country_index = std::move(countries);
    }
};

} // namespace dasmig

#endif // DASMIG_CITYGEN_HPP
