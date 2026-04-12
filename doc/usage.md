# Usage Guide

This guide covers every feature of the city-generator library in detail. For a quick overview, see the [README](../README.md). For the full API reference, run `doxygen Doxyfile` from the repository root and open `doc/api/html/index.html`.

[TOC]

<!-- GitHub-rendered TOC (Doxygen uses [TOC] above instead) -->
<ul>
<li><a href="#quick-start">Quick Start</a></li>
<li><a href="#installation">Installation</a></li>
<li><a href="#loading-resources">Loading Resources</a></li>
<li><a href="#generating-cities">Generating Cities</a></li>
<li><a href="#city-fields">City Fields</a></li>
<li><a href="#country-filtering">Country Filtering</a></li>
<li><a href="#population-weighting">Population Weighting</a></li>
<li><a href="#uniform-selection">Uniform Selection</a></li>
<li><a href="#seeding-and-deterministic-generation">Seeding and Deterministic Generation</a></li>
<li><a href="#multi-instance-support">Multi-Instance Support</a></li>
<li><a href="#data-pipeline">Data Pipeline</a></li>
<li><a href="#admin1-code-resolution">Admin1 Code Resolution</a></li>
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
    auto br = gen.get_city("BR");
    std::cout << br.name << " (pop. " << br.population << ")\n";
}
```

## Installation

1. Copy `dasmig/citygen.hpp` and `dasmig/random.hpp` into your include path.
2. Copy the `resources/` folder (containing `full/` and/or `lite/` subdirectories) so it is accessible at runtime.
3. Compile with C++23 enabled: `-std=c++23`.

## Loading Resources

The library ships two dataset tiers:

| Tier | Enum | Cities | Source |
|------|------|--------|--------|
| lite | `dasmig::dataset::lite` | ~25k (pop ≥ 15,000) | `cities15000.zip` |
| full | `dasmig::dataset::full` | ~200k (pop ≥ 500) | `cities500.zip` |

Resources are stored as `resources/lite/cities.tsv` and `resources/full/cities.tsv`.

### Automatic loading (singleton)

On first access the singleton constructor probes these base paths:

| Priority | Base path |
|----------|------|
| 1 | `resources/` |
| 2 | `../resources/` |
| 3 | `city-generator/resources/` |

It loads `lite/cities.tsv` if found, otherwise falls back to `full/cities.tsv`.

### Explicit tier loading

```cpp
dasmig::cg gen;
gen.load(dasmig::dataset::lite);  // ~25k major cities
gen.load(dasmig::dataset::full);  // ~200k cities (can combine)
```

If your resources are elsewhere, call `load()` with a direct path:

```cpp
dasmig::cg::instance().load("/opt/data/cities.tsv");
```

Calling `load()` multiple times is safe — each call adds to the existing data.

## Generating Cities

### Random (population-weighted)

```cpp
auto c = dasmig::cg::instance().get_city();
std::string text = c;  // implicit conversion — returns the city name
```

### Country-filtered

```cpp
auto c = dasmig::cg::instance().get_city("US");
// c.country_code == "US"
```

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

Pass an ISO-3166 two-letter country code to restrict generation:

```cpp
auto jp = gen.get_city("JP");  // Japanese city
auto de = gen.get_city("DE");  // German city
```

Country-filtered selection is also population-weighted within the country.
Throws `std::invalid_argument` if no cities match the code.

## Population Weighting

By default, cities are selected with probability proportional to their population.
A city with 1,000,000 inhabitants is ~1000× more likely to be selected
than one with 1,000. Cities with zero population are assigned a minimum
weight of 1, ensuring they can still appear.

## Uniform Selection

Switch to equal-probability selection where every loaded city has the
same chance of being picked, regardless of population:

```cpp
auto& gen = dasmig::cg::instance();

gen.weighted(false);           // uniform random
auto c = gen.get_city();       // any city equally likely
auto j = gen.get_city("JP");   // uniform within Japan

gen.weighted(true);            // restore population-weighted (default)
```

The `weighted()` setter returns `*this` for chaining:

```cpp
gen.weighted(false).seed(42);  // uniform + deterministic
```

Query the current mode with:

```cpp
bool is_weighted = gen.weighted(); // true by default
```

## Seeding and Deterministic Generation

### Per-call seeding

Pass an explicit seed to `get_city()` to produce the same city every time:

```cpp
auto c = dasmig::cg::instance().get_city(42);
// Always produces the same city for the same data + seed.
```

### Seed replay

Every city records its seed. Retrieve it with `city::seed()` and pass
it back to reproduce the exact same result:

```cpp
auto c = dasmig::cg::instance().get_city();
auto saved_seed = c.seed();

// Later, replay:
auto replay = dasmig::cg::instance().get_city(saved_seed);
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
auto c = gen.get_city("BR", 42);
// Deterministic within Brazilian cities.
```

> **Note:** To replay a country-filtered city, pass both the country code
> and the seed: `gen.get_city("BR", c.seed())`. Using just `c.seed()`
> without the country filter will select from the full dataset and likely
> produce a different city.

## Multi-Instance Support

Construct independent generators with their own data and engine:

```cpp
dasmig::cg my_gen;
my_gen.load("path/to/cities.tsv");
auto c = my_gen.get_city();
```

## Data Pipeline

The shipped datasets are generated from GeoNames using the included Python script:

```bash
python scripts/prepare_geonames.py            # generate both tiers
python scripts/prepare_geonames.py --tier full # full only (~200k cities)
python scripts/prepare_geonames.py --tier lite # lite only (~25k cities)
```

This produces `resources/full/cities.tsv` and `resources/lite/cities.tsv`.

The GeoNames data is licensed under CC BY 4.0. See `LICENSE_DATA.txt`.

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
auto c = gen.get_city("US");
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

## Thread Safety

Each `cg` instance is independent. The static `instance()` singleton uses a
local static for safe initialization.

| Operation | Thread-safe? |
|-----------|-------------|
| `instance()` | Yes (static local) |
| `get_city()` on **different** instances | Yes |
| `get_city()` on the **same** instance | **No** — requires external synchronization |
| `load()` | **No** — must not be called concurrently with `get_city()` on the same instance |

Call `load()` once during initialization before spawning threads. For
concurrent generation, give each thread its own `cg` instance.

## Error Reference

| Exception | Condition |
|-----------|-----------|
| `std::runtime_error` | `get_city()` called with no data loaded |
| `std::invalid_argument` | `get_city(country)` with unknown country code |
