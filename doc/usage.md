# Usage Guide

This guide covers every feature of the city-generator library in detail. For a quick overview, see the [README](../README.md). For the full API reference, run `doxygen Doxyfile` from the repository root and open `doc/api/html/index.html`.

[TOC]

<!-- GitHub-rendered TOC (Doxygen uses [TOC] above instead) -->
<ul>
<li><a href="#quick-start">Quick Start</a></li>
<li><a href="#installation">Installation</a></li>
<li><a href="#loading-resources">Loading Resources</a></li>
<li><a href="#generating-cities">Generating Cities</a></li>
<li><a href="#per-call-requests">Per-Call Requests</a></li>
<li><a href="#city-fields">City Fields</a></li>
<li><a href="#country-filtering">Country Filtering</a></li>
<li><a href="#population-weighting">Population Weighting</a></li>
<li><a href="#uniform-selection">Uniform Selection</a></li>
<li><a href="#seeding-and-deterministic-generation">Seeding and Deterministic Generation</a></li>
<li><a href="#multi-instance-support">Multi-Instance Support</a></li>
<li><a href="#data-pipeline">Data Pipeline</a></li>
<li><a href="#admin1-code-resolution">Admin1 Code Resolution</a></li>
<li><a href="#extending-data">Extending Data</a></li>
<li><a href="#replay-and-json">Replay and JSON</a></li>
<li><a href="#compiled-in-data">Compiled-in Data</a></li>
<li><a href="#migrating-from-v1">Migrating from v1</a></li>
<li><a href="#thread-safety">Thread Safety</a></li>
<li><a href="#error-reference">Error Reference</a></li>
</ul>

## Quick Start

```cpp
#include <dasmig/citygen.hpp>
#include <iostream>

int main()
{
    auto& gen = dasmig::cg::instance();

    // Random city (population-weighted).
    auto c = gen.get_city();
    std::cout << c.name << ", " << c.country_code << "\n";

    // City from a specific country.
    auto br = gen.get_city({.country = "BR"});
    std::cout << br.name << " (pop. " << br.population << ")\n";
}
```

## Installation

1. Copy `dasmig/citygen.hpp`, `dasmig/core.hpp` and the `dasmig/core/` directory into your include path, keeping them side by side.
2. Copy the `resources/` folder (containing `full/` and/or `lite/` subdirectories) so it is accessible at runtime.
3. Compile with C++23 enabled: `-std=c++23`.

## Loading Resources

The library ships two dataset tiers:

| Tier | Enum | Cities | Source |
|------|------|--------|--------|
| lite | `dasmig::dataset::lite` | 33,521 (pop ≥ 15,000) | `cities15000.zip` |
| full | `dasmig::dataset::full` | 231,224 (pop ≥ 500) | `cities500.zip` |

Resources are stored as `resources/lite/cities.tsv` and `resources/full/cities.tsv`.

### Automatic loading (singleton)

On first access the singleton loads the `lite` tier (or `full` if `lite` is missing). It uses the compiled-in copy when built with `DASMIG_CITY_EMBED` (see Compiled-in Data), and otherwise looks for `lite/cities.tsv` in these directories, relative to the working directory:

| Priority | Base path |
|----------|------|
| 1 | `resources/` |
| 2 | `../resources/` |
| 3 | `city-generator/resources/` |
| 4 | `../city-generator/resources/` |

### Explicit tier loading

```cpp
dasmig::cg gen;
gen.load(dasmig::dataset::lite);  // 33,521 major cities
gen.load(dasmig::dataset::full);  // 231,224 cities
```

If your resources are elsewhere, call `load()` with a direct path:

```cpp
dasmig::cg::instance().load("/opt/data/cities.tsv");
```

Each `load()` replaces the data loaded before (use `extend()` to add on top; see Extending Data). `load(path)` does nothing if the path is not a file; a malformed file throws `dasmig::core::tsv_error` with its file, line and column.

## Generating Cities

### Random (population-weighted)

```cpp
auto c = dasmig::cg::instance().get_city();
std::string text = c;  // implicit conversion — returns the city name
```

### Country-filtered

```cpp
auto c = dasmig::cg::instance().get_city({.country = "US"});
// c.country_code == "US"
```

## Per-Call Requests

`get_city(request)` and `get_city(request, seed)` take a `city_request` that
selects a country and weighting mode for that call. `get_city()` is the same
as a default request:

```cpp
auto& gen = dasmig::cg::instance();
const dasmig::city_request request{.country = "BR", .weighted = false};
auto c = gen.get_city(request, 42);  // uniform within Brazil
auto replay = gen.get_city(request, c.seed());
// replay.to_json() == c.to_json() with the same loaded data
```

| Field | Type | Default | Meaning |
|-------|------|---------|---------|
| `country` | `std::optional<std::string>` | `std::nullopt` | No country filter; set a code such as `"BR"` to filter |
| `weighted` | `bool` | `true` | Population-weighted selection; `false` gives every eligible city the same chance |

An explicit empty, unknown or differently cased country code throws
`std::invalid_argument` if no loaded cities match it. No data loaded throws
`std::runtime_error` first.

The seeded overload is available on a `const cg`. Neither overload retains
a country filter or weighting mode: the generator has no request defaults.
The result carries the same `city` fields, JSON and seed/data signature.
Keep the request alongside the seed for replay: the signature does not record
the country filter or weighting mode.

## City Fields

Every generated city includes all 16 GeoNames fields:

| Field | Type | Description |
|-------|------|-------------|
| `geonameid` | `uint32_t` | GeoNames primary key |
| `name` | `string` | UTF-8 city name |
| `asciiname` | `string` | ASCII transliteration |
| `latitude` | `double` | WGS84 decimal degrees |
| `longitude` | `double` | WGS84 decimal degrees |
| `feature_code` | `string` | PPL, PPLA, PPLC, etc. |
| `country_code` | `string` | ISO-3166 two-letter |
| `cc2` | `string` | Alternate country codes |
| `admin1_code` | `string` | State/province |
| `admin2_code` | `string` | County/district |
| `admin3_code` | `string` | Township/commune |
| `admin4_code` | `string` | Sub-district |
| `population` | `uint64_t` | City population |
| `elevation` | `int16_t` | Meters (-9999 if unknown) |
| `dem` | `int16_t` | Digital elevation model |
| `timezone` | `string` | IANA timezone ID |

```cpp
auto c = gen.get_city();
std::cout << c.name << " (" << c.country_code << ")\n";
std::cout << "Coordinates: " << c.latitude << ", " << c.longitude << "\n";
std::cout << "Population: " << c.population << "\n";
std::cout << "Timezone: " << c.timezone << "\n";
```

## Country Filtering

Pass a `city_request` with an ISO-3166 two-letter country code to restrict generation:

```cpp
auto jp = gen.get_city({.country = "JP"});  // Japanese city
auto de = gen.get_city({.country = "DE"});  // German city
```

Country-filtered selection is population-weighted within the country unless
the request sets `weighted = false`. Throws `std::invalid_argument` if no
cities match the code.

## Population Weighting

By default, cities are selected with probability proportional to their population.
A city with 1,000,000 inhabitants is ~1000× more likely to be selected
than one with 1,000. Cities with zero population are assigned a minimum
weight of 1, ensuring they can still appear.

## Uniform Selection

Set `weighted = false` in the request for equal-probability selection, where
every eligible city has the same chance regardless of population:

```cpp
auto& gen = dasmig::cg::instance();

auto c = gen.get_city({.weighted = false});                    // any city equally likely
auto j = gen.get_city({.country = "JP", .weighted = false});   // uniform within Japan
auto s = gen.get_city({.weighted = false}, 42);                // uniform + deterministic
```

## Seeding and Deterministic Generation

### Per-call seeding

Pass an explicit seed as the second argument to produce the same city every time:

```cpp
auto c = dasmig::cg::instance().get_city({}, 42);
// Always produces the same city for the same data + request + seed.
```

### Seed replay

Every city records its seed. Retrieve it with `city::seed()` and pass
it back with the same data and request to reproduce the exact same result:

```cpp
auto c = dasmig::cg::instance().get_city();
auto saved_seed = c.seed();

// Later, replay:
auto replay = dasmig::cg::instance().get_city({}, saved_seed);
// replay.geonameid == c.geonameid
```

### Sequence seeding

Seed the generator's engine for a reproducible sequence:

```cpp
auto& gen = dasmig::cg::instance();

gen.seed(100);
auto a = gen.get_city();
auto b = gen.get_city();

gen.seed(100);                // reset
auto a2 = gen.get_city();     // identical to a
auto b2 = gen.get_city();     // identical to b

gen.unseed(); // restore non-deterministic behavior
```

### Country filter with seed

```cpp
auto c = gen.get_city({.country = "BR"}, 42);
// Deterministic within Brazilian cities.
```

> **Note:** To replay a country-filtered city, pass the same request and
> the seed: `gen.get_city({.country = "BR"}, c.seed())`. Using just
> `c.seed()` with a default request will select from the full dataset and
> likely produce a different city.

## Multi-Instance Support

Construct independent generators with their own data and engine:

```cpp
dasmig::cg my_gen;
my_gen.load("path/to/cities.tsv");
auto c = my_gen.get_city();
```

## Data Pipeline

The shipped datasets are generated from GeoNames by `scripts/prepare_geonames.py`. [Data and Method](data.md) describes the source, every column and how selection works.

## Admin1 Code Resolution

Cities include a raw `admin1_code` field (e.g. `"CA"`, `"08"`) that maps
to a first-level administrative division (state, province, region). These
codes are opaque identifiers. To display human-readable names, load the
GeoNames `admin1CodesASCII.txt` mapping file:

```cpp
auto& gen = dasmig::cg::instance();
gen.load_admin1("resources/admin1CodesASCII.txt");
```

Then resolve codes for any city:

```cpp
auto c = gen.get_city({.country = "US"});
std::string state = gen.resolve_admin1(c.country_code, c.admin1_code);
// e.g. "California" instead of "CA"
```

If no mapping exists for a code, `resolve_admin1()` returns the raw code
as a fallback. An empty `admin1_code` returns an empty string.

Check whether mappings have been loaded:

```cpp
if (gen.has_admin1())
    std::cout << gen.resolve_admin1("US", "NY") << "\n"; // "New York"
```

The mapping file is tab-separated with four columns:
`CC.CODE \t Name \t ASCII_Name \t GeoNameID`. Download it from
[GeoNames](https://download.geonames.org/export/dump/admin1CodesASCII.txt).

## Extending Data

The shipped data is a base layer. Files with the same columns as `cities.tsv` can be loaded on top of it to add cities, change them or remove them, without editing the shipped file. Rows are keyed by `geonameid`.

```cpp
auto& gen = dasmig::cg::instance();

// Override mode (the default): a row replaces the loaded city with the same
// id; new ids are added.
gen.extend("mods/settlements.tsv");

// From text, for example supplied by a WebAssembly host or a game scenario.
gen.extend_from_string(text, "scenario: flooded coast");

// Remove cities: a layer needs only the key and a disabled column.
gen.extend_from_string("geonameid\tdisabled\n3448439\t1\n", "cleanup");

// Add mode refuses to touch cities that are already loaded.
gen.extend("mods/new_cities.tsv", dasmig::core::layer_mode::add);
```

- A layer must have the columns `geonameid`, `name`, `latitude`, `longitude`, `country_code`, `population` and `timezone`; the other columns are optional (empty text, unknown elevation, `dem` 0). Ids must be positive, numbers must fit their types and coordinates must be in range; all of this is checked before anything changes. A bad layer throws `dasmig::core::tsv_error` with the file, line and column, and the data stays as it was.
- New country codes become available to `get_city({.country = ...})` at once.
- `layers()` lists what is loaded: source, mode, rows added, replaced and removed, and warnings such as unknown columns.
- `load(...)` replaces the base and drops every layer.

## Replay and JSON

Every result records the seed and the data it came from:

```cpp
auto c = gen.get_city({.country = "BR"});
c.seed();                    // the call seed
c.signature().data;          // gen.data_hash() when it was generated

// Same seed, same data, same weighting: the same city on every compiler
// and platform.
auto again = gen.get_city({.country = "BR"}, c.seed());
```

`to_json()` gives the result as JSON, the form the WebAssembly layer and the game consume; an unknown elevation is `null`:

```json
{"geonameid":953781,"name":"Soweto","asciiname":"Soweto","latitude":-26.26781,"longitude":27.85849,
 "feature_code":"PPL","country_code":"ZA",...,"population":1695047,"elevation":null,"dem":1632,
 "timezone":"Africa/Johannesburg","signature":{"seed":"0000000000000001","data":"7e8af51013ede34a"}}
```

Seeds are strings because JavaScript numbers cannot hold every 64-bit value. `to_json(writer)` writes into an existing `dasmig::core::json_writer`.

## Compiled-in Data

Define `DASMIG_CITY_EMBED` before including the header to compile the `lite` tier into the program (a 4 MB header; clang compiles it in about 3 s, MSVC in about 7 s). `load(dataset::lite)` and `instance()` then use it and need no files, which is what a WebAssembly build wants. The compiled-in data has the same `data_hash()` as the file, so seeds replay the same either way. Admin1 names are not compiled in.

```cpp
#define DASMIG_CITY_EMBED
#include <dasmig/citygen.hpp>
```

`dasmig/citygen_data.hpp` is generated from `resources/lite` by `make embed`; CI checks it is up to date.

## Migrating from v1

Version 2 moves the library onto [dasmig-core](https://github.com/dasmig/dasmig-core).

- **Every seeded result changes.** Selection now uses the core's engine and weighted choice, so a seed gives the same city on every compiler, standard library and CPU, which the `std::` distributions could not promise. Seeds saved with v1 do not reproduce their v1 results. The selection probabilities are unchanged (checked against v1 over 20,000 draws per scenario).
- **`load(path)` replaces the data.** In v1 each call added to what was loaded. Use `extend()` to add data on top.
- **Malformed data is an error.** v1 skipped lines it could not parse (and parsed numbers with the C locale). v2 throws `dasmig::core::tsv_error` with the file, line and column and loads nothing; the same applies to `load_admin1()`. A path that does not exist is still ignored.
- **`load(dataset)` is `[[nodiscard]]`**, as in the other libraries.
- **Loading is slower**: the full tier takes about 1.3 s instead of 0.5 s, because every row is checked. The `lite` tier takes about 0.2 s.
- **Files:** replace `random.hpp` with `core.hpp` and the `core/` directory (see Installation).
- **New:** `extend()`, `extend_from_string()`, `layers()`, `data_hash()`, `city::signature()`, `city::to_json()`, `DASMIG_CITY_EMBED`.

## Thread Safety

Each `cg` instance is independent. The static `instance()` singleton uses a
local static for safe initialization.

| Operation | Thread-safe? |
|-----------|-------------|
| `instance()` | Yes (static local) |
| `get_city()` on **different** instances | Yes |
| `get_city()` on the **same** instance | **No** — requires external synchronization |
| `load()`, `extend()`, `load_admin1()` | **No** — must not be called concurrently with `get_city()` on the same instance |

Call `load()` once during initialization before spawning threads. For
concurrent generation, give each thread its own `cg` instance.

## Error Reference

| Exception | Condition |
|-----------|-----------|
| `std::runtime_error` | `get_city()` called with no data loaded |
| `std::invalid_argument` | `get_city(request)` or `get_city(request, seed)` with a country code that matches no loaded cities (including an explicit empty code) |
| `dasmig::core::tsv_error` | A malformed `cities.tsv`, layer or admin1 file (message gives file, line and column); the data is unchanged |
