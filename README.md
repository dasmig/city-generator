# City Generator for C++

> **Requires C++23** (e.g., `-std=c++23` for GCC/Clang, `/std:c++latest` for MSVC).

[![City Generator for C++](https://raw.githubusercontent.com/dasmig/city-generator/master/doc/city-generator.png)](https://github.com/dasmig/city-generator/releases)

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/dasmig/city-generator/master/LICENSE.MIT)
[![CI](https://github.com/dasmig/city-generator/actions/workflows/ci.yml/badge.svg)](https://github.com/dasmig/city-generator/actions/workflows/ci.yml)
[![GitHub Releases](https://img.shields.io/github/release/dasmig/city-generator.svg)](https://github.com/dasmig/city-generator/releases)
[![C++23](https://img.shields.io/badge/standard-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Header-only](https://img.shields.io/badge/type-header--only-green.svg)](https://github.com/dasmig/city-generator#integration)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue.svg)](https://dasmig.github.io/city-generator/)

**[API Reference](https://dasmig.github.io/city-generator/)** · **[Usage Guide](doc/usage.md)** · **[Data and Method](doc/data.md)** · **[Releases](https://github.com/dasmig/city-generator/releases)**

Cities drawn by population, with coordinates, country, timezone and other GeoNames fields.

## Features

- **Population-Weighted Generation**. Cities are selected with probability proportional to their population, mirroring real-world demographic distributions.

- **Rich City Data**. Every generated city includes 16 GeoNames fields: name, coordinates, country, administrative divisions, population, elevation, timezone, and more.

- **Country Filtering**. Generate cities from a specific country with `get_city({.country = "BR"})`.

- **GeoNames Data**. Ships with two dataset tiers — *full* (231,224 cities, pop ≥ 500) and *lite* (33,521 major cities, pop ≥ 15,000) — from GeoNames, licensed under CC BY 4.0.

- **Request-First API**. `get_city(request)` and `get_city(request, seed)` take a `city_request` with an optional country filter and a population-weighted (default) or uniform selection mode. There are no generator-level defaults.

- **Admin1 Code Resolution**. Translate raw admin1 codes (e.g. `"CA"`, `"08"`) into human-readable state/province names using the GeoNames `admin1CodesASCII.txt` mapping.

- **Nearest City**. `find_nearest({.latitude, .longitude})` maps a point to the closest loaded city, optionally within a country or above a population.

- **Deterministic Seeding**. Per-call `get_city(request, seed)` for reproducible results, and `city::seed()` for replaying a previous generation.

- **Batch Generation**. Generate thousands of results from one seed, on several threads with the same output for any thread count, optionally unique by a key, and stream them as JSON Lines or CSV (see the usage guide).

- **Identical Everywhere**. A seed gives the same city on every compiler, standard library and CPU (GCC, Clang, MSVC, Emscripten; x86-64 and ARM64), checked by known-answer tests.

- **Extensible Data**. Load your own TSV layers on top of the shipped data to add, change or remove cities (`extend()`), checked before they are applied.

- **JSON and Replay**. `to_json()` on every result, and a `signature()` recording the seed and the data it came from.

- **Compiled-in Data**. Define `DASMIG_CITY_EMBED` to build the `lite` tier into the program, for WebAssembly and other builds without files.

- **Multi-Instance Support**. Construct independent `cg` instances with their own data and random engine — ideal for embedding inside other generators.

## Integration

[`citygen.hpp`](https://github.com/dasmig/city-generator/blob/master/dasmig/citygen.hpp) is the library [released here](https://github.com/dasmig/city-generator/releases). Keep [`core.hpp`](https://github.com/dasmig/city-generator/blob/master/dasmig/core.hpp) and the [`core/`](https://github.com/dasmig/city-generator/tree/master/dasmig/core) directory ([dasmig-core](https://github.com/dasmig/dasmig-core)) in the same directory. Add

```cpp
#include <dasmig/citygen.hpp>
```

to the files you want to use it in, and supply the [`resources`](https://github.com/dasmig/city-generator/tree/master/resources) folder (or define `DASMIG_CITY_EMBED`).

## Usage

```cpp
#include <dasmig/citygen.hpp>
#include <iostream>

// For convenience.
using cg = dasmig::cg;

// Manually load a specific dataset tier if necessary.
cg::instance().load(dasmig::dataset::lite);  // 33,521 major cities
// OR
cg::instance().load(dasmig::dataset::full);  // 231,224 cities

// Generate a random city (population-weighted).
auto city = cg::instance().get_city({});
std::cout << city.name << ", " << city.country_code
          << " (pop. " << city.population << ")" << std::endl;

// Generate a city from a specific country.
auto br_city = cg::instance().get_city({.country = "BR"});

// Access all available fields.
std::cout << city.latitude << ", " << city.longitude << std::endl;
std::cout << city.timezone << std::endl;
std::cout << city.elevation << " m" << std::endl;

// Deterministic generation — same request and seed always produce the same city.
auto seeded = cg::instance().get_city({}, 42);

// Replay a previous city using its seed.
auto replay = cg::instance().get_city({}, seeded.seed());

// Uniform selection within Brazil.
const dasmig::city_request request{.country = "BR", .weighted = false};
auto requested = cg::instance().get_city(request, 42);
auto requested_replay = cg::instance().get_city(request, requested.seed());

// A default request selects globally with population weighting.
// Replay requires the same request and loaded data as well as the seed.

// A deterministic sequence: keep a dasmig::core::rng and pass its next value.
dasmig::core::rng rng{100};
auto first = cg::instance().get_city({}, rng());
auto second = cg::instance().get_city({}, rng());

// Uniform selection: any city equally likely.
auto uniform_city = cg::instance().get_city({.weighted = false});

// Independent instance — separate data and random engine.
cg my_gen;
my_gen.load("path/to/cities.tsv");
auto c = my_gen.get_city({});
```

Generation always takes a request: `get_city(request)` or `get_city(request, seed)`, with `{}` for the defaults. The generator has no `seed()` or `unseed()`; see [Migrating from v3](doc/usage.md#migrating-from-v3).

For the complete feature guide — fields, filtering, weighting, and more — see the **[Usage Guide](doc/usage.md)**.

## Data

The city data is sourced from the [GeoNames](https://www.geonames.org/) geographical database. Two tiers are provided:

| Tier | Source | Cities |
|------|--------|--------|
| full | `cities500.zip` | 231,224 (pop ≥ 500) |
| lite | `cities15000.zip` | 33,521 (pop ≥ 15,000) |

The data is licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). See `LICENSE_DATA.txt` for details.

To regenerate datasets:

```bash
python scripts/prepare_geonames.py            # generate both tiers
python scripts/prepare_geonames.py --tier lite # lite only
python scripts/prepare_geonames.py --tier full # full only
```

## Building

```bash
make            # example
make test       # tests
make bench      # benchmarks, results in build/bench.tsv
make test-embed # tests with the lite tier compiled in
make coverage   # code coverage
make lint       # clang-tidy
make docs       # API docs
make embed      # regenerate dasmig/citygen_data.hpp from resources/lite
```

## Compiler Support

Tested with:
- Clang 18+ (`-std=c++23`)
- GCC 14+ (`-std=c++23`)
- MSVC 19.38+ (`/std:c++latest`)
- Emscripten 3.1.74

## Dependencies

| Dependency | Version | Bundled | Purpose |
|-----------|---------|---------|---------|
| [dasmig-core](https://github.com/dasmig/dasmig-core) | 1.4 | Yes (`core.hpp`, `core/`) | Platform-independent random engine and distributions, data layers, JSON |
| [Catch2](https://github.com/catchorg/Catch2) | 3.x | Yes (amalgamated) | Unit testing |

## Related Libraries

| Library | Description |
|---------|-------------|
| [address-generator](https://github.com/dasmig/address-generator) | Postal addresses with real streets by place |
| [biodata-generator](https://github.com/dasmig/biodata-generator) | Procedural human physical characteristics |
| [birth-generator](https://github.com/dasmig/birth-generator) | Demographically plausible birthdays |
| [country-generator](https://github.com/dasmig/country-generator) | Weighted country selection by population |
| [device-generator](https://github.com/dasmig/device-generator) | Devices, browsers, user agents and IP addresses that fit together |
| [email-generator](https://github.com/dasmig/email-generator) | Email addresses in each country's style |
| [entity-generator](https://github.com/dasmig/entity-generator) | ECS-based entity generation |
| [interest-generator](https://github.com/dasmig/interest-generator) | Religion, hobbies and favourite media by country, age and sex |
| [name-generator](https://github.com/dasmig/name-generator) | Culturally appropriate full names |
| [nickname-generator](https://github.com/dasmig/nickname-generator) | Gamer-style nicknames |
| [occupation-generator](https://github.com/dasmig/occupation-generator) | Employment status, education and occupation by country, age and sex |
| [persona-generator](https://github.com/dasmig/persona-generator) | Complete characters composed from the other generators |
| [phone-generator](https://github.com/dasmig/phone-generator) | Phone numbers valid in each country's numbering plan |
| [relationship-generator](https://github.com/dasmig/relationship-generator) | Families around a person: parents, siblings, partners and children |
| [trait-generator](https://github.com/dasmig/trait-generator) | Personality traits with conflicts, synergies and Big Five axes |
| [user-generator](https://github.com/dasmig/user-generator) | Complete user records: account, username, email, phone and address |

## License

This library is released under the [MIT License](https://opensource.org/licenses/MIT).

```
MIT License

Copyright (c) 2020-2026 Diego Dasso Migotto
```
